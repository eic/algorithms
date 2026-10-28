// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the non-narrowing type conversions used for properties
//
#include <algorithms/detail/upcast.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace algorithms::detail;

namespace {
enum class Color { kRed, kGreen };
enum Plain { kPlainA = 7 };
struct Custom {};
} // namespace

TEST_CASE("Unsigned integers upcast to uint64_t", "[upcast]") {
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<uint8_t>, uint64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<uint16_t>, uint64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<uint32_t>, uint64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<uint64_t>, uint64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<std::size_t>, uint64_t>);
}

TEST_CASE("Signed integers upcast to int64_t", "[upcast]") {
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<int8_t>, int64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<int16_t>, int64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<int32_t>, int64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<int64_t>, int64_t>);
}

TEST_CASE("Enums upcast to int64_t", "[upcast]") {
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<Color>, int64_t>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<Plain>, int64_t>);
}

TEST_CASE("Floating point types upcast to double", "[upcast]") {
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<float>, double>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<double>, double>);
}

TEST_CASE("String-like types upcast to std::string", "[upcast]") {
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<std::string>, std::string>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<const char*>, std::string>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<char*>, std::string>);
}

TEST_CASE("Booleans and other types are not upcast", "[upcast]") {
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<bool>, bool>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<Custom>, Custom>);
  STATIC_REQUIRE(std::is_same_v<upcast_type_t<std::vector<int>>, std::vector<int>>);
}

TEST_CASE("upcast converts values without changing them", "[upcast]") {
  CHECK(upcast(uint8_t{255}) == uint64_t{255});
  CHECK(upcast(int8_t{-128}) == int64_t{-128});
  CHECK(upcast(1.5f) == 1.5);
  CHECK(upcast("text") == std::string{"text"});
  CHECK(upcast(true) == true);
  CHECK(upcast(kPlainA) == int64_t{7});

  STATIC_REQUIRE(std::is_same_v<decltype(upcast(uint8_t{1})), uint64_t>);
  STATIC_REQUIRE(std::is_same_v<decltype(upcast(1.5f)), double>);
  STATIC_REQUIRE(std::is_same_v<decltype(upcast("text")), std::string>);
}
