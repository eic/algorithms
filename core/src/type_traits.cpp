// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Compile-time checks of the argument type deduction in type_traits.h
#include <algorithms/type_traits.h>

#include <optional>
#include <type_traits>
#include <vector>

namespace {

using namespace algorithms;

struct Coll {};

// Normal data type T
static_assert(!is_vector_v<Coll> && !is_optional_v<Coll> && !is_vector_of_optional_v<Coll>);
static_assert(std::is_same_v<data_type_t<Coll>, Coll>);
static_assert(std::is_same_v<input_type_t<Coll>, gsl::not_null<const Coll*>>);
static_assert(std::is_same_v<output_type_t<Coll>, gsl::not_null<Coll*>>);

// Optional data type std::optional<T>
using OptColl = std::optional<Coll>;
static_assert(!is_vector_v<OptColl> && is_optional_v<OptColl> && !is_vector_of_optional_v<OptColl>);
static_assert(std::is_same_v<data_type_t<OptColl>, Coll>);
static_assert(std::is_same_v<input_type_t<OptColl>, const Coll*>);
static_assert(std::is_same_v<output_type_t<OptColl>, Coll*>);

// Vector of normal data std::vector<T>
using VecColl = std::vector<Coll>;
static_assert(is_vector_v<VecColl> && !is_optional_v<VecColl> && !is_vector_of_optional_v<VecColl>);
static_assert(std::is_same_v<data_type_t<VecColl>, Coll>);
static_assert(std::is_same_v<input_type_t<VecColl>, const std::vector<gsl::not_null<const Coll*>>>);
static_assert(std::is_same_v<output_type_t<VecColl>, const std::vector<gsl::not_null<Coll*>>>);

// Vector of optional data std::vector<std::optional<T>>
using VecOptColl = std::vector<std::optional<Coll>>;
static_assert(is_vector_v<VecOptColl> && !is_optional_v<VecOptColl> &&
              is_vector_of_optional_v<VecOptColl>);
static_assert(std::is_same_v<data_type_t<VecOptColl>, Coll>);
static_assert(std::is_same_v<input_type_t<VecOptColl>, const std::vector<const Coll*>>);
static_assert(std::is_same_v<output_type_t<VecOptColl>, const std::vector<Coll*>>);

// Same with a const-qualified data type, as commonly used for input collections
using VecOptConstColl = std::vector<std::optional<const Coll>>;
static_assert(is_vector_of_optional_v<VecOptConstColl>);
static_assert(std::is_same_v<data_type_t<VecOptConstColl>, const Coll>);
static_assert(std::is_same_v<input_type_t<VecOptConstColl>, const std::vector<const Coll*>>);

} // namespace
