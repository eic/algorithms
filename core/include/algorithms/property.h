// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Wouter Deconinck, Sylvester Joosten
//
// Defines the Configurable base class and related PropertyMixin
// These base classes provide access to the Property<T> object, a self-registering
// configurable property that acts as a bare object T from a performance point-of-view
//
#pragma once

#include <concepts>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

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

// Configuration/property handling
class Configurable {
public:
  class PropertyBase;
  using PropertyMap = std::map<std::string_view, PropertyBase&>;

  template <typename T> void setProperty(std::string_view name, T&& value) {
    m_props.at(name).set(static_cast<detail::upcast_type_t<T>>(value));
  }
  template <typename T> T getProperty(std::string_view name) const {
    return std::get<T>(m_props.at(name).get());
  }
  const PropertyMap& getProperties() const { return m_props; }
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
    bool hasValue() const { return m_has_value; }

  protected:
    bool m_has_value = false;
  };

  // A property type that auto-registers itself with the property handler
  // Essentially a simplified and const-like version of Gaudi::Property
  template <class T> class Property : public PropertyBase {
  public:
    using value_type = T;
    using impl_type  = detail::upcast_type_t<T>;

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
      set(static_cast<impl_type>(v));
    }

    Property()                = delete;
    Property(const Property&) = default;
    Property& operator=(const Property&) = default;

    // Only settable by explicitly calling the ::set() member function
    // as we want the Property to mostly act as if it is constant
    virtual void set(const PropertyValue& v) {
      m_value     = static_cast<value_type>(std::get<impl_type>(v));
      m_has_value = true;
    }
    // virtual getter for use from PropertyBase - use ::value() instead for direct member
    // access
    virtual PropertyValue get() const { return static_cast<impl_type>(m_value); }

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
    T m_value;
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
