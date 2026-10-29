// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Wouter Deconinck, Sylvester Joosten
//
// Algorithm base class, defined as a template with a tuple of Input<> and Output<>
// parameters.
//
// Known types are:
//   - Normal data type T
//   - Optional data type std::optional<T>
//   - Vector of normal data std::vector<T>
//   - Vector of optional data std::vector<std::optional<T>>
//
// For input data, this then selects:
//   - T                   --> gsl::not_null<const T*> (NOT allowed to be null)
//   - optional<T>         --> const T* (allowed to be null)
//   - vector<T>           --> std::vector<gsl::not_null<const T*>> (N arguments, NOT allowed
//                                                                   to be null, but can be
//                                                                   zero length)
//   - vector<optional<T>> --> std::vector<const T*> (N arguments, each allowed to be null,
//                                                    and can be zero length)
//
// Same for output data, but replace `const T*` with `T*` (mutable) everywhere.
//
// The ::process() algorithm is then provided with a tuple of both the input and the
// output pointers according to this scheme.
//
// Finally, provides provides utility traits to determine if a type Input<T...> or Output<T...> are
// an Input or Output Type (is_input_v<U> and is_output_v<U>)
//
#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include <algorithms/detail/demangle.h>
#include <algorithms/logger.h>
#include <algorithms/name.h>
#include <algorithms/property.h>
#include <algorithms/service.h>
#include <algorithms/type_traits.h>

namespace algorithms {

// T should either be the desired input type, a std::vector<> of the desired input type,
// a std::optional<> of the desired input type, or a std::vector<std::optional<>> of the
// desired input type
template <class... T> struct Input : std::tuple<input_type_t<T>...> {
  constexpr static const size_t kSize = sizeof...(T);
  using value_type                    = std::tuple<input_type_t<T>...>;
  using data_type                     = std::tuple<T...>;
  using key_type                      = std::array<const std::string, kSize>;
};
template <class... T> struct Output : std::tuple<output_type_t<T>...> {
  constexpr static const size_t kSize = sizeof...(T);
  using value_type                    = std::tuple<output_type_t<T>...>;
  using data_type                     = std::tuple<T...>;
  using key_type                      = std::array<const std::string, kSize>;
};

class AlgorithmBase : public PropertyMixin, public LoggerMixin, public NameMixin {
public:
  AlgorithmBase(std::string_view name, std::string_view description)
      : LoggerMixin(name), NameMixin(name, description) {}
};

// TODO: C++20 Concepts version for better error handling
template <class InputType, class OutputType> class Algorithm : public AlgorithmBase {
public:
  using input_type     = InputType;
  using output_type    = OutputType;
  using algorithm_type = Algorithm;
  using Input          = typename input_type::value_type;
  using Output         = typename output_type::value_type;
  using InputNames     = typename input_type::key_type;
  using OutputNames    = typename output_type::key_type;

  Algorithm(std::string_view name, const InputNames& input_names, const OutputNames& output_names,
            std::string_view description)
      : AlgorithmBase(name, description)
      , m_input_names{input_names}
      , m_output_names{output_names} {}

  virtual ~Algorithm() {}
  virtual void init() {}
  virtual void process(const Input&, const Output&) const {}

  const InputNames& inputNames() const { return m_input_names; }
  const OutputNames& outputNames() const { return m_output_names; }

private:
  const InputNames m_input_names;
  const OutputNames m_output_names;
};

// Algorithm service that stores factories for different algorithms indexed by their
// underlying algorithm_type. This allows a framework to only need to know about every
// algorithm_type (algorithm signature) once, which is sufficient to constrain all
// compile-time code needed.
class AlgorithmSvc : public LoggedService<AlgorithmSvc> {
public:
  void init() {}

  // Add a new factory for an algorithm with a specified type. The full demangled type name
  // will be used as identifier for the factory
  template <class Algo> void add() {
    using AlgoType = typename Algo::algorithm_type;
    std::lock_guard<std::mutex> lock{m_mutex};
    auto& factories = m_factories[typeid(AlgoType)];
    // do nothing if already there
    factories.try_emplace(detail::demangledName<Algo>(),
                          []() -> std::unique_ptr<AlgorithmBase> { return std::make_unique<Algo>(); });
  }

  // Get a new owning pointer to an instance of an algorithm.
  // Throws if the resource isn't available, or if we aren't fully ready yet.
  template <class AlgoType> std::unique_ptr<AlgoType> get(std::string_view name) const {
    ensureReady();
    std::lock_guard<std::mutex> lock{m_mutex};
    const auto* factory = find<AlgoType>(name);
    if (factory == nullptr) {
      raise(fmt::format("No factory with name {} and type {} registered with the AlgorithmSvc",
                        name, detail::demangledName<AlgoType>()));
    }
    // This creates an object and gives ownership to the unique_ptr
    return std::unique_ptr<AlgoType>(static_cast<AlgoType*>((*factory)().release()));
  }
  // Return a vector of the names of all available algorithms of a certain type
  // Just return an empty vector if no names are present (don't throw any exceptions
  // for missing entries here).
  // Throws if we aren't fully ready yet.
  template <class AlgoType> std::vector<std::string> ls() const {
    ensureReady();
    std::lock_guard<std::mutex> lock{m_mutex};
    std::vector<std::string> ret;
    if (const auto it = m_factories.find(typeid(AlgoType)); it != m_factories.end()) {
      for (const auto& [name, factory] : it->second) {
        ret.push_back(name);
      }
    }
    return ret;
  }

private:
  using factory_type = std::function<std::unique_ptr<AlgorithmBase>()>;

  // Find the factory of type AlgoType with a specific name, or nullptr if there is none.
  // Throws if the name is invalid
  template <class AlgoType> const factory_type* find(std::string_view name) const {
    if (name.empty()) {
      raise(fmt::format("Invalid name provided: '{}'", name));
    }
    const auto it = m_factories.find(typeid(AlgoType));
    if (it == m_factories.end()) {
      return nullptr;
    }
    const auto factory = it->second.find(name);
    return (factory != it->second.end()) ? &factory->second : nullptr;
  }
  // Throw an exception if we are not ready as factory to avoid giving incomplete factory
  // information to the caller
  void ensureReady() const {
    if (!ready()) {
      raise("Attempt to use AlgorithmSvc, but service not yet marked as ready");
    }
  }

  mutable std::mutex m_mutex;
  std::map<std::type_index, std::map<std::string, factory_type, std::less<>>> m_factories;

  ALGORITHMS_DEFINE_LOGGED_SERVICE(AlgorithmSvc)
};

namespace detail {
  template <class T> struct is_input : std::false_type {};
  template <class... T> struct is_input<Input<T...>> : std::true_type {};
  template <class T> struct is_output : std::false_type {};
  template <class... T> struct is_output<Output<T...>> : std::true_type {};
} // namespace detail
template <class T> constexpr bool is_input_v  = detail::is_input<T>::value;
template <class T> constexpr bool is_output_v = detail::is_output<T>::value;

} // namespace algorithms

