#pragma once

namespace Jug::Base {

  /** Simple particle data.
   *
   */
  struct Particle {
    int         pdgCode;
    int         charge;
    double      mass; //std::string name;
  };
} // namespace Jug::Base
