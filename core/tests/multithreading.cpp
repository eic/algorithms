// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for concurrent use of algorithms, logging, properties, services and random numbers,
// as in multithreaded frameworks. Run these with the thread sanitizer (USE_TSAN) to detect
// data races that do not change the results.
//
#include <algorithms/algorithm.h>
#include <algorithms/logger.h>
#include <algorithms/random.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <latch>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace algorithms;

namespace {

constexpr std::size_t kThreads    = 8;
constexpr std::size_t kIterations = 1000;

// Run f(thread index) on kThreads threads that all start at the same time
void runConcurrently(const std::function<void(std::size_t)>& f) {
  std::latch start{kThreads};
  std::vector<std::jthread> threads;
  for (std::size_t t = 0; t < kThreads; ++t) {
    threads.emplace_back([&f, &start, t] {
      start.arrive_and_wait();
      f(t);
    });
  }
}

// Scales the input by a configurable factor
class Scale : public Algorithm<Input<int, std::optional<int>, std::vector<int>>,
                               Output<int, std::vector<int>>> {
public:
  Scale()
      : Algorithm{"Scale",
                  {"value", "offset", "values"},
                  {"scaled", "scaledValues"},
                  "Scales values by a factor, with an optional offset"} {}

  void process(const Input& input, const Output& output) const final {
    const auto& [value, offset, values] = input;
    const auto& [scaled, scaled_values] = output;

    const int add = (offset != nullptr) ? *offset : 0;
    *scaled       = *value * m_factor + add;
    for (std::size_t i = 0; i < std::min(values.size(), scaled_values.size()); ++i) {
      *scaled_values[i] = *values[i] * m_factor + add;
    }
  }

private:
  Property<int> m_factor{this, "factor", 3, "Scale factor"};
};

class TestLogger : public LoggerMixin {
public:
  TestLogger() : LoggerMixin("TestLogger") {}
  using LoggerMixin::endmsg;
  using LoggerMixin::info;
};

// Records all log messages, from any thread
class Recorder {
public:
  // installed once, as the LogSvc keeps the action for the rest of the process
  static Recorder& instance() {
    static Recorder& recorder = []() -> Recorder& {
      static Recorder r;
      LogSvc::instance().init([](LogLevel, std::string_view, std::string_view msg) {
        std::lock_guard<std::mutex> lock{r.m_mutex};
        r.m_messages.emplace_back(msg);
      });
      return r;
    }();
    return recorder;
  }
  void clear() {
    std::lock_guard<std::mutex> lock{m_mutex};
    m_messages.clear();
  }
  std::vector<std::string> messages() {
    std::lock_guard<std::mutex> lock{m_mutex};
    return m_messages;
  }

private:
  std::mutex m_mutex;
  std::vector<std::string> m_messages;
};

// Distinct service types, to be accessed for the first time concurrently
constexpr const char* kIndexedSvcNames[] = {"IndexedSvc0", "IndexedSvc1", "IndexedSvc2",
                                            "IndexedSvc3", "IndexedSvc4", "IndexedSvc5",
                                            "IndexedSvc6", "IndexedSvc7"};
template <std::size_t N> class IndexedSvc : public Service<IndexedSvc<N>> {
public:
  void init() {}
  constexpr static const char* kName = kIndexedSvcNames[N];

protected:
  IndexedSvc() : Service<IndexedSvc<N>>(kName) {}
  friend class Service<IndexedSvc<N>>;
};

} // namespace

TEST_CASE("Algorithms can process concurrently on one instance", "[multithreading]") {
  Scale algo;
  algo.setProperty("factor", 2);
  const Scale& shared = algo;

  std::vector<std::size_t> failures(kThreads, 0);
  runConcurrently([&](std::size_t t) {
    for (std::size_t i = 0; i < kIterations; ++i) {
      const int value  = static_cast<int>(t * kIterations + i);
      const int offset = static_cast<int>(t);
      const std::vector<int> values{value, value + 1};
      int scaled = 0;
      std::vector<int> scaled_values(2, 0);

      shared.process({&value, (i % 2 == 0) ? &offset : nullptr, {&values[0], &values[1]}},
                     {&scaled, {&scaled_values[0], &scaled_values[1]}});

      const int add = (i % 2 == 0) ? offset : 0;
      if (scaled != 2 * value + add || scaled_values[0] != 2 * value + add ||
          scaled_values[1] != 2 * (value + 1) + add) {
        ++failures[t];
      }
    }
  });
  CHECK(failures == std::vector<std::size_t>(kThreads, 0));
}

TEST_CASE("Algorithms can be constructed and configured concurrently", "[multithreading]") {
  std::vector<int> results(kThreads, 0);
  runConcurrently([&](std::size_t t) {
    Scale algo;
    algo.setProperty("factor", static_cast<int>(t));
    algo.validate();
    const int value = 10;
    int scaled = 0;
    algo.process({&value, nullptr, {}}, {&scaled, {}});
    results[t] = scaled;
  });
  for (std::size_t t = 0; t < kThreads; ++t) {
    CHECK(results[t] == static_cast<int>(10 * t));
  }
}

TEST_CASE("Properties can be read concurrently", "[multithreading]") {
  const Scale algo;
  std::vector<std::size_t> failures(kThreads, 0);
  runConcurrently([&](std::size_t t) {
    for (std::size_t i = 0; i < kIterations; ++i) {
      if (algo.getProperty<int64_t>("factor") != 3 || !algo.hasProperty("factor") ||
          !algo.missingProperties().empty()) {
        ++failures[t];
      }
    }
  });
  CHECK(failures == std::vector<std::size_t>(kThreads, 0));
}

TEST_CASE("Loggers report complete messages from concurrent threads", "[multithreading]") {
  auto& recorder = Recorder::instance();
  recorder.clear();
  const TestLogger logger;

  runConcurrently([&](std::size_t t) {
    for (std::size_t i = 0; i < kIterations / 10; ++i) {
      logger.info(fmt::format("plain {} {}", t, i));
      logger.info("formatted {} {}", t, i);
    }
  });

  std::set<std::string> expected;
  for (std::size_t t = 0; t < kThreads; ++t) {
    for (std::size_t i = 0; i < kIterations / 10; ++i) {
      expected.insert(fmt::format("plain {} {}", t, i));
      expected.insert(fmt::format("formatted {} {}", t, i));
    }
  }
  const auto messages = recorder.messages();
  CHECK(messages.size() == expected.size());
  CHECK(std::set<std::string>(messages.begin(), messages.end()) == expected);
}

TEST_CASE("Logger streams report complete messages from concurrent threads",
          "[multithreading]") {
  auto& recorder = Recorder::instance();
  recorder.clear();
  const TestLogger logger;

  runConcurrently([&](std::size_t t) {
    for (std::size_t i = 0; i < kIterations / 10; ++i) {
      logger.info() << "stream " << t << " " << i << TestLogger::endmsg;
    }
  });

  std::set<std::string> expected;
  for (std::size_t t = 0; t < kThreads; ++t) {
    for (std::size_t i = 0; i < kIterations / 10; ++i) {
      expected.insert(fmt::format("stream {} {}", t, i));
    }
  }
  const auto messages = recorder.messages();
  CHECK(messages.size() == expected.size());
  CHECK(std::set<std::string>(messages.begin(), messages.end()) == expected);
}

TEST_CASE("Different services can be accessed for the first time concurrently",
          "[multithreading]") {
  std::vector<const ServiceBase*> services(kThreads, nullptr);
  runConcurrently([&](std::size_t t) {
    switch (t) {
    case 0: services[t] = &IndexedSvc<0>::instance(); break;
    case 1: services[t] = &IndexedSvc<1>::instance(); break;
    case 2: services[t] = &IndexedSvc<2>::instance(); break;
    case 3: services[t] = &IndexedSvc<3>::instance(); break;
    case 4: services[t] = &IndexedSvc<4>::instance(); break;
    case 5: services[t] = &IndexedSvc<5>::instance(); break;
    case 6: services[t] = &IndexedSvc<6>::instance(); break;
    default: services[t] = &IndexedSvc<7>::instance(); break;
    }
  });
  const auto& registered = ServiceSvc::instance().services();
  for (std::size_t t = 0; t < kThreads; ++t) {
    const auto name = "IndexedSvc" + std::to_string(t);
    REQUIRE(registered.count(name) == 1);
    CHECK(registered.at(name) == services[t]);
  }
}

TEST_CASE("A service can be accessed for the first time concurrently", "[multithreading]") {
  std::vector<const RandomSvc*> services(kThreads, nullptr);
  runConcurrently([&](std::size_t t) { services[t] = &RandomSvc::instance(); });
  CHECK(std::all_of(services.begin(), services.end(),
                    [&](const auto* svc) { return svc == services[0]; }));
}

TEST_CASE("A generator can be shared between threads", "[multithreading]") {
  const Generator gen{RandomSvc::instance().createEngine(1), 16};
  std::vector<std::vector<uint64_t>> draws(kThreads);
  runConcurrently([&](std::size_t t) {
    for (std::size_t i = 0; i < kIterations; ++i) {
      draws[t].push_back(gen.uniform_int<uint64_t>(0, std::numeric_limits<uint64_t>::max()));
    }
  });

  // full-range draws map one-to-one onto the engine output, so all draws are distinct
  std::set<uint64_t> unique;
  for (const auto& d : draws) {
    unique.insert(d.begin(), d.end());
  }
  CHECK(unique.size() == kThreads * kIterations);
}

TEST_CASE("Generators of one engine can be used in parallel threads", "[multithreading]") {
  const auto engine = RandomSvc::instance().createEngine(2);
  std::vector<std::vector<uint64_t>> draws(kThreads);
  std::vector<double> gaussians(kThreads, 0.);
  runConcurrently([&](std::size_t t) {
    const Generator gen{engine, 16};
    for (std::size_t i = 0; i < kIterations; ++i) {
      draws[t].push_back(gen.uniform_int<uint64_t>(0, std::numeric_limits<uint64_t>::max()));
      gaussians[t] += gen.gaussian();
      gen.poisson(2.5);
    }
  });

  // each generator refreshes its cache from the shared engine, so all draws are distinct
  std::set<uint64_t> unique;
  for (const auto& d : draws) {
    unique.insert(d.begin(), d.end());
  }
  CHECK(unique.size() == kThreads * kIterations);
}
