#pragma once

#include <string>

namespace Jug {

  template <class FUNCTIONAL>
  class Service: public std::function<FUNCTIONAL> {
  public:
    Service() = default;
  };

} // namespace Jug
