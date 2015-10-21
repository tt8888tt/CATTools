#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/Common/interface/Association.h"
#include "DataFormats/Common/interface/RefToPtr.h"

#include "DataFormats/PatCandidates/interface/MET.h"
#include "CATTools/DataFormats/interface/MET.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidateFwd.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include "CommonTools/UtilAlgos/interface/StringCutObjectSelector.h"
#include "RecoEcal/EgammaCoreTools/interface/EcalClusterLazyTools.h"

using namespace edm;
using namespace std;

namespace cat {

  class CATMETProducer : public edm::stream::EDProducer<> {
  public:
    explicit CATMETProducer(const edm::ParameterSet & iConfig);
    virtual ~CATMETProducer() { }

    void produce(edm::Event & iEvent, const edm::EventSetup & iSetup) override;

  private:
    edm::EDGetTokenT<pat::METCollection> src_;
    bool setUnclusteredEn_;

  };

} // namespace

cat::CATMETProducer::CATMETProducer(const edm::ParameterSet & iConfig) :
  src_(consumes<pat::METCollection>(iConfig.getParameter<edm::InputTag>("src"))),
  setUnclusteredEn_(iConfig.getParameter<bool>("setUnclusteredEn"))
{
  produces<std::vector<cat::MET> >();
}

void
cat::CATMETProducer::produce(edm::Event & iEvent, const edm::EventSetup & iSetup)
{
  Handle<pat::METCollection> src;
  iEvent.getByToken(src_, src);

  auto_ptr<vector<cat::MET> >  out(new vector<cat::MET>());

  const pat::MET & aPatMET = src->front();
  cat::MET aMET(aPatMET, aPatMET.sumEt() );
  if (setUnclusteredEn_){
    aMET.setUnclusteredEnUp(aPatMET.shiftedPx(pat::MET::UnclusteredEnUp),
			    aPatMET.shiftedPy(pat::MET::UnclusteredEnUp),
			    aPatMET.shiftedSumEt(pat::MET::UnclusteredEnUp));
    aMET.setUnclusteredEnDown(aPatMET.shiftedPx(pat::MET::UnclusteredEnDown),
			      aPatMET.shiftedPy(pat::MET::UnclusteredEnDown),
  			      aPatMET.shiftedSumEt(pat::MET::UnclusteredEnDown));
  }
  out->push_back(aMET);

  iEvent.put(out);
}

#include "FWCore/Framework/interface/MakerMacros.h"
using namespace cat;
DEFINE_FWK_MODULE(CATMETProducer);
