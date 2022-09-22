// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Sylvester Joosten

#include <string>
#include <type_traits>

#include <algorithms/algorithm.h>
#include <algorithms/type_traits.h>

#include <GaudiAlg/GaudiAlgorithm.h>
#include <GaudiKernel/Service.h>
#include <JugAlgo/IAlgoServiceSvc.h>
#include <JugAlgo/detail/DataProxy.h>

namespace Jug::Algo {

template <class AlgoImpl> class Algorithm : public GaudiAlgorithm {
public:
  using algo_type   = AlgoImpl;
  using input_type  = typename algo_type::input_type;
  using output_type = typename algo_type::output_type;
  using Input       = typename algo_type::Input;
  using Output      = typename algo_type::Output;

  Algorithm(const std::string& name, ISvcLocator* svcLoc)
      : GaudiAlgorithm(name, svcLoc)
      , m_algo{name}
      , m_output{this, m_algo.outputNames()}
      , m_input{this, m_algo.inputNames()} {}

  StatusCode initialize() override {
    debug() << "Initializing " << name() << endmsg;

    // Grab the AlgoServiceSvc
    m_algo_svc = service("AlgoServiceSvc");
    if (!m_algo_svc) {
      error() << "Unable to get an instance of the AlgoServiceSvc" << endmsg;
      return StatusCode::FAILURE;
    }

    // Forward the log level of this algorithm
    const algorithms::LogLevel level{
        static_cast<algorithms::LogLevel>(msgLevel() > 0 ? msgLevel() - 1 : 0)};
    debug() << "Setting the logger level to " << algorithms::logLevelName(level) << endmsg;
    m_algo.level(level);

    // Init our data structures
    debug() << "Initializing data structures" << endmsg;
    m_input.init();
    m_output.init();

    // call configure function that passes properties
    debug() << "Configuring properties" << endmsg;
    auto sc = configure();
    if (sc != StatusCode::SUCCESS) {
      return sc;
    }

    // call the internal algorithm init
    debug() << "Initializing underlying algorithm " << m_algo.name() << endmsg;
    m_algo.init();
    return StatusCode::SUCCESS;
  }

  StatusCode execute() override {
    try {
      m_algo.process(m_input.get(), m_output.get());
    } catch (const std::exception& e) {
      error() << e.what() << endmsg;
      return StatusCode::FAILURE;
    }
    return StatusCode::SUCCESS;
  }

  virtual StatusCode configure() = 0;

protected:
  template <typename T> void setAlgoProp(std::string_view name, T&& value) {
    m_algo.template setProperty<T>(name, value);
  }
  template <typename T> T getAlgoProp(std::string name) const {
    return m_algo.template getProperty<T>(name);
  }
  bool hasAlgoProp(std::string_view name) const { return m_algo.hasProperty(name); }

private:
  algo_type m_algo;
  SmartIF<IAlgoServiceSvc> m_algo_svc;
  detail::DataProxy<output_type> m_output;
  detail::DataProxy<input_type> m_input;
};

} // namespace Jug::Algo

