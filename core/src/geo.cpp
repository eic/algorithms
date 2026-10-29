// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Wouter Deconinck, Sylvester Joosten
//
// Implementation of the geo service
//
#include <algorithms/geo.h>

namespace algorithms {

void GeoSvc::init(const dd4hep::Detector* det) {
  if (det) {
    info() << "Initializing geometry service from pre-initialized detector" << endmsg;
    m_detector = det;
    // no detector given, need to self-initialize
  } else {
    info() << "No external detector provided, self-initializing" << endmsg;
    if (m_xml_list.empty()) {
      raise<ServiceError>("No external detector provided and no compact files specified in "
                          "the 'detectors' property");
    }
    auto detector = dd4hep::Detector::make_unique("");
    for (std::string_view name : m_xml_list) {
      info() << fmt::format("Loading compact file: {}", name) << endmsg;
      detector->fromCompact(std::string(name));
    }
    detector->volumeManager();
    detector->apply("DD4hepVolumeManager", 0, nullptr);
    m_detector_ptr = std::move(detector);
    m_detector = m_detector_ptr.get();
  }
  // always: instantiate cellIDConverter
  m_converter = std::make_unique<const dd4hep::rec::CellIDPositionConverter>(*m_detector);
}
} // namespace algorithms

