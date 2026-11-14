// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Wouter Deconinck, Sylvester Joosten
//
// Defines the Configurable base class and related PropertyMixin
// These base classes provide access to the Property<T> object, a self-registering
// configurable property that acts as a bare object T from a performance point-of-view
//
#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithms/detail/property_string.h>
#include <algorithms/detail/upcast.h>
#include <algorithms/error.h>
#include <algorithms/name.h>

namespace algorithms {

class PropertyError : public Error {
public:
  PropertyError(std::string_view msg) : Error{msg, "algorithms::PropertyError"} {}
};

// Data types supported for Properties, defined as std::variant. This allows for
// automatic Property registration with the calling framework, and enables compile-time
// type errors.
using PropertyValue = std::variant<bool, uint32_t, int32_t, uint64_t, int64_t, double, std::string,
                                   std::vector<bool>, std::vector<uint32_t>, std::vector<int32_t>,
                                   std::vector<uint64_t>, std::vector<int64_t>, std::vector<double>,
                                   std::vector<std::string>>;

namespace detail {
  template <class T> struct is_std_vector : std::false_type {};
  template <class T, class A> struct is_std_vector<std::vector<T, A>> : std::true_type {};

  // Lossless conversion between PropertyValue alternatives: identical types, integers to
  // integers within range, integers to floating point, and element-wise for vectors.
  // bool only converts to and from bool. Returns std::nullopt if the conversion would
  // change the type category or lose information.
  template <class To, class From> std::optional<To> convert_lossless(const From& from) {
    if constexpr (std::is_same_v<To, From>) {
      return from;
    } else if constexpr (std::is_same_v<To, bool> || std::is_same_v<From, bool>) {
      return std::nullopt;
    } else if constexpr (std::is_integral_v<To> && std::is_integral_v<From>) {
      if (std::in_range<To>(from)) {
        return static_cast<To>(from);
      }
      return std::nullopt;
    } else if constexpr (std::is_floating_point_v<To> && std::is_arithmetic_v<From>) {
      return static_cast<To>(from);
    } else if constexpr (is_std_vector<To>::value && is_std_vector<From>::value) {
      To to;
      to.reserve(from.size());
      for (const auto& element : from) {
        const auto converted = convert_lossless<typename To::value_type>(
            static_cast<typename From::value_type>(element));
        if (!converted) {
          return std::nullopt;
        }
        to.push_back(*converted);
      }
      return to;
    } else {
      return std::nullopt;
    }
  }

  template <class To> std::optional<To> convert_lossless(const PropertyValue& value) {
    return std::visit([](const auto& from) { return convert_lossless<To>(from); }, value);
  }

  template <class T, class Variant> struct is_variant_alternative : std::false_type {};
  template <class T, class... Ts>
  struct is_variant_alternative<T, std::variant<Ts...>>
      : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
  template <class T>
  constexpr bool is_property_alternative_v = is_variant_alternative<T, PropertyValue>::value;

  // The PropertyValue alternative that represents a Property<T>:
  //  - the upcast type of T, if that is an alternative (bool, integers, enums, floating
  //    point, strings, and vectors of exactly these alternatives)
  //  - for std::vector<U> and std::array<U, N>: std::vector of the upcast type of U, if that
  //    is an alternative (e.g. std::vector<float> and std::array<double, 2> are represented
  //    as std::vector<double>)
  //  - otherwise, for types that can be converted to and from strings (see
  //    property_string.h): std::string
  //  - otherwise void, and Property<T> does not compile
  template <class T> constexpr auto select_sequence_impl() {
    if constexpr (PropertySequence<T>) {
      using V = std::vector<upcast_type_t<typename T::value_type>>;
      if constexpr (is_property_alternative_v<V>) {
        return std::type_identity<V>{};
      } else {
        return std::type_identity<void>{};
      }
    } else {
      return std::type_identity<void>{};
    }
  }
  template <class T> constexpr auto select_property_impl() {
    using U = upcast_type_t<T>;
    using S = typename decltype(select_sequence_impl<T>())::type;
    if constexpr (is_property_alternative_v<U>) {
      return std::type_identity<U>{};
    } else if constexpr (!std::is_void_v<S>) {
      return std::type_identity<S>{};
    } else if constexpr (StringConvertible<T>) {
      return std::type_identity<std::string>{};
    } else {
      return std::type_identity<void>{};
    }
  }
  template <class T> using property_impl_t = typename decltype(select_property_impl<T>())::type;

  // Convert a value to its PropertyValue alternative
  template <class T> property_impl_t<T> to_property_impl(const T& value) {
    using Impl = property_impl_t<T>;
    if constexpr (std::is_same_v<Impl, upcast_type_t<T>>) {
      return static_cast<Impl>(value);
    } else if constexpr (PropertySequence<T> && is_std_vector<Impl>::value) {
      Impl impl;
      impl.reserve(value.size());
      for (const auto& element : value) {
        impl.push_back(static_cast<typename Impl::value_type>(element));
      }
      return impl;
    } else {
      return to_string(value);
    }
  }

  // Convert a PropertyValue alternative to the value type; returns an error message on
  // failure (out of range, wrong number of elements, or unparsable string)
  template <class T>
  std::optional<std::string> from_property_impl(const property_impl_t<T>& impl, T& value) {
    using Impl = property_impl_t<T>;
    if constexpr (std::is_same_v<Impl, upcast_type_t<T>>) {
      if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        if (!std::in_range<T>(impl)) {
          return fmt::format("value {} out of range", impl);
        }
      }
      value = static_cast<T>(impl);
    } else if constexpr (PropertySequence<T> && is_std_vector<Impl>::value) {
      using Element = typename T::value_type;
      if constexpr (is_std_array<T>::value) {
        if (impl.size() != value.size()) {
          return fmt::format("{} elements given, {} required", impl.size(), value.size());
        }
      }
      std::vector<Element> elements;
      elements.reserve(impl.size());
      for (const auto& element : impl) {
        if constexpr (std::is_integral_v<Element> && !std::is_same_v<Element, bool>) {
          if (!std::in_range<Element>(element)) {
            return fmt::format("element {} out of range", element);
          }
        }
        elements.push_back(static_cast<Element>(element));
      }
      if constexpr (is_std_array<T>::value) {
        std::copy(elements.begin(), elements.end(), value.begin());
      } else {
        value = T(elements.begin(), elements.end());
      }
    } else {
      if (!from_string(impl, value)) {
        return fmt::format("cannot parse '{}'", impl);
      }
    }
    return std::nullopt;
  }
} // namespace detail

// Configuration/property handling
class Configurable {
public:
  class PropertyBase;
  using PropertyMap = std::map<std::string_view, PropertyBase&>;

  template <typename T> void setProperty(std::string_view name, T&& value) {
    using U = std::remove_cvref_t<T>;
    if constexpr (detail::is_property_alternative_v<detail::upcast_type_t<U>>) {
      m_props.at(name).set(static_cast<detail::upcast_type_t<U>>(value));
    } else {
      m_props.at(name).set(detail::to_property_impl<U>(value));
    }
  }
  // Set a property from its string representation (see detail/property_string.h)
  void setPropertyString(std::string_view name, std::string_view value) {
    m_props.at(name).parse(value);
  }
  // Get the string representation of a property (see detail/property_string.h)
  std::string getPropertyString(std::string_view name) const { return m_props.at(name).str(); }
  // Get a property value as type T, which can be any type the stored value converts to
  // without loss (e.g. int32_t for a property of type int64_t that holds a small value)
  template <typename T> T getProperty(std::string_view name) const {
    const auto& prop = m_props.at(name);
    const auto value = detail::convert_lossless<T>(prop.get());
    if (!value) {
      throw PropertyError(
          fmt::format("Property '{}' cannot be retrieved as the requested type", name));
    }
    return *value;
  }
  const PropertyMap& getProperties() const { return m_props; }

  // A Configurable registers references to its own Property members, so copies would refer
  // to the properties of the original object
  Configurable()                               = default;
  Configurable(const Configurable&)            = delete;
  Configurable& operator=(const Configurable&) = delete;
  Configurable(Configurable&&)                 = delete;
  Configurable& operator=(Configurable&&)      = delete;
  virtual ~Configurable()                      = default;

  bool hasProperty(std::string_view name) const {
    return m_props.count(name) && m_props.at(name).hasValue();
  }
  // get a vector of the names of all missing (unset) properties
  auto missingProperties() const {
    std::vector<std::string_view> missing;
    for (const auto& [name, prop] : m_props) {
      if (!prop.hasValue()) {
        missing.push_back(name);
      }
    }
    return missing;
  }
  // Throw an exception if any properties are not set
  void validate() const {
    const auto missing = missingProperties();
    if (!missing.empty()) {
      throw PropertyError(fmt::format("Missing properties: {}", missing));
    }
  }

private:
  void registerProperty(PropertyBase& prop) {
    if (m_props.count(prop.name())) {
      throw PropertyError(fmt::format("Duplicate property name: {}", prop.name()));
    }
    m_props.emplace(prop.name(), prop);
  }

  PropertyMap m_props;

public:
  class PropertyBase : public NameMixin {
  public:
    PropertyBase(std::string_view name, std::string_view description)
        : NameMixin{name, description} {}
    virtual void set(const PropertyValue& v) = 0;
    virtual PropertyValue get() const        = 0;
    // String representation of the value, and parsing of it, with the value type's own
    // conventions (e.g. enum names, shortest round-trip floating point); see
    // detail/property_string.h. parse() throws PropertyError if the string is invalid.
    virtual std::string str() const             = 0;
    virtual void parse(std::string_view text)   = 0;
    bool hasValue() const { return m_has_value; }

  protected:
    bool m_has_value = false;
  };

  // A property type that auto-registers itself with the property handler
  // Essentially a simplified and const-like version of Gaudi::Property
  template <class T> class Property : public PropertyBase {
  public:
    using value_type = T;
    using impl_type  = detail::property_impl_t<T>;
    static_assert(!std::is_void_v<impl_type>,
                  "Property<T> requires T to be representable as a PropertyValue, a vector or "
                  "array of such values, or convertible to and from std::string");

    Property(Configurable* owner, std::string_view name, std::string_view description)
        : PropertyBase{name, description} {
      if (owner) {
        owner->registerProperty(*this);
      } else {
        throw PropertyError(
            fmt::format("Attempting to create Property '{}' without valid owner", name));
      }
    }
    Property(Configurable* owner, std::string_view name, const value_type& v,
             std::string_view description)
        : Property(owner, name, description) {
      m_value     = v;
      m_has_value = true;
    }

    Property()                = delete;
    Property(const Property&) = default;
    Property& operator=(const Property&) = default;

    // Only settable by explicitly calling the ::set() member function
    // as we want the Property to mostly act as if it is constant.
    // Accepts any PropertyValue that converts without loss to the value type (e.g. an
    // int32_t for a Property<std::size_t>, or an integer for a Property<double>).
    virtual void set(const PropertyValue& v) {
      const auto converted = detail::convert_lossless<impl_type>(v);
      if (!converted) {
        throw PropertyError(
            fmt::format("Property '{}' cannot be set from a value of this type", name()));
      }
      value_type value{m_value};
      if (const auto error = detail::from_property_impl<value_type>(*converted, value)) {
        throw PropertyError(fmt::format("Property '{}': {}", name(), *error));
      }
      m_value     = std::move(value);
      m_has_value = true;
    }
    // virtual getter for use from PropertyBase - use ::value() instead for direct member
    // access
    virtual PropertyValue get() const { return detail::to_property_impl(m_value); }

    virtual std::string str() const { return detail::to_string(m_value); }
    virtual void parse(std::string_view text) {
      value_type value{m_value};
      if (!detail::from_string(text, value)) {
        throw PropertyError(fmt::format("Property '{}': cannot parse '{}'", name(), text));
      }
      m_value     = std::move(value);
      m_has_value = true;
    }

    // Direct access to the value. Use this one whenever possible (or go through the
    // automatic casting)
    const value_type& value() const { return m_value; }

    // automatically cast to T
    operator T() const { return m_value; }

    // act as if this is a const T
    template <typename U> bool operator==(const U& rhs) const { return m_value == rhs; }
    template <typename U> bool operator!=(const U& rhs) const { return m_value != rhs; }
    template <typename U> bool operator>(const U& rhs) const { return m_value > rhs; }
    template <typename U> bool operator>=(const U& rhs) const { return m_value >= rhs; }
    template <typename U> bool operator<(const U& rhs) const { return m_value < rhs; }
    template <typename U> bool operator<=(const U& rhs) const { return m_value <= rhs; }
    template <typename U> decltype(auto) operator+(const U& rhs) const { return m_value + rhs; }
    template <typename U> decltype(auto) operator-(const U& rhs) const { return m_value - rhs; }
    template <typename U> decltype(auto) operator*(const U& rhs) const { return m_value * rhs; }
    template <typename U> decltype(auto) operator/(const U& rhs) const { return m_value / rhs; }

    // stl collection helpers if needed
    // constrained so they only exist when the value type provides them, so generic code
    // (e.g. range detection in fmt) does not mistake every Property for a collection
    // (no mutating helpers, as the Property acts as if it is constant)
    decltype(auto) size() const
      requires requires(const T& v) { v.size(); }
    {
      return value().size();
    }
    decltype(auto) length() const
      requires requires(const T& v) { v.length(); }
    {
      return value().length();
    }
    decltype(auto) empty() const
      requires requires(const T& v) { v.empty(); }
    {
      return value().empty();
    }
    decltype(auto) begin() const
      requires requires(const T& v) { v.begin(); }
    {
      return value().begin();
    }
    decltype(auto) end() const
      requires requires(const T& v) { v.end(); }
    {
      return value().end();
    }
    template <class Arg>
      requires requires(const T& v, const Arg& arg) { v[arg]; }
    decltype(auto) operator[](const Arg& arg) const {
      return value()[arg];
    }
    template <class Key>
      requires requires(const T& v, const Key& key) { v.find(key); }
    decltype(auto) find(const Key& key) const {
      return value().find(key);
    }

    // In case our property has operator (), delegate the operator()
    template <class... Args>
      requires std::invocable<const T&, Args&&...>
    decltype(auto) operator()(Args&&... args) const {
      return m_value(std::forward<Args>(args)...);
    }

  private:
    // value-initialized, so a Property without default has a well-defined value
    T m_value{};
  };
}; // namespace algorithms

// Property mixin, provides all the configuration functionality for
// our algorithms and services
// Currently an alias to Configurable
using PropertyMixin = Configurable;

} // namespace algorithms

// operator== overload not needed in C++20 as it will call the member version
#if __cpp_impl_three_way_comparison < 201711
template <class T, class U>
bool operator==(const U& rhs, const algorithms::Configurable::Property<T>& p) {
  return p == rhs;
}
#endif
template <class T>
  requires requires(std::ostream& os, const T& v) { os << v; }
std::ostream& operator<<(std::ostream& os, const algorithms::Configurable::Property<T>& p) {
  return os << p.value();
}

// Make Property formattable, as its value (also for collections, which are not formatted
// as a range of the Property itself)
template <class T>
struct fmt::formatter<algorithms::Configurable::Property<T>> : fmt::formatter<T> {
  template <class FormatContext>
  auto format(const algorithms::Configurable::Property<T>& p, FormatContext& ctx) const {
    return fmt::formatter<T>::format(p.value(), ctx);
  }
};
template <class T, class Char>
struct fmt::is_range<algorithms::Configurable::Property<T>, Char> : std::false_type {};
// others needed??? TODO
