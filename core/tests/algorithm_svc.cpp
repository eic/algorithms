// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the AlgorithmSvc factory service
//
// The AlgorithmSvc only works after ServiceSvc::init(), which can only be called once per
// process, so the full lifecycle is tested in a single test case.
//
#include <algorithms/algorithm.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

using namespace algorithms;
using Catch::Matchers::ContainsSubstring;

namespace algorithm_svc_test {

using IntAlgorithm    = Algorithm<Input<int>, Output<int>>;
using DoubleAlgorithm = Algorithm<Input<double>, Output<double>>;

class AddOne : public IntAlgorithm {
public:
  AddOne() : IntAlgorithm{"AddOne", {"in"}, {"out"}, "Adds one"} {}
  void process(const Input& input, const Output& output) const final {
    *std::get<0>(output) = *std::get<0>(input) + 1;
  }
};

class Twice : public IntAlgorithm {
public:
  Twice() : IntAlgorithm{"Twice", {"in"}, {"out"}, "Multiplies by two"} {}
  void process(const Input& input, const Output& output) const final {
    *std::get<0>(output) = *std::get<0>(input) * 2;
  }
};

class Half : public DoubleAlgorithm {
public:
  Half() : DoubleAlgorithm{"Half", {"in"}, {"out"}, "Divides by two"} {}
  void process(const Input& input, const Output& output) const final {
    *std::get<0>(output) = *std::get<0>(input) / 2;
  }
};

} // namespace algorithm_svc_test

using namespace algorithm_svc_test;

TEST_CASE("AlgorithmSvc creates registered algorithms by type and name", "[algorithm]") {
  auto& svc = AlgorithmSvc::instance();
  CHECK(svc.name() == "AlgorithmSvc");

  svc.add<AddOne>();
  svc.add<Twice>();
  svc.add<Half>();
  // adding an algorithm twice has no effect
  svc.add<AddOne>();

  // the service cannot be used before all services are initialized
  CHECK_THROWS_AS(svc.ls<IntAlgorithm>(), Error);
  CHECK_THROWS_WITH(svc.get<IntAlgorithm>("algorithm_svc_test::AddOne"),
                    "Attempt to use AlgorithmSvc, but service not yet marked as ready");

  ServiceSvc::instance().init();
  REQUIRE(svc.ready());

  // algorithms are listed by type, with their demangled type name
  CHECK(svc.ls<IntAlgorithm>() ==
        std::vector<std::string>{"algorithm_svc_test::AddOne", "algorithm_svc_test::Twice"});
  CHECK(svc.ls<DoubleAlgorithm>() == std::vector<std::string>{"algorithm_svc_test::Half"});
  CHECK(svc.ls<Algorithm<Input<float>, Output<float>>>().empty());

  // each get creates a new instance
  const auto add_one = svc.get<IntAlgorithm>("algorithm_svc_test::AddOne");
  const auto twice   = svc.get<IntAlgorithm>("algorithm_svc_test::Twice");
  const auto half    = svc.get<DoubleAlgorithm>("algorithm_svc_test::Half");
  REQUIRE(add_one != nullptr);
  REQUIRE(twice != nullptr);
  REQUIRE(half != nullptr);
  CHECK(add_one != svc.get<IntAlgorithm>("algorithm_svc_test::AddOne"));
  CHECK(add_one->name() == "AddOne");
  CHECK(twice->name() == "Twice");

  const int i = 3;
  int j       = 0;
  add_one->process({&i}, {&j});
  CHECK(j == 4);
  twice->process({&i}, {&j});
  CHECK(j == 6);

  const double x = 3.;
  double y       = 0.;
  half->process({&x}, {&y});
  CHECK(y == 1.5);

  // unknown names, types and invalid names are rejected
  CHECK_THROWS_WITH(svc.get<IntAlgorithm>("algorithm_svc_test::Half"),
                    ContainsSubstring("No factory with name algorithm_svc_test::Half and type"));
  CHECK_THROWS_WITH((svc.get<Algorithm<Input<float>, Output<float>>>("AddOne")),
                    ContainsSubstring("No factory with name AddOne"));
  CHECK_THROWS_WITH(svc.get<IntAlgorithm>(""), "Invalid name provided: ''");
}
