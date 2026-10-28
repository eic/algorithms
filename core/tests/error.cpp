// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the Error exception base class
//
#include <algorithms/error.h>

#include <catch2/catch_test_macros.hpp>

#include <exception>
#include <string>
#include <type_traits>

using namespace algorithms;

TEST_CASE("Error carries a message and a default type", "[error]") {
  const Error e{"something went wrong"};
  CHECK(std::string{e.what()} == "something went wrong");
  CHECK(std::string{e.type()} == "algorithms::Error");
}

TEST_CASE("Error carries a custom type", "[error]") {
  const Error e{"bad value", "my::CustomError"};
  CHECK(std::string{e.what()} == "bad value");
  CHECK(std::string{e.type()} == "my::CustomError");
}

TEST_CASE("Error can be caught as std::exception", "[error]") {
  STATIC_REQUIRE(std::is_base_of_v<std::exception, Error>);
  try {
    throw Error{"thrown"};
  } catch (const std::exception& e) {
    CHECK(std::string{e.what()} == "thrown");
  }
}
