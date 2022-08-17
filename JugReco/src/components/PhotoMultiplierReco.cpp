// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Chao Peng, Sylvester Joosten, Whitney Armstrong, Chao Peng

/*  General PhotoMultiplier Reconstruction
 *
 *  Estimate the number of photo-electrons and convert getTimeStamp to time
 *  Collect cell information
 *
 *  Author: Chao Peng (ANL)
 *  Date: 10/03/2020
 */

#include <algorithm>

#include "Jug/Property.h"
#include "JugAlg/JugAlgorithm.h"
#include "JugAlg/JugTool.h"
#include "JugAlg/Transformer.h"
#include "JugKernel/PhysicalConstants.h"
#include "JugKernel/RndmGenerators.h"
#include "JugKernel/ToolHandle.h"

#include "DDRec/CellIDPositionConverter.h"
#include "DDRec/Surface.h"
#include "DDRec/SurfaceManager.h"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"

// Event Model related classes
#include "eicd/PMTHitCollection.h"
#include "eicd/RawPMTHitCollection.h"

using namespace Jug::Units;

namespace Jug::Reco {

/**  General PhotoMultiplier Reconstruction
 *
 *  Estimate the number of photo-electrons and convert getTimeStamp to time
 *  Collect cell information
 *
 * \ingroup reco
 */
class PhotoMultiplierReco : public JugAlgorithm {
private:
  DataHandle<eicd::RawPMTHitCollection> m_inputHitCollection{"inputHitCollection", Jug::DataHandle::Reader, this};
  DataHandle<eicd::PMTHitCollection> m_outputHitCollection{"outputHitCollection", Jug::DataHandle::Writer, this};
  Jug::Property<double> m_timeStep{this, "timeStep", 0.0625 * ns};
  Jug::Property<double> m_minNpe{this, "minNpe", 0.0};
  Jug::Property<double> m_speMean{this, "speMean", 80.0};
  Jug::Property<double> m_pedMean{this, "pedMean", 200.0};
  /// Pointer to the geometry service
  SmartIF<IGeoSvc> m_geoSvc;

public:
  // ill-formed: using JugAlgorithm::JugAlgorithm;
  PhotoMultiplierReco(const std::string& name, ISvcLocator* svcLoc) : JugAlgorithm(name, svcLoc) {
    declareProperty("inputHitCollection", m_inputHitCollection, "");
    declareProperty("outputHitCollection", m_outputHitCollection, "");
  }

  StatusCode initialize() override {
    if (JugAlgorithm::initialize().isFailure()) {
      return StatusCode::FAILURE;
    }
    m_geoSvc = service("GeoSvc");
    if (!m_geoSvc) {
      error() << "Unable to locate Geometry Service. "
              << "Make sure you have GeoSvc and SimSvc in the right order in the configuration." << endmsg;
      return StatusCode::FAILURE;
    }
    return StatusCode::SUCCESS;
  }

  StatusCode execute() override {
    // input collections
    const auto& rawhits = *m_inputHitCollection.get();
    // Create output collections
    auto& hits = *m_outputHitCollection.createAndPut();

    // reconstruct number of photo-electrons and time
    for (const auto& rh : rawhits) {
      float npe = (rh.getIntegral() - m_pedMean) / m_speMean;
      if (npe >= m_minNpe) {
        float time = rh.getTimeStamp() * (static_cast<float>(m_timeStep) / ns);
        auto id    = rh.getCellID();
        // global positions
        auto gpos = m_geoSvc->cellIDPositionConverter()->position(id);
        // local positions
        auto pos = m_geoSvc->cellIDPositionConverter()->findContext(id)->volumePlacement().position();
        // cell dimension
        auto dim = m_geoSvc->cellIDPositionConverter()->cellDimensions(id);
        hits.push_back(eicd::PMTHit{
            rh.getCellID(),
            npe,
            time,
            static_cast<float>(m_timeStep / ns),
            {static_cast<float>(gpos.x()), static_cast<float>(gpos.y()), static_cast<float>(gpos.z())},
            {static_cast<float>(dim[0] / mm), static_cast<float>(dim[1] / mm), static_cast<float>(dim[2] / mm)},
            0, // @FIXME: Add sector similar to CalorimeterHit
            {static_cast<float>(pos.x()), static_cast<float>(pos.y()), static_cast<float>(pos.z())}});
      }
    }

    return StatusCode::SUCCESS;
  }
};


} // namespace Jug::Reco
