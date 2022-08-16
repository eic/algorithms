// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Chao Peng, Wouter Deconinck, Whitney Armstrong, Sylvester Joosten, Jihee Kim

// Apply Birks Law to correct the energy deposit
// It uses the contributions member in edm4hep::CalorimeterHit, so simulation must enable storeCalorimeterContributions
//
// Author: Chao Peng
// Date: 09/29/2021

#include "JugDigi/CalorimeterBirksCorr.h"

namespace algorithms::digi {

edm4hep::SimCalorimeterHitCollection CalorimeterBirksCorr::operator()(const edm4hep::SimCalorimeterHitCollection& input) const {
  edm4hep::SimCalorimeterHitCollection ohits;
  for (const auto& hit : input) {
    auto ohit     = ohits.create(hit.getCellID(), hit.getEnergy(), hit.getPosition());
    double energy = 0.;
    for (const auto& c : hit.getContributions()) {
      ohit.addToContributions(c);
      const double charge = m_pidSvc(c.getPDG()).charge;
      // some tolerance for precision
      if (std::abs(charge) > 1e-5) {
        // FIXME
        // energy += c.getEnergy() / (1. + c.getEnergy() / c.length * m_birksConstant);
        error() << "edm4hep::CaloHitContribution has no length field for Birks correction." << endmsg;
      }
    }
    // replace energy deposit with Birks Law corrected value
    ohit.setEnergy(energy);
  }
  return ohits;
}

} // namespace algorithms::digi
