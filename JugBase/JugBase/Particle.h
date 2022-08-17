#pragma once

namespace algorithms::base {

  /** Simple particle data.
   *
   */
  struct Particle {
    int         pdgCode;
    int         charge;
    double      mass; //std::string name;
  };
} // namespace algorithms::base
