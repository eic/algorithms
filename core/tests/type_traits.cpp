// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the type traits that map algorithm argument types to pointer types:
//   - T              --> exactly one argument, NOT allowed to be null
//   - std::optional<T> --> exactly one argument, allowed to be null
//   - std::vector<T>   --> N >= 0 arguments, NOT allowed to be null
//   - std::vector<std::optional<T>> --> N >= 0 arguments, each allowed to be null
// Inputs are passed as pointers to const, outputs as pointers to mutable data.
//
#include <algorithms/type_traits.h>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <edm4hep/MCParticleCollection.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace algorithms;

namespace {

// Non-default allocator, to check that vectors are recognized regardless of allocator
template <class T> struct TestAllocator : std::allocator<T> {};

// Type of the element a (smart) pointer refers to, including its constness
template <class P> using pointee_t = std::remove_reference_t<decltype(*std::declval<P>())>;

} // namespace

// Argument types must work for any data type, from fundamental types to EDM collections
#define ARGUMENT_TEST_TYPES int, double, std::string, edm4hep::MCParticleCollection

TEMPLATE_TEST_CASE("Argument types are classified by the type traits", "[type_traits]",
                   ARGUMENT_TEST_TYPES) {
  using T = TestType;

  SECTION("required") {
    STATIC_REQUIRE_FALSE(is_optional_v<T>);
    STATIC_REQUIRE_FALSE(is_vector_v<T>);
    STATIC_REQUIRE_FALSE(is_vector_of_optional_v<T>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<T>, T>);
  }
  SECTION("optional") {
    STATIC_REQUIRE(is_optional_v<std::optional<T>>);
    STATIC_REQUIRE_FALSE(is_vector_v<std::optional<T>>);
    STATIC_REQUIRE_FALSE(is_vector_of_optional_v<std::optional<T>>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<std::optional<T>>, T>);
  }
  SECTION("vector") {
    STATIC_REQUIRE(is_vector_v<std::vector<T>>);
    STATIC_REQUIRE_FALSE(is_optional_v<std::vector<T>>);
    STATIC_REQUIRE_FALSE(is_vector_of_optional_v<std::vector<T>>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<std::vector<T>>, T>);
  }
  SECTION("vector with custom allocator") {
    using V = std::vector<T, TestAllocator<T>>;
    STATIC_REQUIRE(is_vector_v<V>);
    STATIC_REQUIRE_FALSE(is_optional_v<V>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<V>, T>);
  }
  SECTION("vector of optional") {
    using V = std::vector<std::optional<T>>;
    STATIC_REQUIRE(is_vector_v<V>);
    STATIC_REQUIRE_FALSE(is_optional_v<V>);
    STATIC_REQUIRE(is_vector_of_optional_v<V>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<V>, T>);
  }
  SECTION("vector of optional with custom allocator") {
    using V = std::vector<std::optional<T>, TestAllocator<std::optional<T>>>;
    STATIC_REQUIRE(is_vector_v<V>);
    STATIC_REQUIRE(is_vector_of_optional_v<V>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<V>, T>);
  }
  SECTION("vector of optional const") {
    using V = std::vector<std::optional<const T>>;
    STATIC_REQUIRE(is_vector_of_optional_v<V>);
    STATIC_REQUIRE(std::is_same_v<data_type_t<V>, const T>);
  }
}

TEMPLATE_TEST_CASE("Input arguments map to read-only pointers", "[type_traits]",
                   ARGUMENT_TEST_TYPES) {
  using T = TestType;

  SECTION("required") {
    using I = input_type_t<T>;
    STATIC_REQUIRE(std::is_same_v<I, gsl::not_null<const T*>>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<I>, const T>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<I, std::nullptr_t>);
  }
  SECTION("optional") {
    using I = input_type_t<std::optional<T>>;
    STATIC_REQUIRE(std::is_same_v<I, const T*>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<I>, const T>);
    STATIC_REQUIRE(std::is_constructible_v<I, std::nullptr_t>);
  }
  SECTION("vector") {
    using I = input_type_t<std::vector<T>>;
    using E = typename std::remove_const_t<I>::value_type;
    STATIC_REQUIRE(std::is_same_v<I, const std::vector<gsl::not_null<const T*>>>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<E>, const T>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<E, std::nullptr_t>);
  }
  SECTION("vector of optional") {
    using I = input_type_t<std::vector<std::optional<T>>>;
    using E = typename std::remove_const_t<I>::value_type;
    STATIC_REQUIRE(std::is_same_v<I, const std::vector<const T*>>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<E>, const T>);
    STATIC_REQUIRE(std::is_constructible_v<E, std::nullptr_t>);
    STATIC_REQUIRE(std::is_same_v<input_type_t<std::vector<std::optional<const T>>>,
                                  const std::vector<const T*>>);
  }
}

TEMPLATE_TEST_CASE("Output arguments map to mutable pointers", "[type_traits]",
                   ARGUMENT_TEST_TYPES) {
  using T = TestType;

  SECTION("required") {
    using O = output_type_t<T>;
    STATIC_REQUIRE(std::is_same_v<O, gsl::not_null<T*>>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<O>, T>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<O, std::nullptr_t>);
  }
  SECTION("optional") {
    using O = output_type_t<std::optional<T>>;
    STATIC_REQUIRE(std::is_same_v<O, T*>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<O>, T>);
    STATIC_REQUIRE(std::is_constructible_v<O, std::nullptr_t>);
  }
  SECTION("vector") {
    using O = output_type_t<std::vector<T>>;
    using E = typename std::remove_const_t<O>::value_type;
    STATIC_REQUIRE(std::is_same_v<O, const std::vector<gsl::not_null<T*>>>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<E>, T>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<E, std::nullptr_t>);
  }
  SECTION("vector of optional") {
    using O = output_type_t<std::vector<std::optional<T>>>;
    using E = typename std::remove_const_t<O>::value_type;
    STATIC_REQUIRE(std::is_same_v<O, const std::vector<T*>>);
    STATIC_REQUIRE(std::is_same_v<pointee_t<E>, T>);
    STATIC_REQUIRE(std::is_constructible_v<E, std::nullptr_t>);
  }
}
