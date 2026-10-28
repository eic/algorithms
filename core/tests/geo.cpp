// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the GeoSvc, in standalone mode and with a pre-initialized detector
//
#include <algorithms/geo.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <DD4hep/Detector.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace algorithms;

namespace {
const std::string kCompactFile{TEST_DATA_DIR "/minimal.xml"};

std::vector<std::string>& messages() {
  static std::vector<std::string> m;
  return m;
}

void capture() {
  messages().clear();
  LogSvc::instance().init([](LogLevel, std::string_view, std::string_view msg) {
    messages().emplace_back(msg);
  });
}
} // namespace

TEST_CASE("GeoSvc loads compact files in standalone mode", "[geo]") {
  auto& svc = GeoSvc::instance();
  svc.setProperty("detectors", std::vector<std::string>{kCompactFile});
  svc.init();

  CHECK(svc.world().isValid());
  CHECK(svc.world().name() == std::string{"world"});
  CHECK(svc.detector()->world().ptr() == svc.world().ptr());
  CHECK(svc.cellIDPositionConverter().get() != nullptr);
}

TEST_CASE("GeoSvc uses a pre-initialized detector", "[geo]") {
  // constructed before the GeoSvc, so it outlives the GeoSvc
  static const auto detector = [] {
    auto det = dd4hep::Detector::make_unique("preinitialized");
    det->fromCompact(kCompactFile);
    return det;
  }();

  auto& svc = GeoSvc::instance();
  svc.init(detector.get());
  const auto* converter = svc.cellIDPositionConverter().get();

  CHECK(svc.detector() == detector.get());
  CHECK(svc.world().ptr() == detector->world().ptr());
  CHECK(converter != nullptr);

  // initializing again creates a new cell ID position converter
  svc.init(detector.get());
  CHECK(svc.cellIDPositionConverter().get() != converter);
}

TEST_CASE("GeoSvc requires compact files in standalone mode", "[geo]") {
  capture();
  auto& svc = GeoSvc::instance();
  svc.setProperty("detectors", std::vector<std::string>{});

  CHECK_THROWS_AS(svc.init(), ServiceError);
  CHECK_THROWS_WITH(svc.init(), "No external detector provided and no compact files specified "
                                "in the 'detectors' property");
}

TEST_CASE("GeoSvc logs the compact files it loads", "[geo]") {
  capture();
  auto& svc = GeoSvc::instance();
  svc.setProperty("detectors", std::vector<std::string>{kCompactFile});
  svc.init();

  CHECK(std::find(messages().begin(), messages().end(), "Loading compact file: " + kCompactFile) !=
        messages().end());
}
