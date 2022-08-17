// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck

#ifndef JUGGLER_JUGRECO_TrackFindingAlgorithm_HH
#define JUGGLER_JUGRECO_TrackFindingAlgorithm_HH

#include <functional>
#include <random>
#include <stdexcept>
#include <vector>

#include "Jug/Property.h"
#include "JugAlg/JugAlgorithm.h"
#include "JugKernel/ToolHandle.h"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"
#include "JugBase/BField/DD4hepBField.h"
#include "JugTrack/GeometryContainers.hpp"
#include "JugTrack/Index.hpp"
#include "JugTrack/IndexSourceLink.hpp"
#include "JugTrack/Measurement.hpp"
#include "JugTrack/Track.hpp"
#include "JugTrack/Trajectories.hpp"

#include "eicd/TrackerHitCollection.h"

#include "Acts/Definitions/Common.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/TrackFinding/CombinatorialKalmanFilter.hpp"
#include "Acts/TrackFinding/MeasurementSelector.hpp"

namespace Jug::Reco {

/** Fitting algorithm implmentation .
 *
 * \ingroup tracking
 */
class TrackFindingAlgorithm : public JugAlgorithm {
public:
  /// Track finder function that takes input measurements, initial trackstate
  /// and track finder options and returns some track-finder-specific result.
  using TrackFinderOptions  = Acts::CombinatorialKalmanFilterOptions<IndexSourceLinkAccessor::Iterator>;
  using TrackFinderResult   = std::vector<Acts::Result<Acts::CombinatorialKalmanFilterResult>>;

  /// Find function that takes the above parameters
  /// @note This is separated into a virtual interface to keep compilation units
  /// small
  class TrackFinderFunction {
   public:
    virtual ~TrackFinderFunction() = default;
    virtual TrackFinderResult operator()(const TrackParametersContainer&,
                                         const TrackFinderOptions&) const = 0;
  };

  /// Create the track finder function implementation.
  /// The magnetic field is intentionally given by-value since the variant
  /// contains shared_ptr anyways.
  static std::shared_ptr<TrackFinderFunction> makeTrackFinderFunction(
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    std::shared_ptr<const Acts::MagneticFieldProvider> magneticField);

public:
  DataHandle<IndexSourceLinkContainer> m_inputSourceLinks{"inputSourceLinks", Jug::DataHandle::Reader, this};
  DataHandle<MeasurementContainer> m_inputMeasurements{"inputMeasurements", Jug::DataHandle::Reader, this};
  DataHandle<TrackParametersContainer> m_inputInitialTrackParameters{"inputInitialTrackParameters",
                                                                     Jug::DataHandle::Reader, this};
  DataHandle<TrajectoriesContainer> m_outputTrajectories{"outputTrajectories", Jug::DataHandle::Writer, this};

  Jug::Property<std::vector<double>> m_etaBins{this, "etaBins", {}};
  Jug::Property<std::vector<double>> m_chi2CutOff{this, "chi2CutOff", {15.}};
  Jug::Property<std::vector<size_t>> m_numMeasurementsCutOff{this, "numMeasurementsCutOff", {10}};

  std::shared_ptr<TrackFinderFunction> m_trackFinderFunc;
  SmartIF<IGeoSvc> m_geoSvc;

  std::shared_ptr<const Jug::BField::DD4hepBField> m_BField = nullptr;
  Acts::GeometryContext m_geoctx;
  Acts::CalibrationContext m_calibctx;
  Acts::MagneticFieldContext m_fieldctx;

  Acts::MeasurementSelector::Config m_sourcelinkSelectorCfg;
  Acts::Logging::Level m_actsLoggingLevel = Acts::Logging::INFO;

  TrackFindingAlgorithm(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;

  StatusCode execute() override;
};

} // namespace Jug::Reco

#endif
