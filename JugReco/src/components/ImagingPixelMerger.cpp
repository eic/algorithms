// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Chao Peng, Sylvester Joosten, Wouter Deconinck, Whitney Armstrong

/*
 *  A hits merger for ecal barrel to prepare dataset for machine learning
 *
 *  Author: Chao Peng (ANL), 05/04/2021
 *
 *  SJJ: @TODO: this should really be a clustering algorithm, as it doesn't give us
 *              fully consistent hits anymore (e.g. cellID, local position) (meaning it's
 *              not generically useful as a reconstruction algorithm.
 */
#include <algorithm>
#include <bitset>
#include <unordered_map>

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
#include "JugBase/Utilities/Utils.hpp"

// Event Model related classes
#include "eicd/CalorimeterHitCollection.h"
#include <eicd/vector_utils.h>

using namespace Jug::Units;

struct PairHashFunction {
  template <class T1, class T2> std::size_t operator()(const std::pair<T1, T2>& pair) const {
    return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
  }
};

namespace Jug::Reco {

/** Hits merger for ML algorithm input.
 *
 * A hits merger to prepare dataset for machine learning
 * It merges hits with a defined grid size.
 * Merged hits will be relocated to the grid center and the energies will be summed.
 *
 * \ingroup reco
 */
class ImagingPixelMerger : public JugAlgorithm {
private:
  Jug::Property<float> m_etaSize{this, "etaSize", 0.001};
  Jug::Property<float> m_phiSize{this, "phiSize", 0.001};
  DataHandle<eicd::CalorimeterHitCollection> m_inputHits{"inputHits", Jug::DataHandle::Reader, this};
  DataHandle<eicd::CalorimeterHitCollection> m_outputHits{"outputHits", Jug::DataHandle::Writer, this};

public:
  ImagingPixelMerger(const std::string& name, ISvcLocator* svcLoc) : JugAlgorithm(name, svcLoc) {
    declareProperty("inputHits", m_inputHits, "");
    declareProperty("outputHits", m_outputHits, "");
  }

  StatusCode initialize() override {
    if (JugAlgorithm::initialize().isFailure()) {
      return StatusCode::FAILURE;
    }

    return StatusCode::SUCCESS;
  }

  StatusCode execute() override {
    // input collections
    const auto& hits = *m_inputHits.get();
    // Create output collections
    auto& ohits = *m_outputHits.createAndPut();

    // @TODO: add timing information
    // group the hits by grid per layer
    struct GridData {
      unsigned nHits;
      float rc;
      float energy;
      float energyError;
      float time;
      float timeError;
      int sector; // sector associated with one of the merged hits
    };
    // @TODO: remove this hard-coded value
    int max_nlayers = 50;
    std::vector<std::unordered_map<std::pair<int, int>, GridData, PairHashFunction>> group_hits(max_nlayers);
    for (const auto& h : hits) {
      auto k = h.getLayer();
      if ((int)k >= max_nlayers) {
        continue;
      }
      auto& layer     = group_hits[k];
      const auto& pos = h.getPosition();

      // cylindrical r
      const float rc   = eicd::magnitudeTransverse(pos);
      const double eta = eicd::eta(pos);
      const double phi = eicd::angleAzimuthal(pos);

      const auto grid = std::pair<int, int>{pos2grid(eta, m_etaSize), pos2grid(phi, m_phiSize)};
      auto it         = layer.find(grid);
      // merge energy
      if (it != layer.end()) {
        auto& data = it->second;
        data.nHits += 1;
        data.energy += h.getEnergy();
        data.energyError += h.getEnergyError() * h.getEnergyError();
        data.time += h.getTime();
        data.timeError += h.getTimeError() * h.getTimeError();
      } else {
        layer[grid] = GridData{1,
                               rc,
                               h.getEnergy(),
                               h.getEnergyError() * h.getEnergyError(),
                               h.getTime(),
                               h.getTimeError() * h.getTimeError(),
                               h.getSector()};
      }
    }

    // convert grid data back to hits
    for (const auto& [i, layer] : Jug::Utils::Enumerate(group_hits)) {
      for (const auto& [grid, data] : layer) {
        const double eta   = grid2pos(grid.first, m_etaSize);
        const double phi   = grid2pos(grid.second, m_phiSize);
        const double theta = eicd::etaToAngle(eta);
        const double z     = cotan(theta) * data.rc;
        const float r      = std::hypot(data.rc, z);
        const auto pos     = eicd::sphericalToVector(r, theta, phi);
        auto oh            = ohits.create();
        oh.setEnergy(data.energy);
        oh.setEnergyError(std::sqrt(data.energyError));
        oh.setTime(data.time / data.nHits);
        oh.setTimeError(std::sqrt(data.timeError));
        oh.setPosition(pos);
        oh.setLayer(i);
        oh.setSector(data.sector);
      }
    }
    return StatusCode::SUCCESS;
  }

private:
  static int pos2grid(float pos, float size, float offset = 0.) {
    return std::floor((pos - offset) / size);
  }
  static int grid2pos(float grid, float size, float offset = 0.) {
    return (grid + 0.5) * size + offset;
  }
  static float cotan(float theta) {
    if (std::abs(std::sin(theta)) < 1e-6) {
      return 0.;
    } else {
      return 1. / std::tan(theta);
    }
  }
}; // class ImagingPixelMerger


} // namespace Jug::Reco
