#include "JugAlg/JugAlgorithm.h"

#include "JugBase/DataHandle.h"

#include "edm4hep/MCParticleCollection.h"

class ReadTestConsumer : public JugAlgorithm {

public:
  ReadTestConsumer(const std::string& name, ISvcLocator* svcLoc)
      : JugAlgorithm(name, svcLoc), m_genParticles("MCParticles", Jug::DataHandle::Reader, this) {
    declareProperty("genParticles", m_genParticles, "mc particles to read");
  }

  ~ReadTestConsumer() = default;

  StatusCode initialize() {
    warning() << "This is a deprecated test algorithm" << endmsg;
    return JugAlgorithm::initialize(); }

  StatusCode execute() {
    // Read the input
    const edm4hep::MCParticleCollection* mcparticles = m_genParticles.get();

    // Does the reading work?
    debug() << mcparticles << endmsg;
    debug() << "MCParticle size: " << mcparticles->size() << endmsg;
    // counter for debug messages below
    //int cntr = 0;
    // Loop over all input particles
    //for (const auto& mcpart : *mcparticles) {
    //  if (10 > cntr++) {
    //    debug() << "time: " << mcpart.time << endmsg;
    //  }
    //}
    return StatusCode::SUCCESS;
  }

  StatusCode finalize() { return JugAlgorithm::finalize(); }

private:
  /// Particles to read
  DataHandle<edm4hep::MCParticleCollection> m_genParticles;
};
