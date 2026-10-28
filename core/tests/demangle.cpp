// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for demangled type names
//
#include <algorithms/detail/demangle.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace algorithms::detail;

namespace demangle_test {
struct Named {};
template <class T> struct Templated {};
} // namespace demangle_test

TEST_CASE("demangledName returns readable type names", "[demangle]") {
  CHECK(demangledName<int>() == "int");
  CHECK(demangledName<double>() == "double");
  CHECK(demangledName<demangle_test::Named>() == "demangle_test::Named");
  CHECK(demangledName<demangle_test::Templated<int>>() == "demangle_test::Templated<int>");
  CHECK(demangledName<std::vector<int>>().starts_with("std::vector<int"));
}
