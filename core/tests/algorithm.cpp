// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for Input<T...>, Output<T...> and the Algorithm base class, with required (T),
// optional (std::optional<T>) and vector (std::vector<T>) arguments.
//
#include <algorithms/algorithm.h>

#include <catch2/catch_test_macros.hpp>

#include <edm4hep/MCParticleCollection.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

using namespace algorithms;

namespace {

using MixedInput  = Input<int, std::optional<double>, std::vector<std::string>>;
using MixedOutput = Output<int, std::optional<double>, std::vector<std::string>>;

// Copies each input argument to the output argument of the same type
class MixedAlgorithm : public Algorithm<MixedInput, MixedOutput> {
public:
  static constexpr double kMissing = -1.;

  MixedAlgorithm()
      : Algorithm{"MixedAlgorithm",
                  {"inputInt", "inputDouble", "inputStrings"},
                  {"outputInt", "outputDouble", "outputStrings"},
                  "Copies each input argument to the matching output argument"} {}

  void init() final { m_initialized = true; }

  void process(const Input& input, const Output& output) const final {
    const auto& [int_in, double_in, strings_in]    = input;
    const auto& [int_out, double_out, strings_out] = output;

    *int_out = *int_in;
    if (double_out != nullptr) {
      *double_out = (double_in != nullptr) ? *double_in : kMissing;
    }
    for (std::size_t i = 0; i < std::min(strings_in.size(), strings_out.size()); ++i) {
      *strings_out[i] = *strings_in[i];
    }
  }

  bool initialized() const { return m_initialized; }

private:
  bool m_initialized = false;
};

} // namespace

TEST_CASE("Input and Output combine all argument types", "[algorithm]") {
  STATIC_REQUIRE(MixedInput::kSize == 3);
  STATIC_REQUIRE(MixedOutput::kSize == 3);

  STATIC_REQUIRE(std::is_same_v<MixedInput::data_type,
                                std::tuple<int, std::optional<double>, std::vector<std::string>>>);
  STATIC_REQUIRE(std::is_same_v<MixedOutput::data_type,
                                std::tuple<int, std::optional<double>, std::vector<std::string>>>);

  STATIC_REQUIRE(
      std::is_same_v<MixedInput::value_type,
                     std::tuple<gsl::not_null<const int*>, const double*,
                                const std::vector<gsl::not_null<const std::string*>>>>);
  STATIC_REQUIRE(std::is_same_v<MixedOutput::value_type,
                                std::tuple<gsl::not_null<int*>, double*,
                                           const std::vector<gsl::not_null<std::string*>>>>);
  STATIC_REQUIRE(std::is_base_of_v<MixedInput::value_type, MixedInput>);
  STATIC_REQUIRE(std::is_base_of_v<MixedOutput::value_type, MixedOutput>);

  STATIC_REQUIRE(std::is_same_v<MixedInput::key_type, std::array<const std::string, 3>>);
  STATIC_REQUIRE(std::is_same_v<MixedOutput::key_type, std::array<const std::string, 3>>);
}

TEST_CASE("Input and Output types are recognized", "[algorithm]") {
  STATIC_REQUIRE(is_input_v<MixedInput>);
  STATIC_REQUIRE_FALSE(is_output_v<MixedInput>);
  STATIC_REQUIRE(is_output_v<MixedOutput>);
  STATIC_REQUIRE_FALSE(is_input_v<MixedOutput>);
  STATIC_REQUIRE_FALSE(is_input_v<MixedInput::value_type>);
  STATIC_REQUIRE_FALSE(is_output_v<MixedOutput::value_type>);
  STATIC_REQUIRE_FALSE(is_input_v<int>);
  STATIC_REQUIRE_FALSE(is_output_v<int>);
}

TEST_CASE("Algorithm exposes its argument types", "[algorithm]") {
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::input_type, MixedInput>);
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::output_type, MixedOutput>);
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::algorithm_type,
                                Algorithm<MixedInput, MixedOutput>>);
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::Input, MixedInput::value_type>);
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::Output, MixedOutput::value_type>);
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::InputNames, MixedInput::key_type>);
  STATIC_REQUIRE(std::is_same_v<MixedAlgorithm::OutputNames, MixedOutput::key_type>);
  STATIC_REQUIRE(std::is_base_of_v<AlgorithmBase, MixedAlgorithm>);
  STATIC_REQUIRE(std::has_virtual_destructor_v<MixedAlgorithm::algorithm_type>);
}

TEST_CASE("Algorithm keeps its name, description and argument names", "[algorithm]") {
  const MixedAlgorithm algo;

  CHECK(algo.name() == "MixedAlgorithm");
  CHECK(algo.description() == "Copies each input argument to the matching output argument");
  CHECK(algo.inputNames() == MixedAlgorithm::InputNames{"inputInt", "inputDouble", "inputStrings"});
  CHECK(algo.outputNames() ==
        MixedAlgorithm::OutputNames{"outputInt", "outputDouble", "outputStrings"});
}

TEST_CASE("Algorithm is configurable and has a log level", "[algorithm]") {
  class ConfigurableAlgorithm : public Algorithm<Input<int>, Output<int>> {
  public:
    ConfigurableAlgorithm() : Algorithm{"ConfigurableAlgorithm", {"in"}, {"out"}, ""} {}
    void process(const Input& input, const Output& output) const final {
      *std::get<0>(output) = *std::get<0>(input) * m_factor;
    }

  private:
    Property<int> m_factor{this, "factor", 2, "Multiplication factor"};
  };

  ConfigurableAlgorithm algo;
  algo.level(LogLevel::kDebug);
  CHECK(algo.level() == LogLevel::kDebug);

  const int in = 21;
  int out      = 0;
  algo.process({&in}, {&out});
  CHECK(out == 42);

  algo.setProperty("factor", 3);
  algo.process({&in}, {&out});
  CHECK(out == 63);
}

TEST_CASE("Algorithm base class has default init and process", "[algorithm]") {
  using Base = Algorithm<Input<int>, Output<int>>;
  auto algo  = std::make_unique<Base>("Base", Base::InputNames{"in"}, Base::OutputNames{"out"},
                                     "Base algorithm without overrides");

  const int in = 1;
  int out      = 0;
  algo->init();
  algo->process({&in}, {&out});
  CHECK(out == 0);
}

TEST_CASE("Algorithm init can be overridden", "[algorithm]") {
  MixedAlgorithm algo;
  CHECK_FALSE(algo.initialized());
  algo.init();
  CHECK(algo.initialized());
}

TEST_CASE("Algorithm processes all argument types", "[algorithm]") {
  const MixedAlgorithm algo;

  const int int_in       = 42;
  const double double_in = 3.5;
  const std::string a{"a"};
  const std::string b{"b"};

  int int_out       = 0;
  double double_out = 0.;
  std::string x;
  std::string y;

  SECTION("all arguments present") {
    algo.process({&int_in, &double_in, {&a, &b}}, {&int_out, &double_out, {&x, &y}});
    CHECK(int_out == 42);
    CHECK(double_out == 3.5);
    CHECK(x == "a");
    CHECK(y == "b");
  }

  SECTION("optional input absent") {
    algo.process({&int_in, nullptr, {&a, &b}}, {&int_out, &double_out, {&x, &y}});
    CHECK(int_out == 42);
    CHECK(double_out == MixedAlgorithm::kMissing);
    CHECK(x == "a");
    CHECK(y == "b");
  }

  SECTION("optional output absent") {
    algo.process({&int_in, &double_in, {&a, &b}}, {&int_out, nullptr, {&x, &y}});
    CHECK(int_out == 42);
    CHECK(double_out == 0.);
    CHECK(x == "a");
    CHECK(y == "b");
  }

  SECTION("vector arguments empty") {
    algo.process({&int_in, &double_in, {}}, {&int_out, &double_out, {}});
    CHECK(int_out == 42);
    CHECK(double_out == 3.5);
    CHECK(x.empty());
    CHECK(y.empty());
  }

  SECTION("vector arguments with a single element") {
    algo.process({&int_in, &double_in, {&b}}, {&int_out, &double_out, {&x}});
    CHECK(x == "b");
    CHECK(y.empty());
  }
}

namespace {

using Particles = edm4hep::MCParticleCollection;

// Merges particles from a required, an optional, and a vector of collections
class MergeParticles
    : public Algorithm<Input<Particles, std::optional<Particles>, std::vector<Particles>>,
                       Output<Particles>> {
public:
  MergeParticles()
      : Algorithm{"MergeParticles",
                  {"requiredParticles", "optionalParticles", "otherParticles"},
                  {"mergedParticles"},
                  "Merges particles from all input collections"} {}

  void process(const Input& input, const Output& output) const final {
    const auto& [required, optional, others] = input;
    const auto& [merged]                     = output;

    copy(*required, *merged);
    if (optional != nullptr) {
      copy(*optional, *merged);
    }
    for (const auto& other : others) {
      copy(*other, *merged);
    }
  }

private:
  static void copy(const Particles& from, Particles& to) {
    for (const auto& particle : from) {
      to.create().setPDG(particle.getPDG());
    }
  }
};

Particles makeParticles(std::initializer_list<int> pdgs) {
  Particles particles;
  for (int pdg : pdgs) {
    particles.create().setPDG(pdg);
  }
  return particles;
}

std::vector<int> pdgs(const Particles& particles) {
  std::vector<int> ret;
  for (const auto& particle : particles) {
    ret.push_back(particle.getPDG());
  }
  return ret;
}

} // namespace

TEST_CASE("Algorithm processes all argument types of EDM collections", "[algorithm][edm]") {
  const MergeParticles algo;

  const auto required = makeParticles({11});
  const auto optional = makeParticles({22, 22});
  const auto other1   = makeParticles({2212});
  const auto other2   = makeParticles({2112, -211});

  Particles merged;

  SECTION("all arguments present") {
    algo.process({&required, &optional, {&other1, &other2}}, {&merged});
    CHECK(pdgs(merged) == std::vector<int>{11, 22, 22, 2212, 2112, -211});
  }

  SECTION("optional input absent") {
    algo.process({&required, nullptr, {&other1, &other2}}, {&merged});
    CHECK(pdgs(merged) == std::vector<int>{11, 2212, 2112, -211});
  }

  SECTION("vector input empty") {
    algo.process({&required, &optional, {}}, {&merged});
    CHECK(pdgs(merged) == std::vector<int>{11, 22, 22});
  }
}
