#pragma once

#include <functional>
#include <string>

namespace Jug {

  class ISvcLocator;

  class StatusCode {
    public:
      enum Value : uint {
        SUCCESS,
        FAILURE
      };
      StatusCode() = default;
      constexpr StatusCode(Value value): m_value(value) { };
      constexpr operator Value() const { return m_value; }
      explicit operator bool() const = delete;        
      constexpr bool operator==(StatusCode a) const { return m_value == a.m_value; }
      constexpr bool operator!=(StatusCode a) const { return m_value != a.m_value; }
      constexpr bool isFailure() const { return m_value == FAILURE; }
    private:
      Value m_value;
  };

  class JugAlgorithm {
  private:
    std::string m_name;
    ISvcLocator* m_svc;

    static std::function<std::ostream&()> m_debug;
    static std::function<std::ostream&()> m_info;
    static std::function<std::ostream&()> m_warning;
    static std::function<std::ostream&()> m_error;
    static std::function<std::ostream&()> m_endmsg;

    static void SetLoggers(
      std::function<std::ostream&()> debug,
      std::function<std::ostream&()> info,
      std::function<std::ostream&()> warning,
      std::function<std::ostream&()> error
    ) {
      m_debug = debug;
      m_info = info;
      m_warning = warning;
      m_error = error;
    }


  public:
    JugAlgorithm(const std::string& name, ISvcLocator* svc)
    : m_name(name),m_svc(svc) {
    }

  protected:
    static std::ostream& debug() { return m_debug(); };
    static std::ostream& info() { return m_info(); };
    static std::ostream& warning() { return m_warning(); };
    static std::ostream& error() { return m_error(); };
    static std::ostream& endmsg() { return m_endmsg(); };

  };

} // namespace Jug
