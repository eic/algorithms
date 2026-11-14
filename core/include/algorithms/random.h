#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <tuple>

#include <algorithms/detail/random.h>
#include <algorithms/logger.h>
#include <algorithms/service.h>

namespace algorithms {

// Random Engine callback function:
//   - Signature: std::function<std::vector<value_type>(size_t N)> --> generates a vector
//     of N numbers
//   - RandomEngineCB is required to return N random numbers between 0 and
//     std::numeric_limits<uint_fast64_t>::max()
//   - RandomEngineCB is responsible to deal with possible simultaneous access by multiple
//     Generator instances (required to be thread-safe).
using RandomEngineCB = detail::CachedBitGenerator::GenFunc;

// thread-safe generator front-end. Requires that the underlying random engine used by
// the RandomSvc is thread-safe.
//
// Besides the convenience methods for common distributions, a Generator satisfies the
// std::uniform_random_bit_generator concept, so it can be passed to any standard (or
// third-party) distribution, e.g. when an algorithm needs several independent distribution
// objects that each carry their own state between draws.
class Generator {
public:
  using result_type = detail::CachedBitGenerator::result_type;

  Generator(const RandomEngineCB& gen, const size_t cache_size) : m_gen{gen, cache_size} {}

  static constexpr result_type min() { return detail::CachedBitGenerator::min(); }
  static constexpr result_type max() { return detail::CachedBitGenerator::max(); }
  result_type operator()() const {
    std::lock_guard<std::mutex> lock{m_mutex};
    return m_gen();
  }

  template <std::integral Int = int>
  Int uniform_int(const Int min = Int{0}, const Int max = Int{1}) const {
    std::uniform_int_distribution<Int> d{min, max};
    std::lock_guard<std::mutex> lock{m_mutex};
    return d(m_gen);
  }
  template <std::floating_point Float = double>
  Float uniform_double(const Float min = Float{0}, const Float max = Float{1}) const {
    std::uniform_real_distribution<Float> d{min, max};
    std::lock_guard<std::mutex> lock{m_mutex};
    return d(m_gen);
  }
  template <std::integral Int = int> Int poisson(const double mean = 1.) const {
    std::poisson_distribution<Int> d{mean};
    std::lock_guard<std::mutex> lock{m_mutex};
    return d(m_gen);
  }
  template <std::integral Int = int>
  Int binomial(const Int trials = Int{1}, const double p = 0.5) const {
    std::binomial_distribution<Int> d{trials, p};
    std::lock_guard<std::mutex> lock{m_mutex};
    return d(m_gen);
  }
  template <std::floating_point Float = double>
  Float exponential(const Float lambda = Float{1}) const {
    std::exponential_distribution<Float> d{lambda};
    std::lock_guard<std::mutex> lock{m_mutex};
    return d(m_gen);
  }
  // The normal distribution generates values in pairs. The Generator keeps one distribution
  // per floating point type, so the second value of a pair is used by the next call (with
  // the same Float type) instead of being discarded. Any mu and sigma can be used per call:
  // the stored value is a standard normal deviate, scaled when it is returned. The sequence
  // is identical to calling a single std::normal_distribution<Float> with this Generator.
  template <std::floating_point Float = double>
  Float gaussian(const Float mu = Float{0}, const Float sigma = Float{1}) const {
    using Dist = std::normal_distribution<Float>;
    std::lock_guard<std::mutex> lock{m_mutex};
    return std::get<Dist>(m_normal)(m_gen, typename Dist::param_type{mu, sigma});
  }

private:
  mutable detail::CachedBitGenerator m_gen;
  mutable std::tuple<std::normal_distribution<float>, std::normal_distribution<double>,
                     std::normal_distribution<long double>>
      m_normal;
  mutable std::mutex m_mutex;
};

// Random service that creates multiple Generators that are linked to a single random
// engine. The Generators are safe to be used in parallel as long as the Engine itself is
// thread-safe (this is a hard requirement for MT). The Generators avoid unnecesary locking
// by running off a auto-refreshing cached random sequence.
class RandomSvc : public LoggedService<RandomSvc> {
public:
  using value_type = detail::CachedBitGenerator::result_type;

  // Generator linked to the shared service engine. The numbers drawn by a given Generator
  // depend on the order in which all Generators request numbers from the shared engine, so
  // they are not reproducible in multi-threaded execution.
  Generator generator() { return {m_gen, m_cache_size}; }
  // Generator with its own private engine, seeded with the provided seed. The numbers
  // drawn depend only on the seed, independent of any other Generator, which makes them
  // reproducible regardless of the number of threads or the order of execution. Derive
  // the seed from the event (and the algorithm) to obtain reproducible per-event sequences.
  // The service seed and any external generator function are not used.
  Generator generator(const size_t seed) const { return {createEngine(seed), m_cache_size}; }
// FIXME fix the CMake setup so these are properly found in Gaudi
#if 0 
  void init();
  void init(const RandomEngineCB& gen);
#endif
  void init() {
    if (m_seed.hasValue()) {
      info() << "Custom random seed requested: " << m_seed << endmsg;
      m_gen = createEngine(m_seed);
    }
  }
  void init(const RandomEngineCB& gen) {
    info() << "Loading external generator function." << endmsg;
    m_gen = gen;
    if (m_seed.hasValue()) {
      warning() << "Custom random seed request ignored when using external generator function"
                << endmsg;
    }
  }

#if 0
private:
  static RandomEngineCB createEngine(const size_t seed = 1);
#endif
  static RandomEngineCB createEngine(const size_t seed = 1) {
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
  // end of FIXME

private:
  RandomEngineCB m_gen{createEngine()};
  Property<size_t> m_seed{this, "seed", "Random seed for the internal random engine"};
  Property<size_t> m_cache_size{this, "cacheSize", 1024, "Cache size for each generator instance"};
  std::mutex m_mutex;

  ALGORITHMS_DEFINE_LOGGED_SERVICE(RandomSvc)
};

} // namespace algorithms
