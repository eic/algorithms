// Algorithm base classes
#include "JugBase/Algorithm.h"
#include "JugBase/Property.h"
#include "JugBase/Service.h"
#include "JugBase/Particle.h"

// DD4hep
#include "DD4hep/DD4hepUnits.h"

// EDM4hep
#include "edm4hep/SimCalorimeterHitCollection.h"

namespace algorithms::digi {

/** Generic calorimeter hit digitiziation.
 *
 * \ingroup digi
 * \ingroup calorimetry
 */
class CalorimeterBirksCorr final : public JugAlgorithm<edm4hep::SimCalorimeterHitCollection, edm4hep::SimCalorimeterHitCollection> {
public:
  CalorimeterBirksCorr() = default;

  // Properties
  algorithms::Property<double> m_birksConstant{this, "birksConstant", 0.126 * dd4hep::mm / dd4hep::MeV};

  // Services
  algorithms::Service<algorithms::base::Particle(int)> m_pidSvc;

  // Operator
  edm4hep::SimCalorimeterHitCollection operator()(const edm4hep::SimCalorimeterHitCollection& input) const;
};

} // namespace algorithms::digi
