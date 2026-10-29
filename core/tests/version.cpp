// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the generated version header
//
#include <algorithms/version.h>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

using namespace algorithms;

// The macros must be usable in preprocessor conditionals
#if !defined(ALGORITHMS_VERSION) || ALGORITHMS_VERSION < ALGORITHMS_VERSION_CODE(2, 1, 0)
#error "ALGORITHMS_VERSION must be at least 2.1.0"
#endif

TEST_CASE("Version macros and constants agree", "[version]") {
  STATIC_CHECK(versionMajor == ALGORITHMS_VERSION_MAJOR);
  STATIC_CHECK(versionMinor == ALGORITHMS_VERSION_MINOR);
  STATIC_CHECK(versionPatch == ALGORITHMS_VERSION_PATCH);
  STATIC_CHECK(versionString == ALGORITHMS_VERSION_STRING);
  CHECK(fmt::format("{}.{}.{}", versionMajor, versionMinor, versionPatch) == versionString);
}

TEST_CASE("Version code is ordered like the version", "[version]") {
  STATIC_CHECK(ALGORITHMS_VERSION_CODE(2, 1, 0) < ALGORITHMS_VERSION_CODE(2, 1, 1));
  STATIC_CHECK(ALGORITHMS_VERSION_CODE(2, 1, 999) < ALGORITHMS_VERSION_CODE(2, 2, 0));
  STATIC_CHECK(ALGORITHMS_VERSION_CODE(2, 999, 999) < ALGORITHMS_VERSION_CODE(3, 0, 0));
}

TEST_CASE("versionAtLeast compares against the current version", "[version]") {
  STATIC_CHECK(versionAtLeast(versionMajor, versionMinor, versionPatch));
  STATIC_CHECK(versionAtLeast(versionMajor, versionMinor));
  STATIC_CHECK(versionAtLeast(versionMajor));
  STATIC_CHECK(!versionAtLeast(versionMajor, versionMinor, versionPatch + 1));
  STATIC_CHECK(!versionAtLeast(versionMajor, versionMinor + 1, 0));
  STATIC_CHECK(!versionAtLeast(versionMajor + 1, 0, 0));
  STATIC_CHECK(versionAtLeast(versionMajor - 1, versionMinor + 1, versionPatch + 1));
  STATIC_CHECK(versionAtLeast(0, 0, 0));
}
