#include "Gaudi/Algorithm.h"
#include "GaudiAlg/GaudiTool.h"
#include "GaudiAlg/Producer.h"
#include "GaudiAlg/Transformer.h"
#include "GaudiKernel/RndmGenerators.h"
#include <algorithm>
#include <cmath>

#include "JugBase/IParticleSvc.h"
#include "JugBase/DataHandle.h"
#include "JugBase/UniqueID.h"

#include <CLHEP/Vector/LorentzVector.h>

// Event Model related classes
#include "dd4pod/Geant4ParticleCollection.h"
#include "eicd/InclusiveKinematicsCollection.h"

namespace Jug::Fast {

class InclusiveKinematicsTruth : public GaudiAlgorithm, AlgorithmIDMixin<int32_t> {
public:
  DataHandle<dd4pod::Geant4ParticleCollection> m_inputParticleCollection{"mcparticles", Gaudi::DataHandle::Reader,
                                                                         this};
  DataHandle<eic::InclusiveKinematicsCollection> m_outputInclusiveKinematicsCollection{"InclusiveKinematicsTruth",
                                                                                       Gaudi::DataHandle::Writer, this};

  SmartIF<IParticleSvc> m_pidSvc;
  double m_proton;
  double m_neutron;

  InclusiveKinematicsTruth(const std::string& name, ISvcLocator* svcLoc)
      : GaudiAlgorithm(name, svcLoc), AlgorithmIDMixin(name, info()) {
    declareProperty("inputMCParticles", m_inputParticleCollection, "mcparticles");
    declareProperty("outputData", m_outputInclusiveKinematicsCollection, "InclusiveKinematicsTruth");
  }

  StatusCode initialize() override {
    if (GaudiAlgorithm::initialize().isFailure())
      return StatusCode::FAILURE;

    m_pidSvc = service("ParticleSvc");
    if (!m_pidSvc) {
      error() << "Unable to locate Particle Service. "
              << "Make sure you have ParticleSvc in the configuration."
              << endmsg;
      return StatusCode::FAILURE;
    }
    m_proton = m_pidSvc->particle(2212).mass;
    m_neutron = m_pidSvc->particle(2112).mass;

    return StatusCode::SUCCESS;
  }

  StatusCode execute() override {
    // input collection
    const dd4pod::Geant4ParticleCollection& parts = *(m_inputParticleCollection.get());
    // output collection
    auto& out_kinematics = *(m_outputInclusiveKinematicsCollection.createAndPut());

    // Loop over generated particles to get incoming electron and proton beams
    // and the scattered electron. In the presence of QED radition on the incoming
    // or outgoing electron line, the vertex kinematics will be different than the
    // kinematics calculated using the scattered electron as done here.
    // Also need to update for CC events.
    auto ei = CLHEP::HepLorentzVector(0., 0., 0., 0.);
    auto pi = CLHEP::HepLorentzVector(0., 0., 0., 0.);
    auto ef = CLHEP::HepLorentzVector(0., 0., 0., 0.);

    bool ebeam_found = false;
    bool pbeam_found = false;
    int32_t mcscatID = -1;
  
    for (const auto& p : parts) {

      if (p.genStatus() == 4 && p.pdgID() == 11) { // Incoming electron
        ei.setPx(p.ps().x);
        ei.setPy(p.ps().y);
        ei.setPz(p.ps().z);
        ei.setE(p.energy());
        ebeam_found = true;
      }
      else if (p.genStatus() == 4 && p.pdgID() == 2212) { // Incoming proton
        pi.setPx(p.ps().x);
        pi.setPy(p.ps().y);
        pi.setPz(p.ps().z);
        pi.setE(p.energy());
        pbeam_found = true;
      }
      else if (p.genStatus() == 4 && p.pdgID() == 2112) { // Incoming neutron
        pi.setPx(p.ps().x);
        pi.setPy(p.ps().y);
        pi.setPz(p.ps().z);
        pi.setE(p.energy());
        pbeam_found = true;
      }
      // Scattered electron. Currently taken as first status==1 electron in HEPMC record,
      // which seems to be correct based on a cursory glance at the Pythia8 output. In the future,
      // it may be better to trace back each final-state electron and see which one originates from
      // the beam.
      else if (p.genStatus() == 1 && p.pdgID() == 11 && mcscatID == -1) {
        ef.setPx(p.ps().x);
        ef.setPy(p.ps().y);
        ef.setPz(p.ps().z);
        ef.setE(p.energy());

        mcscatID = p.ID();
      }
      if (ebeam_found && pbeam_found && mcscatID != -1) {
        // all done!
        break;
      }
    }

    // Not all particles found
    if (ebeam_found == false) {
      if (msgLevel(MSG::DEBUG)) {
        debug() << "No initial electron found" << endmsg;
      }
      return StatusCode::SUCCESS;
    }
    if (pbeam_found == false) {
      if (msgLevel(MSG::DEBUG)) {
        debug() << "No initial proton found" << endmsg;
      }
      return StatusCode::SUCCESS;
    }
    if (mcscatID == -1) {
      if (msgLevel(MSG::DEBUG)) {
        debug() << "No scattered electron found" << endmsg;
      }
      return StatusCode::SUCCESS;
    }

    // DIS kinematics calculations
    auto kin = out_kinematics.create();
    const auto q = ei - ef;
    kin.Q2(-1. * q.m2());
    kin.y((q * pi) / (ei * pi));
    kin.nu(q * pi / m_proton);
    kin.x(kin.Q2() / (2. * q * pi));
    kin.W(sqrt((pi + q).m2()));
    kin.scatID(mcscatID);

    // Debugging output
    if (msgLevel(MSG::DEBUG)) {
      debug() << "pi = " << pi << endmsg;
      debug() << "ei = " << ei << endmsg;
      debug() << "ef = " << ef << endmsg;
      debug() << "q = " << q << endmsg;
      debug() << "x,y,Q2,W,nu = "
              << kin.x() << "," 
              << kin.y() << ","
              << kin.Q2() << ","
              << kin.W() << ","
              << kin.nu()
              << endmsg;
    }

    return StatusCode::SUCCESS;
  }
};

DECLARE_COMPONENT(InclusiveKinematicsTruth)

} // namespace Jug::Fast
