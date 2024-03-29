// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck

#include "CKFTracking.h"

// Gaudi
#include "GaudiAlg/GaudiAlgorithm.h"
#include "GaudiKernel/ToolHandle.h"
#include "GaudiAlg/Transformer.h"
#include "GaudiAlg/GaudiTool.h"
#include "GaudiKernel/RndmGenerators.h"
#include "Gaudi/Property.h"

#include "DDRec/CellIDPositionConverter.h"
#include "DDRec/SurfaceManager.h"
#include "DDRec/Surface.h"

#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/Plugins/DD4hep/DD4hepDetectorElement.hpp"
#include "Acts/Surfaces/PerigeeSurface.hpp"

#include "Acts/TrackFitting/GainMatrixSmoother.hpp"
#include "Acts/TrackFitting/GainMatrixUpdater.hpp"
#include "Acts/Propagator/EigenStepper.hpp"
#include "Acts/Propagator/Navigator.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Definitions/Common.hpp"
#include "Acts/Utilities/Helpers.hpp"
#include "Acts/Utilities/Logger.hpp"
#include "Acts/Definitions/Units.hpp"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"
#include "JugBase/BField/DD4hepBField.h"

#include "JugTrack/GeometryContainers.hpp"
#include "JugTrack/Measurement.hpp"
#include "JugTrack/Index.hpp"
#include "JugTrack/IndexSourceLink.hpp"
#include "JugTrack/Track.hpp"


#include "edm4eic/TrackerHitCollection.h"

#include <functional>
#include <stdexcept>
#include <vector>
#include <random>
#include <stdexcept>


static const std::map<int, Acts::Logging::Level> s_msgMap = {
    {MSG::DEBUG, Acts::Logging::DEBUG},
    {MSG::VERBOSE, Acts::Logging::VERBOSE},
    {MSG::INFO, Acts::Logging::INFO},
    {MSG::WARNING, Acts::Logging::WARNING},
    {MSG::FATAL, Acts::Logging::FATAL},
    {MSG::ERROR, Acts::Logging::ERROR},
};

namespace Jug::Reco {

  using namespace Acts::UnitLiterals;

  CKFTracking::CKFTracking(const std::string& name, ISvcLocator* svcLoc)
      : GaudiAlgorithm(name, svcLoc)
  {
    declareProperty("inputSourceLinks", m_inputSourceLinks, "");
    declareProperty("inputMeasurements", m_inputMeasurements, "");
    declareProperty("inputInitialTrackParameters", m_inputInitialTrackParameters, "");
    declareProperty("outputTrajectories", m_outputTrajectories, "");
  }

  StatusCode CKFTracking::initialize()
  {
    if (GaudiAlgorithm::initialize().isFailure()) {
      return StatusCode::FAILURE;
    }
    m_geoSvc = service("GeoSvc");
    if (!m_geoSvc) {
      error() << "Unable to locate Geometry Service. "
              << "Make sure you have GeoSvc and SimSvc in the right order in the configuration." << endmsg;
      return StatusCode::FAILURE;
    }

    m_BField   = std::dynamic_pointer_cast<const Jug::BField::DD4hepBField>(m_geoSvc->getFieldProvider());
    m_fieldctx = Jug::BField::BFieldVariant(m_BField);

    // eta bins, chi2 and #sourclinks per surface cutoffs
    m_sourcelinkSelectorCfg = {
        {Acts::GeometryIdentifier(),
            {m_etaBins, m_chi2CutOff,
                {m_numMeasurementsCutOff.begin(), m_numMeasurementsCutOff.end()}
            }
        },
    };
    m_trackFinderFunc = CKFTracking::makeCKFTrackingFunction(m_geoSvc->trackingGeometry(), m_BField);
    auto im = s_msgMap.find(msgLevel());
    if (im != s_msgMap.end()) {
        m_actsLoggingLevel = im->second;
    }
    return StatusCode::SUCCESS;
  }

  StatusCode CKFTracking::execute()
  {
    // Read input data
    const auto* const src_links       = m_inputSourceLinks.get();
    const auto* const init_trk_params = m_inputInitialTrackParameters.get();
    const auto* const measurements    = m_inputMeasurements.get();

    //// Prepare the output data with MultiTrajectory
    // TrajectoryContainer trajectories;
    auto* trajectories = m_outputTrajectories.createAndPut();
    trajectories->reserve(init_trk_params->size());

    //// Construct a perigee surface as the target surface
    auto pSurface = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3{0., 0., 0.});

    ACTS_LOCAL_LOGGER(Acts::getDefaultLogger("CKFTracking Logger", m_actsLoggingLevel));

    Acts::PropagatorPlainOptions pOptions;
    pOptions.maxSteps = 10000;

    MeasurementCalibrator calibrator{*measurements};
    Acts::GainMatrixUpdater kfUpdater;
    Acts::GainMatrixSmoother kfSmoother;
    Acts::MeasurementSelector measSel{m_sourcelinkSelectorCfg};

    Acts::CombinatorialKalmanFilterExtensions<Acts::VectorMultiTrajectory>
        extensions;
    extensions.calibrator.connect<&MeasurementCalibrator::calibrate>(&calibrator);
    extensions.updater.connect<
        &Acts::GainMatrixUpdater::operator()<Acts::VectorMultiTrajectory>>(
        &kfUpdater);
    extensions.smoother.connect<
        &Acts::GainMatrixSmoother::operator()<Acts::VectorMultiTrajectory>>(
        &kfSmoother);
    extensions.measurementSelector
        .connect<&Acts::MeasurementSelector::select<Acts::VectorMultiTrajectory>>(
            &measSel);

    IndexSourceLinkAccessor slAccessor;
    slAccessor.container = src_links;
    Acts::SourceLinkAccessorDelegate<IndexSourceLinkAccessor::Iterator>
        slAccessorDelegate;
    slAccessorDelegate.connect<&IndexSourceLinkAccessor::range>(&slAccessor);

    // Set the CombinatorialKalmanFilter options
    CKFTracking::TrackFinderOptions options(
        m_geoctx, m_fieldctx, m_calibctx, slAccessorDelegate,
        extensions, Acts::LoggerWrapper{logger()}, pOptions, &(*pSurface));

    auto results = (*m_trackFinderFunc)(*init_trk_params, options);

    for (std::size_t iseed = 0; iseed < init_trk_params->size(); ++iseed) {

      auto& result = results[iseed];

      if (result.ok()) {
        // Get the track finding output object
        auto& trackFindingOutput = result.value();
        // Create a SimMultiTrajectory
        trajectories->emplace_back(std::move(trackFindingOutput.fittedStates), 
                                   std::move(trackFindingOutput.lastMeasurementIndices),
                                   std::move(trackFindingOutput.fittedParameters));
      } else {
        if (msgLevel(MSG::DEBUG)) {
          debug() << "Track finding failed for truth seed " << iseed << "with error: " << result.error() << endmsg;
        }
      }
    }

    return StatusCode::SUCCESS;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  DECLARE_COMPONENT(CKFTracking)
} // namespace Jug::Reco
