#pragma once

#include <functional>

// Algorithm base classes
#include "Jug/Algorithm.h"
#include "Jug/Property.h"
#include "Jug/Service.h"

// DD4hep
#include "DD4hep/DD4hepUnits.h"
#include "DDRec/CellIDPositionConverter.h"
#include "DDSegmentation/BitFieldCoder.h"

// EDM4hep
#include "edm4hep/RawCalorimeterHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"

namespace Jug::Digi {

/** Generic calorimeter hit digitization.
 *
 * \ingroup digi
 * \ingroup calorimetry
 */
// FIXME should be using JugAlgorithm template
// class CalorimeterHitDigi : JugAlgorithm<edm4hep::RawCalorimeterHitCollection(edm4hep::SimCalorimeterHitCollection)>
class CalorimeterHitDigi : JugAlgorithm {

public:
  // additional smearing resolutions
  Jug::Property<std::vector<double>> u_eRes{this, "energyResolutions", {}}; // a/sqrt(E/GeV) + b + c/(E/GeV)
  Jug::Property<double> m_tRes{this, "timeResolution", 0.0 * dd4hep::ns};

  // digitization settings
  Jug::Property<unsigned int> m_capADC{this, "capacityADC", 8096};
  Jug::Property<double> m_dyRangeADC{this, "dynamicRangeADC", 100 * dd4hep::MeV};
  Jug::Property<unsigned int> m_pedMeanADC{this, "pedestalMean", 400};
  Jug::Property<double> m_pedSigmaADC{this, "pedestalSigma", 3.2};
  Jug::Property<double> m_resolutionTDC{this, "resolutionTDC", 0.010 * dd4hep::ns};

  Jug::Property<double> m_corrMeanScale{this, "scaleResponse", 1.0};
  // These are better variable names for the "energyResolutions" array which is a bit
  // magic @FIXME
  // Jug::Property<double>             m_corrSigmaCoeffE{this, "responseCorrectionSigmaCoeffE", 0.0};
  // Jug::Property<double>             m_corrSigmaCoeffSqrtE{this, "responseCorrectionSigmaCoeffSqrtE", 0.0};

  // signal sums
  // @TODO: implement signal sums with timing
  // field names to generate id mask, the hits will be grouped by masking the field
  Jug::Property<std::vector<std::string>> u_fields{this, "signalSumFields", {}};
  // ref field ids are used for the merged hits, 0 is used if nothing provided
  Jug::Property<std::vector<int>> u_refs{this, "fieldRefNumbers", {}};
  Jug::Property<std::string> m_readout{this, "readoutClass", ""};

  // Geometry service
  Jug::Service<dd4hep::Detector*(void)> m_geoSvc;

  // unitless counterparts of inputs FIXME remove
  double dyRangeADC{0}, stepTDC{0}, tRes{0}, eRes[3] = {0., 0., 0.};
  uint64_t id_mask{0}, ref_mask{0};

  CalorimeterHitDigi() = default;

  bool initialize();

  edm4hep::RawCalorimeterHitCollection operator()(const edm4hep::SimCalorimeterHitCollection& input,
                                                  const std::function<double()> normdist) const;

  bool finalize();

private:
  edm4hep::RawCalorimeterHitCollection single_hits_digi(const edm4hep::SimCalorimeterHitCollection& simhits,
                                                        const std::function<double()> normdist) const;

  edm4hep::RawCalorimeterHitCollection signal_sum_digi(const edm4hep::SimCalorimeterHitCollection& simhits,
                                                       const std::function<double()> normdist) const;
};

} // namespace Jug::Digi
