#include <algorithms/random.h>

namespace algorithms {

// FIXME fix the CMake setup so these are properly found in Gaudi...
#if 0
void RandomSvc::init() {
  if (m_seed.hasValue()) {
    info() << "Custom random seed requested: " << m_seed << endmsg;
    m_gen = createEngine(m_seed);
  }
}
void RandomSvc::init(const RandomEngineCB& gen) {
  init();
  info() << "Loading external generator function." << endmsg;
  m_gen = gen;
  if (m_seed.hasValue()) {
    warning() << "Custom random seed request ignored when using external generator function"
              << endmsg;
  }
}
RandomEngineCB RandomSvc::createEngine(const size_t seed) {
  // Each engine has its own state, shared by all copies of the callback
  struct Engine {
    std::mutex m;
    std::mt19937_64 gen;
  };
  auto engine = std::make_shared<Engine>();
  engine->gen.seed(seed);
  return [engine](const size_t size) {
    std::lock_guard<std::mutex> lock{engine->m};
    std::vector<value_type> ret(size);
    // pass the generator by reference, so its state advances
    std::generate(ret.begin(), ret.end(), std::ref(engine->gen));
    return ret;
  };
}
#endif
} // namespace algorithms
