// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Conversion of Property values to and from strings, for frameworks that configure
// algorithms with text (command line, option files), and for value types that are not
// one of the PropertyValue alternatives.
//
// Format:
//  - std::string: as is
//  - bool: "true"/"false" (parsed also from "1"/"0")
//  - integers (including int8_t/uint8_t): decimal numbers, range-checked
//  - floating point: shortest representation that round-trips (e.g. 0.1f -> "0.1")
//  - enums: operator<< / operator>> if declared for the enum (so names can be used),
//    otherwise the underlying integer
//  - std::vector and std::array: comma-separated elements ("1,2,3"), whitespace around
//    elements is ignored; std::array requires exactly N elements. Elements cannot contain
//    commas.
//  - other types: operator<< / operator>>; parsing must consume the whole string
//
#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

namespace algorithms::detail {

template <class T> struct is_std_array : std::false_type {};
template <class T, std::size_t N> struct is_std_array<std::array<T, N>> : std::true_type {};
template <class T> struct is_std_vector_like : std::false_type {};
template <class T, class A> struct is_std_vector_like<std::vector<T, A>> : std::true_type {};
template <class T>
concept PropertySequence = is_std_array<T>::value || is_std_vector_like<T>::value;

template <class T>
concept OStreamable = requires(std::ostream& os, const T& v) { os << v; };
template <class T>
concept IStreamable = requires(std::istream& is, T& v) { is >> v; };

// Types whose values can be converted to and from strings
template <class T> constexpr bool is_string_convertible() {
  if constexpr (std::is_same_v<T, std::string> || std::is_arithmetic_v<T> || std::is_enum_v<T>) {
    return true;
  } else if constexpr (PropertySequence<T>) {
    return is_string_convertible<typename T::value_type>();
  } else {
    return OStreamable<T> && IStreamable<T> && std::default_initializable<T>;
  }
}
template <class T>
concept StringConvertible = is_string_convertible<T>();

inline std::string_view trim(std::string_view s) {
  constexpr std::string_view whitespace = " \t\n\r\f\v";
  const auto first                      = s.find_first_not_of(whitespace);
  if (first == std::string_view::npos) {
    return {};
  }
  const auto last = s.find_last_not_of(whitespace);
  return s.substr(first, last - first + 1);
}

template <StringConvertible T> std::string to_string(const T& value) {
  if constexpr (std::is_same_v<T, std::string>) {
    return value;
  } else if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
                       std::is_same_v<T, unsigned char>) {
    // as numbers, not characters
    return fmt::format("{}", static_cast<int>(value));
  } else if constexpr (std::is_arithmetic_v<T>) {
    return fmt::format("{}", value);
  } else if constexpr (PropertySequence<T>) {
    std::string out;
    for (bool first = true; const auto& element : value) {
      if (!first) {
        out += ",";
      }
      first = false;
      out += to_string(static_cast<typename T::value_type>(element));
    }
    return out;
  } else if constexpr (OStreamable<T> && (!std::is_enum_v<T> || IStreamable<T>)) {
    // enums only by name if they can also be parsed by name
    std::ostringstream os;
    os << value;
    return os.str();
  } else {
    static_assert(std::is_enum_v<T>);
    return fmt::format("{}", static_cast<std::underlying_type_t<T>>(value));
  }
}

// Parse a string into value; returns false (leaving value unspecified) on failure
template <StringConvertible T> bool from_string(std::string_view text, T& value) {
  if constexpr (std::is_same_v<T, std::string>) {
    value = std::string{text};
    return true;
  } else if constexpr (std::is_same_v<T, bool>) {
    const auto s = trim(text);
    if (s == "true" || s == "1") {
      value = true;
    } else if (s == "false" || s == "0") {
      value = false;
    } else {
      return false;
    }
    return true;
  } else if constexpr (std::is_integral_v<T>) {
    const auto s = trim(text);
    // parse as a wide integer, so (u)int8_t is a number and the range check is explicit
    using wide_type = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
    wide_type wide{};
    const auto* begin    = s.data() + (s.starts_with('+') ? 1 : 0);
    const auto [ptr, ec] = std::from_chars(begin, s.data() + s.size(), wide);
    if (ec != std::errc{} || ptr != s.data() + s.size() || s.empty() || !std::in_range<T>(wide)) {
      return false;
    }
    value = static_cast<T>(wide);
    return true;
  } else if constexpr (std::is_floating_point_v<T>) {
    const auto s         = trim(text);
    const auto* begin    = s.data() + (s.starts_with('+') ? 1 : 0);
    const auto [ptr, ec] = std::from_chars(begin, s.data() + s.size(), value);
    return ec == std::errc{} && ptr == s.data() + s.size() && !s.empty();
  } else if constexpr (PropertySequence<T>) {
    std::vector<typename T::value_type> elements;
    if (!trim(text).empty()) {
      std::size_t start = 0;
      while (true) {
        const auto comma = text.find(',', start);
        const auto token = text.substr(
            start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        typename T::value_type element{};
        if (!from_string(std::is_same_v<typename T::value_type, std::string> ? trim(token) : token,
                         element)) {
          return false;
        }
        elements.push_back(std::move(element));
        if (comma == std::string_view::npos) {
          break;
        }
        start = comma + 1;
      }
    }
    if constexpr (is_std_array<T>::value) {
      if (elements.size() != value.size()) {
        return false;
      }
      std::move(elements.begin(), elements.end(), value.begin());
    } else {
      value = T(elements.begin(), elements.end());
    }
    return true;
  } else if constexpr (IStreamable<T>) {
    std::istringstream is{std::string{trim(text)}};
    is >> value;
    return !is.fail() && (is >> std::ws).eof();
  } else {
    static_assert(std::is_enum_v<T>);
    std::underlying_type_t<T> underlying{};
    if (!from_string(text, underlying)) {
      return false;
    }
    value = static_cast<T>(underlying);
    return true;
  }
}

} // namespace algorithms::detail
