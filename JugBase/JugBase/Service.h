#pragma once

#include <string>

namespace algorithms {

  template <class FUNCTIONAL>
  class Service: public std::function<FUNCTIONAL> {
  public:
    Service() = default;
  };

} // namespace algorithms
