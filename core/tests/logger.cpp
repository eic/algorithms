// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Wouter Deconinck
//
// Tests for the LogSvc, the LoggerMixin and the LoggedService base class
//
#include <algorithms/logger.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <ios>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

using namespace algorithms;

namespace {

struct Message {
  LogLevel level;
  std::string caller;
  std::string msg;
  bool operator==(const Message&) const = default;
};

std::vector<Message>& messages() {
  static std::vector<Message> m;
  return m;
}

// Replace the LogSvc action with one that records all messages
void capture() {
  messages().clear();
  LogSvc::instance().init([](LogLevel l, std::string_view caller, std::string_view msg) {
    messages().push_back({l, std::string{caller}, std::string{msg}});
  });
}

class TestLogger : public LoggerMixin {
public:
  TestLogger() : LoggerMixin("TestLogger") {}
  explicit TestLogger(LogLevel threshold) : LoggerMixin("TestLogger", threshold) {}

  using LoggerMixin::critical;
  using LoggerMixin::debug;
  using LoggerMixin::endmsg;
  using LoggerMixin::error;
  using LoggerMixin::info;
  using LoggerMixin::raise;
  using LoggerMixin::trace;
  using LoggerMixin::warning;

  using LoggerMixin::aboveCriticalThreshold;
  using LoggerMixin::aboveDebugThreshold;
  using LoggerMixin::aboveErrorThreshold;
  using LoggerMixin::aboveInfoThreshold;
  using LoggerMixin::aboveTraceThreshold;
  using LoggerMixin::aboveWarningThreshold;
};

class CustomError : public Error {
public:
  CustomError(std::string_view msg) : Error{msg, "CustomError"} {}
};

class TestLoggedSvc : public LoggedService<TestLoggedSvc> {
public:
  void init() {}
  void hello() const { info() << "hello" << endmsg; }

  ALGORITHMS_DEFINE_LOGGED_SERVICE(TestLoggedSvc)
};

const std::vector<LogLevel> kAllLevels{LogLevel::kTrace,   LogLevel::kDebug, LogLevel::kInfo,
                                       LogLevel::kWarning, LogLevel::kError, LogLevel::kCritical};

} // namespace

// Must run before any test replaces the default action
TEST_CASE("LogSvc has a default action that prints messages", "[logger]") {
  CHECK_NOTHROW(LogSvc::instance().report(LogLevel::kInfo, "caller", "default action message"));
}

TEST_CASE("Log levels have names", "[logger]") {
  STATIC_REQUIRE(logLevelName(LogLevel::kTrace) == "TRACE");

  // evaluated at run time as well, since the names are used at run time
  std::vector<std::string_view> names;
  for (const auto level : kAllLevels) {
    names.push_back(logLevelName(level));
  }
  CHECK(names == std::vector<std::string_view>{"TRACE", "DEBUG", "INFO", "WARNING", "ERROR",
                                               "CRITICAL"});

  auto unknown = static_cast<LogLevel>(42);
  CHECK(logLevelName(unknown) == "UNKNOWN");
}

TEST_CASE("LogSvc provides the default log level", "[logger]") {
  auto& svc = LogSvc::instance();
  CHECK(svc.defaultLevel() == LogLevel::kInfo);
  CHECK(TestLogger{}.level() == LogLevel::kInfo);

  svc.defaultLevel(LogLevel::kDebug);
  CHECK(svc.defaultLevel() == LogLevel::kDebug);
  CHECK(TestLogger{}.level() == LogLevel::kDebug);
  CHECK(detail::LoggerStream{"caller", LogLevel::kInfo}.threshold() == LogLevel::kDebug);

  svc.defaultLevel(LogLevel::kInfo);
  CHECK(TestLogger{}.level() == LogLevel::kInfo);
}

TEST_CASE("LogSvc init without arguments keeps the current action", "[logger]") {
  capture();
  LogSvc::instance().init();
  LogSvc::instance().report(LogLevel::kWarning, "caller", "still captured");
  CHECK(messages() == std::vector<Message>{{LogLevel::kWarning, "caller", "still captured"}});
}

TEST_CASE("LoggerMixin streams report messages at or above threshold", "[logger]") {
  capture();
  const TestLogger logger{LogLevel::kTrace};

  logger.trace() << "t" << TestLogger::endmsg;
  logger.debug() << "d" << TestLogger::endmsg;
  logger.info() << "i" << TestLogger::endmsg;
  logger.warning() << "w" << TestLogger::endmsg;
  logger.error() << "e" << TestLogger::endmsg;
  logger.critical() << "c" << TestLogger::endmsg;

  CHECK(messages() == std::vector<Message>{{LogLevel::kTrace, "TestLogger", "t"},
                                           {LogLevel::kDebug, "TestLogger", "d"},
                                           {LogLevel::kInfo, "TestLogger", "i"},
                                           {LogLevel::kWarning, "TestLogger", "w"},
                                           {LogLevel::kError, "TestLogger", "e"},
                                           {LogLevel::kCritical, "TestLogger", "c"}});
}

TEST_CASE("LoggerMixin streams suppress messages below threshold", "[logger]") {
  capture();
  const TestLogger logger{LogLevel::kCritical};

  logger.trace() << "t" << std::hex << 255 << std::endl;
  logger.debug() << "d" << TestLogger::endmsg;
  logger.info() << "i" << TestLogger::endmsg;
  logger.warning() << "w" << TestLogger::endmsg;
  logger.error() << "e" << TestLogger::endmsg;
  logger.critical() << "c" << TestLogger::endmsg;

  CHECK(messages() == std::vector<Message>{{LogLevel::kCritical, "TestLogger", "c"}});
}

TEST_CASE("LoggerMixin streams support stream manipulators", "[logger]") {
  capture();
  const TestLogger logger;

  logger.info() << std::hex << 255 << std::endl;
  logger.info() << std::dec << 255 << TestLogger::endmsg;

  CHECK(messages() == std::vector<Message>{{LogLevel::kInfo, "TestLogger", "ff\n"},
                                           {LogLevel::kInfo, "TestLogger", "255"}});
}

TEST_CASE("LoggerMixin streams report unflushed messages at the end of the statement",
          "[logger]") {
  capture();
  const TestLogger logger;

  logger.info() << "without endmsg " << 1;
  logger.info() << "with endmsg" << TestLogger::endmsg;
  logger.info() << TestLogger::endmsg;
  logger.debug() << "suppressed without endmsg";

  CHECK(messages() == std::vector<Message>{{LogLevel::kInfo, "TestLogger", "without endmsg 1"},
                                           {LogLevel::kInfo, "TestLogger", "with endmsg"},
                                           {LogLevel::kInfo, "TestLogger", ""}});
}

TEST_CASE("LoggerMixin reports plain messages at or above threshold", "[logger]") {
  capture();
  const TestLogger logger{LogLevel::kTrace};

  logger.trace("t");
  logger.debug("d");
  logger.info("i");
  logger.warning("w");
  logger.error("e");
  logger.critical("c");

  CHECK(messages() == std::vector<Message>{{LogLevel::kTrace, "TestLogger", "t"},
                                           {LogLevel::kDebug, "TestLogger", "d"},
                                           {LogLevel::kInfo, "TestLogger", "i"},
                                           {LogLevel::kWarning, "TestLogger", "w"},
                                           {LogLevel::kError, "TestLogger", "e"},
                                           {LogLevel::kCritical, "TestLogger", "c"}});
}

TEST_CASE("LoggerMixin suppresses plain messages below threshold", "[logger]") {
  capture();
  const TestLogger logger{LogLevel::kCritical};

  logger.trace("t");
  logger.debug("d");
  logger.info("i");
  logger.warning("w");
  logger.error("e");
  logger.critical("c");

  CHECK(messages() == std::vector<Message>{{LogLevel::kCritical, "TestLogger", "c"}});
}

TEST_CASE("LoggerMixin reports formatted messages at or above threshold", "[logger]") {
  capture();
  const TestLogger logger{LogLevel::kTrace};

  logger.trace("t{}", 0);
  logger.debug("d{}", 1);
  logger.info("i{}", 2);
  logger.warning("w{}", 3);
  logger.error("e{}", 4);
  logger.critical("c{}", 5);

  CHECK(messages() == std::vector<Message>{{LogLevel::kTrace, "TestLogger", "t0"},
                                           {LogLevel::kDebug, "TestLogger", "d1"},
                                           {LogLevel::kInfo, "TestLogger", "i2"},
                                           {LogLevel::kWarning, "TestLogger", "w3"},
                                           {LogLevel::kError, "TestLogger", "e4"},
                                           {LogLevel::kCritical, "TestLogger", "c5"}});
}

TEST_CASE("LoggerMixin suppresses formatted messages below threshold", "[logger]") {
  capture();
  const TestLogger logger{LogLevel::kCritical};

  logger.trace("t{}", 0);
  logger.debug("d{}", 1);
  logger.info("i{}", 2);
  logger.warning("w{}", 3);
  logger.error("e{}", 4);
  logger.critical("c{}", 5);

  CHECK(messages() == std::vector<Message>{{LogLevel::kCritical, "TestLogger", "c5"}});
}

TEST_CASE("LoggerMixin level can be changed", "[logger]") {
  capture();
  TestLogger logger{LogLevel::kCritical};

  logger.info() << "suppressed" << TestLogger::endmsg;
  logger.level(LogLevel::kInfo);
  CHECK(logger.level() == LogLevel::kInfo);
  logger.info() << "reported" << TestLogger::endmsg;
  logger.info("plain");

  CHECK(messages() == std::vector<Message>{{LogLevel::kInfo, "TestLogger", "reported"},
                                           {LogLevel::kInfo, "TestLogger", "plain"}});
}

TEST_CASE("LoggerMixin reports its threshold", "[logger]") {
  for (const auto level : kAllLevels) {
    const TestLogger logger{level};
    CAPTURE(logLevelName(level));
    CHECK(logger.aboveTraceThreshold() == (level >= LogLevel::kTrace));
    CHECK(logger.aboveDebugThreshold() == (level >= LogLevel::kDebug));
    CHECK(logger.aboveInfoThreshold() == (level >= LogLevel::kInfo));
    CHECK(logger.aboveWarningThreshold() == (level >= LogLevel::kWarning));
    CHECK(logger.aboveErrorThreshold() == (level >= LogLevel::kError));
    CHECK(logger.aboveCriticalThreshold() == (level >= LogLevel::kCritical));
  }
}

TEST_CASE("LoggerMixin raises errors and logs them", "[logger]") {
  capture();
  const TestLogger logger;

  CHECK_THROWS_AS(logger.raise("default error"), Error);
  CHECK_THROWS_WITH(logger.raise("default error"), "default error");
  try {
    logger.raise<CustomError>("custom error");
    FAIL("raise did not throw");
  } catch (const CustomError& e) {
    CHECK(std::string{e.what()} == "custom error");
    CHECK(std::string{e.type()} == "CustomError");
  }

  CHECK(messages() == std::vector<Message>{{LogLevel::kError, "TestLogger", "default error"},
                                           {LogLevel::kError, "TestLogger", "default error"},
                                           {LogLevel::kError, "TestLogger", "custom error"}});
}

TEST_CASE("LoggerStream threshold can be changed", "[logger]") {
  capture();
  detail::LoggerStream stream{"stream", LogLevel::kInfo, LogLevel::kWarning};
  CHECK(stream.threshold() == LogLevel::kWarning);

  stream << "suppressed" << TestLogger::endmsg;
  stream.threshold(LogLevel::kInfo);
  CHECK(stream.threshold() == LogLevel::kInfo);
  stream << "reported" << TestLogger::endmsg;

  CHECK(messages() == std::vector<Message>{{LogLevel::kInfo, "stream", "reported"}});
}

TEST_CASE("LoggedService logs with the service name", "[logger]") {
  capture();
  const auto& svc = TestLoggedSvc::instance();

  CHECK(svc.name() == "TestLoggedSvc");
  CHECK(std::string_view{TestLoggedSvc::kName} == "TestLoggedSvc");
  CHECK(ServiceSvc::instance().has<TestLoggedSvc>());
  svc.hello();

  CHECK(messages() == std::vector<Message>{{LogLevel::kInfo, "TestLoggedSvc", "hello"}});
}
