// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the CachedBitGenerator, the Generator front-end and the RandomSvc
//
#include <algorithms/random.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using namespace algorithms;
using Catch::Approx;

namespace {

using value_type = detail::CachedBitGenerator::result_type;

// Engine that returns a running counter and records how often and with which size it is called
struct CountingEngine {
  std::shared_ptr<value_type> next = std::make_shared<value_type>(0);
  std::shared_ptr<std::vector<std::size_t>> calls =
      std::make_shared<std::vector<std::size_t>>();

  std::vector<value_type> operator()(std::size_t n) const {
    calls->push_back(n);
    std::vector<value_type> ret(n);
    for (auto& v : ret) {
      v = (*next)++;
    }
    return ret;
  }
};

// Engine based on a seeded Mersenne twister
RandomEngineCB mtEngine(std::size_t seed) {
  auto gen = std::make_shared<std::mt19937_64>(seed);
  return [gen](std::size_t n) {
    std::vector<value_type> ret(n);
    for (auto& v : ret) {
      v = (*gen)();
    }
    return ret;
  };
}

// Engine that always returns the same value
RandomEngineCB constantEngine(value_type value) {
  return [value](std::size_t n) { return std::vector<value_type>(n, value); };
}

struct Message {
  LogLevel level;
  std::string msg;
  bool operator==(const Message&) const = default;
};

std::vector<Message>& messages() {
  static std::vector<Message> m;
  return m;
}

void capture() {
  messages().clear();
  LogSvc::instance().init([](LogLevel l, std::string_view, std::string_view msg) {
    messages().push_back({l, std::string{msg}});
  });
}

constexpr int kDraws = 10000;

} // namespace

TEST_CASE("CachedBitGenerator is a uniform random bit generator", "[random]") {
  STATIC_REQUIRE(std::uniform_random_bit_generator<detail::CachedBitGenerator>);
  STATIC_REQUIRE(detail::CachedBitGenerator::min() == 0);
  STATIC_REQUIRE(detail::CachedBitGenerator::max() == std::numeric_limits<value_type>::max());
}

TEST_CASE("CachedBitGenerator refreshes its cache when exhausted", "[random]") {
  const CountingEngine engine;
  detail::CachedBitGenerator gen{engine, 3};

  // no numbers are requested before the first access
  CHECK(engine.calls->empty());

  std::vector<value_type> values;
  for (int i = 0; i < 7; ++i) {
    values.push_back(gen());
  }
  CHECK(values == std::vector<value_type>{0, 1, 2, 3, 4, 5, 6});
  CHECK(*engine.calls == std::vector<std::size_t>{3, 3, 3});
}

TEST_CASE("Generator draws uniform integers", "[random]") {
  const Generator gen{mtEngine(1), 16};

  for (int i = 0; i < kDraws; ++i) {
    const auto def = gen.uniform_int();
    STATIC_REQUIRE(std::is_same_v<decltype(def), const int>);
    REQUIRE((def == 0 || def == 1));

    const auto custom = gen.uniform_int<int64_t>(-5, 5);
    STATIC_REQUIRE(std::is_same_v<decltype(custom), const int64_t>);
    REQUIRE(custom >= -5);
    REQUIRE(custom <= 5);
  }
}

TEST_CASE("Generator draws uniform floating point numbers", "[random]") {
  const Generator gen{mtEngine(2), 16};

  double sum = 0;
  for (int i = 0; i < kDraws; ++i) {
    const auto def = gen.uniform_double();
    STATIC_REQUIRE(std::is_same_v<decltype(def), const double>);
    REQUIRE(def >= 0.);
    REQUIRE(def < 1.);
    sum += def;

    const auto custom = gen.uniform_double<float>(2.f, 3.f);
    STATIC_REQUIRE(std::is_same_v<decltype(custom), const float>);
    REQUIRE(custom >= 2.f);
    REQUIRE(custom <= 3.f);
  }
  CHECK(sum / kDraws == Approx(0.5).margin(0.02));
}

TEST_CASE("Generator draws exponentially distributed numbers", "[random]") {
  const Generator gen{mtEngine(4), 16};

  double sum_def    = 0;
  double sum_custom = 0;
  for (int i = 0; i < kDraws; ++i) {
    const auto def = gen.exponential();
    REQUIRE(def >= 0.);
    sum_def += def;

    const auto custom = gen.exponential<float>(4.f);
    STATIC_REQUIRE(std::is_same_v<decltype(custom), const float>);
    REQUIRE(custom >= 0.f);
    sum_custom += custom;
  }
  CHECK(sum_def / kDraws == Approx(1.).margin(0.05));
  CHECK(sum_custom / kDraws == Approx(0.25).margin(0.0125));
}

TEST_CASE("Generator draws Gaussian distributed numbers", "[random]") {
  const Generator gen{mtEngine(5), 16};

  double sum_def    = 0;
  double sum2_def   = 0;
  double sum_custom = 0;
  for (int i = 0; i < kDraws; ++i) {
    const auto def = gen.gaussian();
    sum_def += def;
    sum2_def += def * def;

    const auto custom = gen.gaussian<float>(10.f, 2.f);
    STATIC_REQUIRE(std::is_same_v<decltype(custom), const float>);
    sum_custom += custom;
  }
  CHECK(sum_def / kDraws == Approx(0.).margin(0.05));
  CHECK(sum2_def / kDraws == Approx(1.).margin(0.05));
  CHECK(sum_custom / kDraws == Approx(10.).margin(0.1));
}

TEST_CASE("Generators with the same engine sequence draw the same numbers", "[random]") {
  const Generator gen1{mtEngine(6), 16};
  const Generator gen2{mtEngine(6), 16};
  for (int i = 0; i < 100; ++i) {
    REQUIRE(gen1.gaussian() == gen2.gaussian());
  }
}

TEST_CASE("RandomSvc creates engines that return the requested number of values", "[random]") {
  auto engine = RandomSvc::instance().createEngine(7);
  CHECK(engine(0).empty());
  CHECK(engine(5).size() == 5);
}

TEST_CASE("RandomSvc engines with the same seed draw the same numbers", "[random]") {
  auto& svc    = RandomSvc::instance();
  auto engine1 = svc.createEngine(8);
  auto engine2 = svc.createEngine(8);
  CHECK(engine1(5) == engine2(5));
}

// Known bug: createEngine passes its std::mt19937_64 to std::generate by value, so the
// engine state never advances and every call returns the same numbers
TEST_CASE("RandomSvc engines draw new numbers on every call", "[random][!shouldfail]") {
  auto engine = RandomSvc::instance().createEngine(9);
  CHECK(engine(5) != engine(5));
}

// Known bug: all engines created by createEngine share one static std::mt19937_64, which is
// seeded only by the first engine that is called, so the seed of later engines is ignored
TEST_CASE("RandomSvc engines with different seeds draw different numbers",
          "[random][!shouldfail]") {
  auto& svc    = RandomSvc::instance();
  auto engine1 = svc.createEngine(10);
  auto engine2 = svc.createEngine(11);
  CHECK(engine1(5) != engine2(5));
}

// The RandomSvc is a process-wide singleton, so its lifecycle is tested in a single test case
TEST_CASE("RandomSvc provides generators", "[random]") {
  capture();
  auto& svc = RandomSvc::instance();
  CHECK(svc.name() == "RandomSvc");

  // the default engine works before initialization
  const double before = svc.generator().uniform_double();
  CHECK(before >= 0.);
  CHECK(before < 1.);

  // initialization without seed keeps the default engine
  svc.init();
  CHECK(messages().empty());

  // an external engine replaces the default engine
  svc.init(constantEngine(0));
  CHECK(svc.generator().uniform_double() == 0.);
  CHECK(messages() ==
        std::vector<Message>{{LogLevel::kInfo, "Loading external generator function."}});

  // generators request numbers from the engine in batches of the cache size
  const CountingEngine counting;
  svc.setProperty("cacheSize", std::size_t{4});
  svc.init(counting);
  const auto gen = svc.generator();
  for (int i = 0; i < 5; ++i) {
    gen.uniform_int<uint64_t>(0, std::numeric_limits<uint64_t>::max());
  }
  CHECK(*counting.calls == std::vector<std::size_t>{4, 4});

  // a custom seed is used for the internal engine, but ignored for an external engine
  messages().clear();
  svc.setProperty("seed", std::size_t{42});
  svc.init();
  const double seeded = svc.generator().uniform_double();
  CHECK(seeded >= 0.);
  CHECK(seeded < 1.);
  svc.init(constantEngine(0));
  CHECK(svc.generator().uniform_double() == 0.);
  CHECK(messages() ==
        std::vector<Message>{
            {LogLevel::kInfo, "Custom random seed requested: 42"},
            {LogLevel::kInfo, "Loading external generator function."},
            {LogLevel::kWarning,
             "Custom random seed request ignored when using external generator function"}});
}
