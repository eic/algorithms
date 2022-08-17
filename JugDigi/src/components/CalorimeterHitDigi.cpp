// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Chao Peng, Wouter Deconinck, Sylvester Joosten

// A general digitization for CalorimeterHit from simulation
// 1. Smear energy deposit with a/sqrt(E/GeV) + b + c/E or a/sqrt(E/GeV) (relative value)
// 2. Digitize the energy with dynamic ADC range and add pedestal (mean +- sigma)
// 3. Time conversion with smearing resolution (absolute value)
// 4. Signal is summed if the SumFields are provided
//
// Author: Chao Peng
// Date: 06/02/2021

#include "JugDigi/CalorimeterHitDigi.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "DD4hep/DD4hepUnits.h"
#include "DDRec/CellIDPositionConverter.h"
#include "DDSegmentation/BitFieldCoder.h"

#include "fmt/format.h"
#include "fmt/ranges.h"

// Event Model related classes
#include "eicd/RawCalorimeterHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"

namespace algorithms::digi {

bool CalorimeterHitDigi::initialize() {
  // set energy resolution numbers
  for (size_t i = 0; i < u_eRes.value().size() && i < 3; ++i) {
    eRes[i] = u_eRes.value()[i];
  }

  // using juggler internal units (GeV, mm, radian, ns)
  dyRangeADC = m_dyRangeADC.value() / dd4hep::GeV;
  tRes       = m_tRes.value() / dd4hep::ns;
  stepTDC    = dd4hep::ns / m_resolutionTDC.value();

  // need signal sum
  if (!u_fields.value().empty()) {
    // sanity checks
    if (!m_geoSvc()) {
      error() << "Unable to locate Geometry Service. "
              << "Make sure you have GeoSvc and SimSvc in the right order in the configuration." << std::endl;
      ;
      return false;
    }
    if (m_readout.value().empty()) {
      error() << "readoutClass is not provided, it is needed to know the fields in readout ids" << std::endl;
      ;
      return false;
    }

    // get decoders
    try {
      auto id_desc = m_geoSvc()->readout(m_readout.value()).idSpec();
      id_mask      = 0;
      std::vector<std::pair<std::string, int>> ref_fields;
      for (size_t i = 0; i < u_fields.value().size(); ++i) {
        id_mask |= id_desc.field(u_fields.value()[i])->mask();
        // use the provided id number to find ref cell, or use 0
        int ref = i < u_refs.value().size() ? u_refs.value()[i] : 0;
        ref_fields.emplace_back(u_fields.value()[i], ref);
      }
      ref_mask = id_desc.encode(ref_fields);
      // debug() << fmt::format("Referece id mask for the fields {:#064b}", ref_mask) << std::endl;;
    } catch (...) {
      error() << "Failed to load ID decoder for " << m_readout.value() << std::endl;
      ;
      return false;
    }
    id_mask = ~id_mask;
    info() << fmt::format("ID mask in {:s}: {:#064b}", m_readout.value(), id_mask) << std::endl;
    ;
    return true;
  }

  return true;
}

eicd::RawCalorimeterHitCollection CalorimeterHitDigi::operator()(const edm4hep::SimCalorimeterHitCollection& input) const {
  if (!u_fields.value().empty()) {
    return signal_sum_digi(input);
  } else {
    return single_hits_digi(input);
  }
}

eicd::RawCalorimeterHitCollection
CalorimeterHitDigi::single_hits_digi(const edm4hep::SimCalorimeterHitCollection& simhits) const {
  eicd::RawCalorimeterHitCollection rawhits;

  for (const auto& ahit : simhits) {
    // Note: juggler internal unit of energy is GeV
    const double eDep = ahit.getEnergy();

    // apply additional calorimeter noise to corrected energy deposit
    const double eResRel = (eDep > 1e-6) ? m_normDist() * std::sqrt(std::pow(eRes[0] / std::sqrt(eDep), 2) +
                                                                    std::pow(eRes[1], 2) + std::pow(eRes[2] / (eDep), 2))
                                         : 0;

    const double ped = m_pedMeanADC.value() + m_normDist() * m_pedSigmaADC.value();
    const long long adc =
        std::llround(ped + m_corrMeanScale.value() * eDep * (1. + eResRel) / dyRangeADC * m_capADC.value());

    double time = std::numeric_limits<double>::max();
    for (const auto& c : ahit.getContributions()) {
      if (c.getTime() <= time) {
        time = c.getTime();
      }
    }
    const long long tdc = std::llround((time + m_normDist() * tRes) * stepTDC);

    eicd::RawCalorimeterHit rawhit(ahit.getCellID(), (adc > m_capADC.value() ? m_capADC.value() : adc), tdc);
    rawhits->push_back(rawhit);
  }

  return rawhits;
}

eicd::RawCalorimeterHitCollection
CalorimeterHitDigi::signal_sum_digi(const edm4hep::SimCalorimeterHitCollection& simhits) const {
  eicd::RawCalorimeterHitCollection rawhits;

  // find the hits that belong to the same group (for merging)
  std::unordered_map<long long, std::vector<edm4hep::SimCalorimeterHit>> merge_map;
  for (const auto& ahit : simhits) {
    int64_t hid = (ahit.getCellID() & id_mask) | ref_mask;
    auto it     = merge_map.find(hid);

    if (it == merge_map.end()) {
      merge_map[hid] = {ahit};
    } else {
      it->second.push_back(ahit);
    }
  }

  // signal sum
  for (auto& [id, hits] : merge_map) {
    double edep     = hits[0].getEnergy();
    double time     = hits[0].getContributions(0).getTime();
    double max_edep = hits[0].getEnergy();
    // sum energy, take time from the most energetic hit
    for (size_t i = 1; i < hits.size(); ++i) {
      edep += hits[i].getEnergy();
      if (hits[i].getEnergy() > max_edep) {
        max_edep = hits[i].getEnergy();
        for (const auto& c : hits[i].getContributions()) {
          if (c.getTime() <= time) {
            time = c.getTime();
          }
        }
      }
    }

    double eResRel = 0.;
    // safety check
    if (edep > 1e-6) {
      eResRel = m_normDist() * eRes[0] / std::sqrt(edep) + m_normDist() * eRes[1] + m_normDist() * eRes[2] / edep;
    }
    double ped             = m_pedMeanADC.value() + m_normDist() * m_pedSigmaADC.value();
    unsigned long long adc = std::llround(ped + edep * (1. + eResRel) / dyRangeADC * m_capADC.value());
    unsigned long long tdc = std::llround((time + m_normDist() * tRes) * stepTDC);

    eicd::RawCalorimeterHit rawhit(id, (adc > m_capADC.value() ? m_capADC.value() : adc), tdc);
    rawhits.push_back(rawhit);
  }

  return rawhits;
}

} // namespace algorithms::digi
