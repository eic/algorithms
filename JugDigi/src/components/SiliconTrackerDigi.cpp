// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck, Sylvester Joosten

#include <algorithm>
#include <cmath>

#include "Jug/Property.h"
#include "JugAlg/JugTool.h"
#include "JugAlg/Transformer.h"
#include "JugKernel/PhysicalConstants.h"
#include "JugKernel/RndmGenerators.h"

#include "JugBase/DataHandle.h"

// Event Model related classes
// edm4hep's tracker hit is the input collectiopn
#include "edm4hep/MCParticle.h"
#include "edm4hep/SimTrackerHitCollection.h"
// eicd's RawTrackerHit is the output
#include "eicd/RawTrackerHitCollection.h"

namespace Jug::Digi {

/** Silicon detector digitization.
 *
 * \ingroup digi
 */
class SiliconTrackerDigi : public JugAlgorithm {
private:
  Jug::Property<double> m_timeResolution{this, "timeResolution", 10}; // todo : add units
  Jug::Property<double> m_threshold{this, "threshold", 0. * Jug::Units::keV};
  Rndm::Numbers m_gaussDist;
  DataHandle<edm4hep::SimTrackerHitCollection> m_inputHitCollection{"inputHitCollection", Jug::DataHandle::Reader,
                                                                    this};
  DataHandle<eicd::RawTrackerHitCollection> m_outputHitCollection{"outputHitCollection", Jug::DataHandle::Writer,
                                                                  this};

public:
  //  ill-formed: using JugAlgorithm::JugAlgorithm;
  SiliconTrackerDigi(const std::string& name, ISvcLocator* svcLoc) : JugAlgorithm(name, svcLoc) {
    declareProperty("inputHitCollection", m_inputHitCollection, "");
    declareProperty("outputHitCollection", m_outputHitCollection, "");
  }
  StatusCode initialize() override {
    if (JugAlgorithm::initialize().isFailure()) {
      return StatusCode::FAILURE;
    }
    IRndmGenSvc* randSvc = svc<IRndmGenSvc>("RndmGenSvc", true);
    StatusCode sc        = m_gaussDist.initialize(randSvc, Rndm::Gauss(0.0, m_timeResolution.value()));
    if (!sc.isSuccess()) {
      return StatusCode::FAILURE;
    }
    return StatusCode::SUCCESS;
  }
  StatusCode execute() override {
    // input collection
    const auto* const simhits = m_inputHitCollection.get();
    // Create output collections
    auto* rawhits = m_outputHitCollection.createAndPut();
    // eicd::RawTrackerHitCollection* rawHitCollection = new eicd::RawTrackerHitCollection();
    std::map<long long, int> cell_hit_map;
    for (const auto& ahit : *simhits) {
      if (msgLevel(MSG::DEBUG)) {
        debug() << "--------------------" << ahit.getCellID() << endmsg;
        debug() << "Hit in cellID = " << ahit.getCellID() << endmsg;
        debug() << "     position = (" << ahit.getPosition().x << "," << ahit.getPosition().y << ","
                << ahit.getPosition().z << ")" << endmsg;
        debug() << "    xy_radius = " << std::hypot(ahit.getPosition().x, ahit.getPosition().y) << endmsg;
        debug() << "     momentum = (" << ahit.getMomentum().x << "," << ahit.getMomentum().y << ","
                << ahit.getMomentum().z << ")" << endmsg;
      }
      if (ahit.getEDep() * Jug::Units::keV < m_threshold) {
        if (msgLevel(MSG::DEBUG)) {
          debug() << "         edep = " << ahit.getEDep() << " (below threshold of " << m_threshold / Jug::Units::keV
                  << " keV)" << endmsg;
        }
        continue;
      } else {
        if (msgLevel(MSG::DEBUG)) {
          debug() << "         edep = " << ahit.getEDep() << endmsg;
        }
      }
      if (cell_hit_map.count(ahit.getCellID()) == 0) {
        cell_hit_map[ahit.getCellID()] = rawhits->size();
        eicd::RawTrackerHit rawhit(ahit.getCellID(),
                                   ahit.getMCParticle().getTime() * 1e6 + m_gaussDist() * 1e3, // ns->fs
                                   std::llround(ahit.getEDep() * 1e6));
        rawhits->push_back(rawhit);
      } else {
        auto hit = (*rawhits)[cell_hit_map[ahit.getCellID()]];
        hit.setTimeStamp(ahit.getMCParticle().getTime() * 1e6 + m_gaussDist() * 1e3);
        auto ch = hit.getCharge();
        hit.setCharge(ch + std::llround(ahit.getEDep() * 1e6));
      }
    }
    return StatusCode::SUCCESS;
  }
};

} // namespace Jug::Digi
