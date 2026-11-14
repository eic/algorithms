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

#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
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
  CHECK_THROWS_AS(c.setProperty("int", 1.5), PropertyError);
  CHECK_THROWS_AS(c.setProperty("int", "text"), PropertyError);
  CHECK_THROWS_AS(c.setProperty("bool", 1), PropertyError);
  CHECK_THROWS_AS(c.setProperty("int", true), PropertyError);
  CHECK_THROWS_AS(c.setProperty("ints", std::vector<double>{1.5}), PropertyError);
  CHECK_THROWS_AS(c.getProperty<bool>("int"), PropertyError);
  CHECK_THROWS_WITH(c.setProperty("int", 1.5),
                    "Property 'int' cannot be set from a value of this type");
}

TEST_CASE("Properties convert values without loss", "[property]") {
  Config c;

  // integers of other sizes and signedness, within range
  c.setProperty("uint", 7);
  CHECK(c.m_uint == 7u);
  c.setProperty("int", uint64_t{8});
  CHECK(c.m_int == 8);
  CHECK(c.getProperty<int32_t>("int") == 8);
  CHECK(c.getProperty<uint32_t>("int") == 8u);

  // integers to floating point
  c.setProperty("double", 2);
  CHECK(c.m_double == 2.0);
  c.setProperty("float", 3);
  CHECK(c.m_float == 3.0f);

  // element-wise for vectors
  c.setProperty("ints", std::vector<int64_t>{4, 5});
  CHECK(c.m_ints.value() == std::vector<int>{4, 5});
  CHECK(c.getProperty<std::vector<double>>("ints") == std::vector<double>{4., 5.});

  // out of range for the upcast or the declared type
  CHECK_THROWS_AS(c.setProperty("uint", -1), PropertyError);
  CHECK_THROWS_AS(c.setProperty("ints", std::vector<int64_t>{int64_t{1} << 40}), PropertyError);
  CHECK(c.getProperty<int32_t>("uint") == 7);
  c.setProperty("int", -1);
  CHECK_THROWS_AS(c.getProperty<uint32_t>("int"), PropertyError);
}

TEST_CASE("Properties check the range of their declared type", "[property]") {
  class Narrow : public PropertyMixin {
  public:
    Property<uint8_t> m_byte{this, "byte", 1, ""};
    Property<int16_t> m_short{this, "short", 1, ""};
  } n;

  n.setProperty("byte", 255);
  CHECK(n.m_byte == 255);
  CHECK_THROWS_WITH(n.setProperty("byte", 256), "Property 'byte': value 256 out of range");
  CHECK_THROWS_AS(n.setProperty("short", -40000), PropertyError);
  CHECK(n.m_byte == 255);
}

TEST_CASE("Properties without default are value-initialized", "[property]") {
  const Config c;
  CHECK_FALSE(c.m_double.hasValue());
  CHECK(c.m_double.value() == 0.0);
  CHECK(std::get<double>(c.getProperties().at("double").get()) == 0.0);
}

TEST_CASE("Configurables cannot be copied or moved", "[property]") {
  STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Configurable>);
  STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Configurable>);
  STATIC_REQUIRE_FALSE(std::is_move_constructible_v<Configurable>);
  STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Config>);
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

namespace {

enum class Sigma { kAbs, kRel };
std::ostream& operator<<(std::ostream& os, Sigma s) {
  return os << (s == Sigma::kAbs ? "abs" : "rel");
}
std::istream& operator>>(std::istream& is, Sigma& s) {
  std::string name;
  is >> name;
  if (name == "abs") {
    s = Sigma::kAbs;
  } else if (name == "rel") {
    s = Sigma::kRel;
  } else {
    is.setstate(std::ios::failbit);
  }
  return is;
}

struct Range {
  double min{0};
  double max{0};
  bool operator==(const Range&) const = default;
};
std::ostream& operator<<(std::ostream& os, const Range& r) { return os << r.min << ":" << r.max; }
std::istream& operator>>(std::istream& is, Range& r) {
  char colon{};
  is >> r.min >> colon >> r.max;
  if (colon != ':') {
    is.setstate(std::ios::failbit);
  }
  return is;
}

class Extended : public PropertyMixin {
public:
  Property<uint8_t> m_byte{this, "byte", 1, ""};
  Property<float> m_float{this, "float", 0.1f, ""};
  Property<double> m_double{this, "double", 0.03, ""};
  Property<Sigma> m_sigma{this, "sigma", Sigma::kAbs, "enum with stream operators"};
  Property<Mode> m_mode{this, "mode", Mode::kSlow, "enum without stream operators"};
  Property<std::vector<float>> m_floats{this, "floats", {0.5f, 1.5f}, ""};
  Property<std::array<double, 2>> m_pair{this, "pair", {1., 2.}, ""};
  Property<std::array<int, 2>> m_ipair{this, "ipair", {3, 4}, ""};
  Property<Range> m_range{this, "range", Range{1., 2.}, "streamable struct"};
};

} // namespace

TEST_CASE("Properties are represented as strings", "[property]") {
  const Config c;
  CHECK(c.getPropertyString("bool") == "true");
  CHECK(c.getPropertyString("int") == "3");
  CHECK(c.getPropertyString("float") == "1.5");
  CHECK(c.getPropertyString("string") == "text");
  CHECK(c.getPropertyString("ints") == "1,2,3");
  CHECK(c.getPropertyString("strings") == "a,b");

  const Extended e;
  CHECK(e.getPropertyString("byte") == "1");
  CHECK(e.getPropertyString("float") == "0.1");
  CHECK(e.getPropertyString("double") == "0.03");
  CHECK(e.getPropertyString("sigma") == "abs");
  CHECK(e.getPropertyString("mode") == "1");
  CHECK(e.getPropertyString("floats") == "0.5,1.5");
  CHECK(e.getPropertyString("pair") == "1,2");
  CHECK(e.getPropertyString("range") == "1:2");
}

TEST_CASE("Properties are parsed from strings", "[property]") {
  Config c;
  c.setPropertyString("bool", "false");
  c.setPropertyString("int", " -4 ");
  c.setPropertyString("double", "1e-3");
  c.setPropertyString("string", " spaced ");
  c.setPropertyString("ints", "4, 5");
  c.setPropertyString("strings", "x, y");
  CHECK(c.m_bool == false);
  CHECK(c.m_int == -4);
  CHECK(c.m_double == 1e-3);
  CHECK(c.m_string == " spaced ");
  CHECK(c.m_ints.value() == std::vector<int>{4, 5});
  CHECK(c.m_strings.value() == std::vector<std::string>{"x", "y"});
  c.setPropertyString("ints", "");
  CHECK(c.m_ints.empty());

  Extended e;
  e.setPropertyString("byte", "2");
  CHECK(e.m_byte == 2); // a number, not the character '2'
  e.setPropertyString("sigma", "rel");
  CHECK(e.m_sigma == Sigma::kRel);
  e.setPropertyString("mode", "0");
  CHECK(e.m_mode == Mode::kFast);
  e.setPropertyString("pair", "5,6");
  CHECK(e.m_pair.value() == std::array<double, 2>{5., 6.});
  e.setPropertyString("range", "3:4");
  CHECK(e.m_range.value() == Range{3., 4.});

  CHECK_THROWS_WITH(c.setPropertyString("int", "1.5"), "Property 'int': cannot parse '1.5'");
  CHECK_THROWS_AS(c.setPropertyString("int", "abc"), PropertyError);
  CHECK_THROWS_AS(c.setPropertyString("bool", "yes"), PropertyError);
  CHECK_THROWS_AS(e.setPropertyString("byte", "256"), PropertyError);
  CHECK_THROWS_AS(e.setPropertyString("sigma", "relative"), PropertyError);
  CHECK_THROWS_AS(e.setPropertyString("pair", "1"), PropertyError);
  CHECK_THROWS_AS(e.setPropertyString("range", "3-4"), PropertyError);
  // failed parsing leaves the value unchanged
  CHECK(c.m_int == -4);
  CHECK(e.m_pair.value() == std::array<double, 2>{5., 6.});
}

TEST_CASE("Properties of further types map onto PropertyValue alternatives", "[property]") {
  STATIC_REQUIRE(std::is_same_v<Prop<std::vector<float>>::impl_type, std::vector<double>>);
  STATIC_REQUIRE(std::is_same_v<Prop<std::array<double, 2>>::impl_type, std::vector<double>>);
  STATIC_REQUIRE(std::is_same_v<Prop<std::array<int, 2>>::impl_type, std::vector<int64_t>>);
  STATIC_REQUIRE(std::is_same_v<Prop<Range>::impl_type, std::string>);
  // unchanged representations
  STATIC_REQUIRE(std::is_same_v<Prop<std::vector<int>>::impl_type, std::vector<int32_t>>);
  STATIC_REQUIRE(std::is_same_v<Prop<Sigma>::impl_type, int64_t>);

  Extended e;
  CHECK(std::get<std::vector<double>>(e.getProperties().at("pair").get()) ==
        std::vector<double>{1., 2.});
  CHECK(std::get<std::string>(e.getProperties().at("range").get()) == "1:2");

  e.setProperty("pair", std::vector<double>{7., 8.});
  CHECK(e.m_pair.value() == std::array<double, 2>{7., 8.});
  e.setProperty("pair", std::array<double, 2>{9., 10.});
  CHECK(e.m_pair.value() == std::array<double, 2>{9., 10.});
  CHECK_THROWS_WITH(e.setProperty("pair", std::vector<double>{1., 2., 3.}),
                    "Property 'pair': 3 elements given, 2 required");
  e.setProperty("floats", std::vector<double>{2.5});
  CHECK(e.m_floats.value() == std::vector<float>{2.5f});
  e.setProperty("range", Range{5., 6.});
  CHECK(e.m_range.value() == Range{5., 6.});
  e.setProperty("range", std::string{"7:8"});
  CHECK(e.m_range.value() == Range{7., 8.});
  CHECK_THROWS_AS(e.setProperty("range", std::string{"bad"}), PropertyError);
}
