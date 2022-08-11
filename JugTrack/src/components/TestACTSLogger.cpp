// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong

#include "TestACTSLogger.h"

// FCCSW
//#include "DetCommon/DetUtils.h"
#include "JugBase/IGeoSvc.h"

#include "Acts/Utilities/Logger.hpp"

using namespace Acts;


TestACTSLogger::TestACTSLogger(const std::string& aName, ISvcLocator* aSvcLoc)
    : JugAlgorithm(aName, aSvcLoc) {}

TestACTSLogger::~TestACTSLogger() = default;

StatusCode TestACTSLogger::initialize() {
  if (JugAlgorithm::initialize().isFailure()) {
    return StatusCode::FAILURE;
  }
  {
    //auto testLoggerInfo =
    //    Acts::getDefaultLogger("TestLoggerInfo", Acts::Logging::INFO);

    ACTS_LOCAL_LOGGER(Acts::getDefaultLogger("TestLoggerInfo", Acts::Logging::INFO));
    ACTS_INFO("TESTING INFO LOGGING LEVEL");
    ACTS_VERBOSE("TESTING DEBUG LOGGING LEVEL");
    ACTS_DEBUG("TESTING VERBOSE LOGGING LEVEL");
  }

  {

    //auto testLoggerVerbose =
    //    Acts::getDefaultLogger("TestLoggerVerbose", Acts::Logging::VERBOSE);
    ACTS_LOCAL_LOGGER(Acts::getDefaultLogger("TestLoggerVerbose", Acts::Logging::VERBOSE));
    ACTS_INFO("TESTING INFO LOGGING LEVEL");
    ACTS_VERBOSE("TESTING DEBUG LOGGING LEVEL");
    ACTS_DEBUG("TESTING VERBOSE LOGGING LEVEL");
  }

  return StatusCode::SUCCESS;
}

StatusCode TestACTSLogger::execute() { 
  return StatusCode::SUCCESS; }

StatusCode TestACTSLogger::finalize() { return JugAlgorithm::finalize(); }
