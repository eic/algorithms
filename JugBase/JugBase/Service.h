#pragma once

#include <functional>
#include <string>

#include "JugBase/Algorithm.h"

namespace algorithms {

template <class FUNCTIONAL> class Service : public std::function<FUNCTIONAL> {
private:
  std::string m_name;

public:
  template <class OWNER> Service(OWNER* /*owner*/, const std::string& name) : m_name(name) {
    if (this != nullptr) {
      //owner->registerService(this, name);
    }
  }
};

} // namespace algorithms
