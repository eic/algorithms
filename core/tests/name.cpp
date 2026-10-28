// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the NameMixin base class
//
#include <algorithms/name.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace algorithms;

TEST_CASE("NameMixin keeps a copy of its name and description", "[name]") {
  std::string name{"myName"};
  std::string description{"my description"};
  const NameMixin n{name, description};

  // modifying the originals must not affect the stored values
  name        = "changed";
  description = "changed";

  CHECK(n.name() == "myName");
  CHECK(n.description() == "my description");
}
