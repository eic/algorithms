// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Chao Peng, Whitney Armstrong, Sylvester Joosten

/*  Clustering Algorithm for Ring Imaging Cherenkov (RICH) events
 *
 *  Author: Chao Peng (ANL)
 *  Date: 10/04/2020
 *
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
#include "FuzzyKClusters.h"
#include "eicd/PMTHitCollection.h"
#include "eicd/RingImageCollection.h"

using namespace Jug::Units;
using namespace Eigen;

namespace Jug::Reco {

/**  Clustering Algorithm for Ring Imaging Cherenkov (RICH) events.
 *
 * \ingroup reco
 */
class PhotoRingClusters : public JugAlgorithm {
private:
  DataHandle<eicd::PMTHitCollection> m_inputHitCollection{"inputHitCollection", Jug::DataHandle::Reader, this};
  DataHandle<eicd::RingImageCollection> m_outputClusterCollection{"outputClusterCollection", Jug::DataHandle::Writer,
                                                                  this};
  // @TODO
  // A more realistic way is to have tracker info as the input to determine how much clusters should be found
  Jug::Property<int> m_nRings{this, "nRings", 1};
  Jug::Property<int> m_nIters{this, "nIters", 1000};
  Jug::Property<double> m_q{this, "q", 2.0};
  Jug::Property<double> m_eps{this, "epsilon", 1e-4};
  Jug::Property<double> m_minNpe{this, "minNpe", 0.5};
  // Pointer to the geometry service
  SmartIF<IGeoSvc> m_geoSvc;

public:
  // ill-formed: using JugAlgorithm::JugAlgorithm;
  PhotoRingClusters(const std::string& name, ISvcLocator* svcLoc) : JugAlgorithm(name, svcLoc) {
    declareProperty("inputHitCollection", m_inputHitCollection, "");
    declareProperty("outputClusterCollection", m_outputClusterCollection, "");
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
    auto& clusters = *m_outputClusterCollection.createAndPut();

    // algorithm
    auto alg = fkc::KRings();

    // fill data
    MatrixXd data(rawhits.size(), 2);
    for (int i = 0; i < data.rows(); ++i) {
      if (rawhits[i].getNpe() > m_minNpe) {
        data.row(i) << rawhits[i].getLocal().x, rawhits[i].getLocal().y;
      }
    }

    // clustering
    auto res = alg.Fit(data, m_nRings, m_q, m_eps, m_nIters);

    // local position
    // @TODO: Many fields in RingImage not filled, need to assess
    //        if those are in fact needed
    for (int i = 0; i < res.rows(); ++i) {
      auto cl = clusters.create();
      cl.setPosition({static_cast<float>(res(i, 0)), static_cast<float>(res(i, 1)), 0});
      // @TODO: positionError() not set
      // @TODO: theta() not set
      // @TODO: thetaError() not set
      cl.setRadius(res(i, 2));
      // @TODO: radiusError not set
    }

    return StatusCode::SUCCESS;
  }
};


} // namespace Jug::Reco
