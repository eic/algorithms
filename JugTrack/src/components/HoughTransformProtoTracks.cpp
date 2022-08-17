// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong

#include <cmath>
// Jug
#include "Jug/Property.h"
#include "JugAlg/JugAlgorithm.h"
#include "JugAlg/JugTool.h"
#include "JugAlg/Transformer.h"
#include "JugKernel/ToolHandle.h"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"
#include "JugTrack/ProtoTrack.hpp"
#include "JugTrack/Track.hpp"

#include "Math/Vector3D.h"

#include "eicd/TrackerHitCollection.h"

namespace Jug::Reco {

/** Hough transform proto track finder.
 *
 *  \ingroup tracking
 */
class HoughTransformProtoTracks : public JugAlgorithm {
private:
  DataHandle<eicd::TrackerHitCollection> m_inputTrackerHits{"inputTrackerHits", Jug::DataHandle::Reader, this};
  DataHandle<Jug::ProtoTrackContainer> m_outputProtoTracks{"outputProtoTracks", Jug::DataHandle::Writer, this};

public:
  HoughTransformProtoTracks(const std::string& name, ISvcLocator* svcLoc) : JugAlgorithm(name, svcLoc) {
    declareProperty("inputTrackerHits", m_inputTrackerHits, "tracker hits whose indices are used in proto-tracks");
    declareProperty("outputProtoTracks", m_outputProtoTracks, "grouped hit indicies");
  }

  StatusCode initialize() override {
    if (JugAlgorithm::initialize().isFailure())
      return StatusCode::FAILURE;
    return StatusCode::SUCCESS;
  }

  StatusCode execute() override {
    // input collection
    //const eicd::TrackerHitCollection* hits = m_inputTrackerHits.get();
    // Create output collections
    //auto proto_tracks = m_outputProtoTracks.createAndPut();

    return StatusCode::SUCCESS;
  }
};

} // namespace Jug::Reco
