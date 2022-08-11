// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Wouter Deconinck, Whitney Armstrong, Chao Peng

// Jug
#include "JugAlg/JugAlgorithm.h"
#include "Jug/Property.h"
#include "JugAlg/JugTool.h"
#include "JugAlg/Transformer.h"

#include "JugBase/DataHandle.h"

// Event Model related classes
#include "edm4hep/SimTrackerHitCollection.h"

namespace Jug::Digi {

    /** Collect the tracking hits into a single collection.
     *
     * \param inputSimTrackerHits [in] vector of collection names
     * \param outputSimTrackerHits [out] hits combined into one collection.
     *
     * \ingroup digi
     */
    class SimTrackerHitsCollector : public JugAlgorithm {
    private:
      Jug::Property<std::vector<std::string>> m_inputSimTrackerHits{this, "inputSimTrackerHits", {},"Tracker hits to be aggregated"};
      DataHandle<edm4hep::SimTrackerHitCollection> m_outputSimTrackerHits{"outputSimTrackerHits", Jug::DataHandle::Writer, this};

      std::vector<DataHandle<edm4hep::SimTrackerHitCollection>*> m_hitCollections;

    public:
      SimTrackerHitsCollector(const std::string& name, ISvcLocator* svcLoc)
          : JugAlgorithm(name, svcLoc)
      {
        declareProperty("outputSimTrackerHits", m_outputSimTrackerHits, "output hits combined into single collection");
      }
      ~SimTrackerHitsCollector() {
        for (auto* col : m_hitCollections) {
          delete col;
        }
      }

      StatusCode initialize() override {
        if (JugAlgorithm::initialize().isFailure()) {
          return StatusCode::FAILURE;
        }
        for (auto colname : m_inputSimTrackerHits) {
          debug() << "initializing collection: " << colname  << endmsg;
          m_hitCollections.push_back(new DataHandle<edm4hep::SimTrackerHitCollection>{colname, Jug::DataHandle::Reader, this});
        }
        return StatusCode::SUCCESS;
      }

      StatusCode execute() override
      {
        auto* outputHits = m_outputSimTrackerHits.createAndPut();
        if (msgLevel(MSG::DEBUG)) {
          debug() << "execute collector" << endmsg;
        }
        for(const auto& hits: m_hitCollections) {
          const edm4hep::SimTrackerHitCollection* hitCol = hits->get();
          if (msgLevel(MSG::DEBUG)) {
            debug() << "col n hits: " << hitCol->size() << endmsg;
          }
          for (const auto& ahit : *hitCol) {
            outputHits->push_back(ahit.clone());
          }
        }
        return StatusCode::SUCCESS;
      }
    };

} // namespace Jug::Digi
