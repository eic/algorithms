// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the ServiceSvc and the Service base class
//
// The ServiceSvc is a process-wide singleton that can only be initialized once, so the
// full service lifecycle is tested in a single test case.
//
#include <algorithms/service.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>
#include <string_view>

using namespace algorithms;
using Catch::Matchers::ContainsSubstring;

namespace {

// Counts how often it is initialized
class CountingSvc : public Service<CountingSvc> {
public:
  void init() { ++m_count; }
  int count() const { return m_count; }

private:
  int m_count = 0;

  ALGORITHMS_DEFINE_SERVICE(CountingSvc)
};

// Fails to initialize on the first attempt
class FlakySvc : public Service<FlakySvc> {
public:
  void init() {
    if (m_fail) {
      m_fail = false;
      throw std::runtime_error("FlakySvc failed to initialize");
    }
  }

private:
  bool m_fail = true;

  ALGORITHMS_DEFINE_SERVICE(FlakySvc)
};

// Has a property without default value
class ConfiguredSvc : public Service<ConfiguredSvc> {
public:
  void init() {}

private:
  Property<int> m_required{this, "required", "required property without default"};

  ALGORITHMS_DEFINE_SERVICE(ConfiguredSvc)
};

// Needs a custom initializer
class CustomInitSvc : public Service<CustomInitSvc> {
public:
  void init() { m_value = -1; }
  void init(int value) { m_value = value; }
  int value() const { return m_value; }

private:
  int m_value = 0;

  ALGORITHMS_DEFINE_SERVICE(CustomInitSvc)
};

// Never instantiated, so never registered
class UnusedSvc : public Service<UnusedSvc> {
public:
  void init() {}

  ALGORITHMS_DEFINE_SERVICE(UnusedSvc)
};

} // namespace

TEST_CASE("ServiceSvc initializes all registered services", "[service]") {
  auto& svcs = ServiceSvc::instance();
  CHECK(svcs.name() == "ServiceSvc");

  // a custom initializer registered before the service is instantiated is kept
  svcs.setInit<CustomInitSvc>([](CustomInitSvc& s) { s.init(42); });

  // services register themselves when they are first accessed
  auto& counting   = CountingSvc::instance();
  auto& flaky      = FlakySvc::instance();
  auto& configured = ConfiguredSvc::instance();
  auto& custom     = CustomInitSvc::instance();

  CHECK(counting.name() == "CountingSvc");
  CHECK(counting.description() == "CountingSvc");
  CHECK(svcs.has<CountingSvc>());
  CHECK(svcs.has<FlakySvc>());
  CHECK(svcs.has<ConfiguredSvc>());
  CHECK(svcs.has<CustomInitSvc>());
  CHECK_FALSE(svcs.has<UnusedSvc>());
  CHECK(svcs.services().size() == 4);
  CHECK(svcs.service("CountingSvc") == &counting);
  CHECK(svcs.service<CountingSvc>("CountingSvc") == &counting);
  CHECK_THROWS_AS(svcs.service("UnusedSvc"), std::out_of_range);

  // nothing is ready before initialization
  CHECK_FALSE(svcs.ready());
  CHECK_FALSE(counting.ready());
  CHECK_THROWS_AS(svcs.initSingle<CountingSvc>(), ServiceError);
  CHECK_THROWS_WITH(svcs.initSingle<CountingSvc>(),
                    "initSingle<CountingSvc>() should not be called before/instead of init()");

  // initialization stops at the first service that throws
  CHECK_THROWS_WITH(svcs.init(), ContainsSubstring(R"(Encountered uninitialized services: ["FlakySvc"])"));
  CHECK(counting.ready());
  CHECK(counting.count() == 1);
  CHECK_FALSE(flaky.ready());
  CHECK_FALSE(svcs.ready());

  // initialization stops at the first service with missing properties
  CHECK_THROWS_WITH(
      svcs.init(),
      ContainsSubstring(R"(Encountered missing service properties: {"ConfiguredSvc": ["required"]})"));
  CHECK(counting.count() == 2);
  CHECK(flaky.ready());
  CHECK_FALSE(configured.ready());
  CHECK_FALSE(svcs.ready());

  // initialization succeeds once all properties are set
  configured.setProperty("required", 1);
  CHECK_NOTHROW(svcs.init());
  CHECK(svcs.ready());
  CHECK(counting.count() == 3);
  CHECK(configured.ready());
  CHECK(custom.ready());
  CHECK(custom.value() == 42);
  CHECK_NOTHROW(svcs.validate());

  // services can only be initialized once
  CHECK_THROWS_WITH(svcs.init(), "Cannot initialize services twice");
  CHECK_THROWS_WITH(svcs.initSingle<CountingSvc>(),
                    "Cannot initialize service CountingSvc - already initialized");
  CHECK(counting.count() == 3);

  // a single service that is no longer ready can be initialized again
  counting.ready(false);
  CHECK_FALSE(svcs.ready());
  CHECK_THROWS_WITH(svcs.validate(), ContainsSubstring(R"(Encountered uninitialized services: ["CountingSvc"])"));
  svcs.initSingle<CountingSvc>();
  CHECK(counting.ready());
  CHECK(counting.count() == 4);
  CHECK(svcs.ready());
}
