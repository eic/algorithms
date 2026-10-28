// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for Configurable (PropertyMixin) and its self-registering Property<T>
//
#include <algorithms/property.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cstdint>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace algorithms;

namespace {

enum class Mode { kFast, kSlow };

class Config : public PropertyMixin {
public:
  Property<bool> m_bool{this, "bool", true, "a bool"};
  Property<int> m_int{this, "int", 3, "an int"};
  Property<unsigned> m_uint{this, "uint", 4u, "an unsigned int"};
  Property<float> m_float{this, "float", 1.5f, "a float"};
  Property<double> m_double{this, "double", "a double without default"};
  Property<std::string> m_string{this, "string", "text", "a string"};
  Property<Mode> m_mode{this, "mode", Mode::kFast, "an enum"};
  Property<std::vector<int>> m_ints{this, "ints", {1, 2, 3}, "a vector of ints"};
  Property<std::vector<std::string>> m_strings{this, "strings", {"a", "b"}, "a vector of strings"};
};

} // namespace

TEST_CASE("Properties register themselves with their owner", "[property]") {
  const Config c;
  const auto& props = c.getProperties();

  CHECK(props.size() == 9);
  CHECK(props.at("int").name() == "int");
  CHECK(props.at("int").description() == "an int");
  CHECK(props.at("double").description() == "a double without default");
}

TEST_CASE("Properties without default are missing until set", "[property]") {
  Config c;

  CHECK(c.hasProperty("int"));
  CHECK_FALSE(c.hasProperty("double"));
  CHECK_FALSE(c.hasProperty("unknown"));
  CHECK(c.missingProperties() == std::vector<std::string_view>{"double"});
  CHECK_THROWS_AS(c.validate(), PropertyError);
  CHECK_THROWS_WITH(c.validate(), R"(Missing properties: ["double"])");
  try {
    c.validate();
  } catch (const PropertyError& e) {
    CHECK(std::string{e.type()} == "algorithms::PropertyError");
  }

  c.setProperty("double", 2.5);
  CHECK(c.hasProperty("double"));
  CHECK(c.missingProperties().empty());
  CHECK_NOTHROW(c.validate());
}

TEST_CASE("Properties are set and retrieved through their upcast type", "[property]") {
  Config c;

  c.setProperty("bool", false);
  c.setProperty("int", short{5});
  c.setProperty("uint", uint8_t{6});
  c.setProperty("float", 2.5f);
  c.setProperty("double", 3.5);
  c.setProperty("string", "other");
  c.setProperty("mode", Mode::kSlow);
  c.setProperty("ints", std::vector<int>{4, 5});
  c.setProperty("strings", std::vector<std::string>{"c"});

  CHECK(c.getProperty<bool>("bool") == false);
  CHECK(c.getProperty<int64_t>("int") == 5);
  CHECK(c.getProperty<uint64_t>("uint") == 6);
  CHECK(c.getProperty<double>("float") == 2.5);
  CHECK(c.getProperty<double>("double") == 3.5);
  CHECK(c.getProperty<std::string>("string") == "other");
  CHECK(c.getProperty<int64_t>("mode") == static_cast<int64_t>(Mode::kSlow));
  CHECK(c.getProperty<std::vector<int>>("ints") == std::vector<int>{4, 5});
  CHECK(c.getProperty<std::vector<std::string>>("strings") == std::vector<std::string>{"c"});

  CHECK(c.m_bool == false);
  CHECK(c.m_int == 5);
  CHECK(c.m_uint == 6u);
  CHECK(c.m_float == 2.5f);
  CHECK(c.m_double == 3.5);
  CHECK(c.m_string == "other");
  CHECK(c.m_mode == Mode::kSlow);
}

TEST_CASE("Properties reject unknown names and wrong types", "[property]") {
  Config c;

  CHECK_THROWS_AS(c.setProperty("unknown", 1), std::out_of_range);
  CHECK_THROWS_AS(c.getProperty<int64_t>("unknown"), std::out_of_range);
  CHECK_THROWS_AS(c.setProperty("int", 1.5), std::bad_variant_access);
  CHECK_THROWS_AS(c.getProperty<int32_t>("int"), std::bad_variant_access);
}

TEST_CASE("Properties require a unique name and a valid owner", "[property]") {
  class Duplicate : public PropertyMixin {
    Property<int> m_a{this, "same", 1, ""};
    Property<int> m_b{this, "same", 2, ""};
  };
  CHECK_THROWS_WITH(Duplicate{}, "Duplicate property name: same");
  CHECK_THROWS_WITH((Configurable::Property<int>{nullptr, "orphan", "no owner"}),
                    "Attempting to create Property 'orphan' without valid owner");
}

TEST_CASE("Properties act as a const value", "[property]") {
  const Config c;

  const int i = c.m_int;
  CHECK(i == 3);
  CHECK(c.m_int.value() == 3);

  CHECK(c.m_int == 3);
  CHECK(3 == c.m_int);
  CHECK(c.m_int != 4);
  CHECK(c.m_int < 4);
  CHECK(c.m_int <= 3);
  CHECK(c.m_int > 2);
  CHECK(c.m_int >= 3);
  CHECK(c.m_int + 1 == 4);
  CHECK(c.m_int - 1 == 2);
  CHECK(c.m_int * 2 == 6);
  CHECK(c.m_int / 3 == 1);
  CHECK(c.m_float * 2.f == 3.f);
}

TEST_CASE("Properties can be printed and formatted", "[property]") {
  const Config c;

  std::ostringstream os;
  os << c.m_int << " " << c.m_string;
  CHECK(os.str() == "3 text");

  CHECK(fmt::format("{} {} {}", c.m_int, c.m_string, c.m_float) == "3 text 1.5");
  CHECK(fmt::format("{:>4}", c.m_int) == "   3");
  CHECK(fmt::format("{}", c.m_ints) == "[1, 2, 3]");
  CHECK(fmt::format("{}", c.m_strings) == R"(["a", "b"])");
}

namespace {
template <class P>
concept Collection = requires(const P& p) {
  p.begin();
  p.end();
  p.size();
  p.empty();
};
template <class P>
concept Streamable = requires(std::ostream& os, const P& p) { os << p; };
template <class T> using Prop = Configurable::Property<T>;
} // namespace

TEST_CASE("Properties only provide collection helpers for collections", "[property]") {
  STATIC_REQUIRE_FALSE(Collection<Prop<int>>);
  STATIC_REQUIRE_FALSE(Collection<Prop<double>>);
  STATIC_REQUIRE(Collection<Prop<std::string>>);
  STATIC_REQUIRE(Collection<Prop<std::vector<int>>>);

  STATIC_REQUIRE_FALSE(fmt::is_range<Prop<std::vector<int>>, char>::value);

  STATIC_REQUIRE(Streamable<Prop<int>>);
  STATIC_REQUIRE_FALSE(Streamable<Prop<Mode>>);
}

TEST_CASE("Properties of containers expose container access", "[property]") {
  Config c;
  const Config& cc = c;

  CHECK(cc.m_ints.size() == 3);
  CHECK_FALSE(cc.m_ints.empty());
  CHECK(cc.m_ints[1] == 2);
  CHECK(c.m_ints[2] == 3);
  CHECK(cc.m_string.length() == 4);
  CHECK(cc.m_string.find("xt") == 2);
  CHECK(cc.m_string[0] == 't');
  CHECK(std::vector<int>(cc.m_ints.begin(), cc.m_ints.end()) == std::vector<int>{1, 2, 3});
  CHECK(std::vector<int>(c.m_ints.begin(), c.m_ints.end()) == std::vector<int>{1, 2, 3});

  int sum = 0;
  for (int v : cc.m_ints) {
    sum += v;
  }
  CHECK(sum == 6);
}

TEST_CASE("Properties can be copied", "[property]") {
  const Config c;
  const auto copy{c.m_strings};
  CHECK(copy.value() == std::vector<std::string>{"a", "b"});
  CHECK(copy.name() == "strings");
  CHECK(copy.hasValue());
}

TEST_CASE("Properties are accessible through PropertyBase", "[property]") {
  Config c;
  auto& base = c.getProperties().at("uint");

  base.set(uint64_t{9});
  CHECK(c.m_uint == 9u);
  CHECK(std::get<uint64_t>(base.get()) == 9);
}

TEST_CASE("Properties can be set from lvalues", "[property]") {
  Config c;
  const int i = 7;
  float f     = 0.5f;
  std::string text{"lvalue"};
  const char* chars = "chars";

  c.setProperty("int", i);
  c.setProperty("float", f);
  c.setProperty("string", text);
  CHECK(c.m_int == 7);
  CHECK(c.m_float == 0.5f);
  CHECK(c.m_string == "lvalue");

  c.setProperty("string", chars);
  CHECK(c.m_string == "chars");
}
