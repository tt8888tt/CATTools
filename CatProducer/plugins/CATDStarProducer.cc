#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/Common/interface/Association.h"
#include "DataFormats/Common/interface/RefToPtr.h"

#include "CATTools/DataFormats/interface/Particle.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include <TLorentzVector.h>
#include "CATTools/DataFormats/interface/SecVertex.h"


#include "RecoVertex/KalmanVertexFit/interface/KalmanVertexFitter.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"
#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"
#include "TrackingTools/IPTools/interface/IPTools.h"

using namespace edm;
using namespace std;
using namespace reco;

namespace cat {

  class CATDStarProducer : public edm::stream::EDProducer<> {
    public:
      explicit CATDStarProducer(const edm::ParameterSet & iConfig);
      virtual ~CATDStarProducer() { }

      void produce(edm::Event & iEvent, const edm::EventSetup & iSetup) override;
      void mcMatching( vector<reco::GenParticle>& aGens, VertexCompositeCandidate& aRecos);
    private:

      edm::EDGetTokenT<edm::View<pat::Jet> >                    jetSrc_;
      edm::EDGetTokenT<edm::View<reco::GenParticle> >            mcSrc_;
      edm::EDGetTokenT<reco::VertexCollection>                vertexLabel_;

      const float gPionMass = 0.1396;
      const float gKaonMass = 0.4937;
      const float gD0Mass   = 1.86480;
      float d0MassWindow_, maxDeltaR_ ,d0MassCut_, matchingDeltaR_;
      unsigned int maxNumPFCand_;
      bool applyCuts_;


  };
  //bool pTComp( const reco::Candidate* a, const reco::Candidate* b) { return a->pt() > b->pt();  }
} // namespace

cat::CATDStarProducer::CATDStarProducer(const edm::ParameterSet & iConfig) :
  jetSrc_(consumes<edm::View<pat::Jet> >(iConfig.getParameter<edm::InputTag>("jetLabel"))),
   mcSrc_(consumes<edm::View<reco::GenParticle> >(iConfig.getParameter<edm::InputTag>("mcLabel"))),
  vertexLabel_(consumes<reco::VertexCollection>(iConfig.getParameter<edm::InputTag>("vertexLabel")))
{
  produces<vector<cat::SecVertex> >("D0Cand");
  produces<vector<cat::SecVertex> >("DstarCand");

  maxNumPFCand_ = iConfig.getParameter<int>("maxNumPFCand");
  d0MassWindow_ = iConfig.getParameter<double>("d0MassWindow");
  d0MassCut_ = iConfig.getParameter<double>("d0MassCut");
  maxDeltaR_  = iConfig.getParameter<double>("maxDeltaR");
  matchingDeltaR_  = iConfig.getParameter<double>("matchingDeltaR");
  applyCuts_ = iConfig.getParameter<bool>("applyCut");
}

  void
cat::CATDStarProducer::mcMatching( vector<reco::GenParticle>& aGens, VertexCompositeCandidate& aReco) {
  float minDR= 999.;
  //float minRelPt = 1.0;
  reco::GenParticle matchedGen;
  for( const auto& aGen : aGens ) {
      float deltaR = reco::deltaR( aGen, aReco);
      if ( deltaR < minDR ) { matchedGen = aGen; minDR = deltaR; }
  }
  if ( minDR < matchingDeltaR_ ) {
    aReco.addDaughter( matchedGen );
  }
  return;
}
  void
cat::CATDStarProducer::produce(edm::Event & iEvent, const edm::EventSetup & iSetup)
{

  Handle<reco::VertexCollection> recVtxs;
  iEvent.getByToken(vertexLabel_,recVtxs);

  if ( recVtxs->empty() ) {
    auto_ptr<vector<cat::SecVertex> >    D0_Out(new vector<cat::SecVertex>());
    auto_ptr<vector<cat::SecVertex> > Dstar_Out(new std::vector<cat::SecVertex>());
    iEvent.put(D0_Out   , "D0Cand");
    iEvent.put(Dstar_Out, "DstarCand");
    return ; 
  }
  reco::Vertex pv = recVtxs->at(0);

  Handle<edm::View<reco::GenParticle> > mcHandle;
  iEvent.getByToken(mcSrc_, mcHandle);

  vector<reco::GenParticle> d0s;
  vector<reco::GenParticle> dstars;

  for( const auto& aGenParticle : *mcHandle) {
    // If genParticle is D0,
    if ( std::abs(aGenParticle.pdgId()) == 421 ) d0s.push_back( aGenParticle);  
    else if ( std::abs(aGenParticle.pdgId()) ==  413 ) dstars.push_back( aGenParticle);
  } 
  Handle<edm::View<pat::Jet> > jetHandle;
  iEvent.getByToken(jetSrc_, jetHandle);

  auto_ptr<vector<cat::SecVertex> >    D0_Out(new vector<cat::SecVertex>());
  auto_ptr<vector<cat::SecVertex> > Dstar_Out(new std::vector<cat::SecVertex>());

  edm::ESHandle<TransientTrackBuilder> trackBuilder;
  iSetup.get<TransientTrackRecord>().get("TransientTrackBuilder",trackBuilder);

  typedef ROOT::Math::SMatrix<double, 3, 3, ROOT::Math::MatRepSym<double, 3> > SMatrixSym3D;
  typedef ROOT::Math::SVector<double, 3> SVector3;

  for (const pat::Jet & aPatJet : *jetHandle){
    std::vector<const reco::Candidate*> jetDaughters;
    std::vector<TransientTrack> tracks;
    
    unsigned int dau_size = aPatJet.numberOfDaughters();
    if ( dau_size < 3 ) continue;
    if ( dau_size > maxNumPFCand_ ) dau_size = maxNumPFCand_;
    for ( unsigned int pion_idx = 0 ; pion_idx< dau_size ; pion_idx++) {
      for ( unsigned int kaon_idx = 0 ; kaon_idx< dau_size ; kaon_idx++) {
        if ( pion_idx == kaon_idx ) continue;
        const pat::PackedCandidate* pionCand = dynamic_cast<const pat::PackedCandidate*>( aPatJet.daughter(pion_idx));
        const pat::PackedCandidate* kaonCand = dynamic_cast<const pat::PackedCandidate*>( aPatJet.daughter(kaon_idx));
        if ( abs(pionCand->pdgId()) != 211 || abs( kaonCand->pdgId()) != 211) continue;
        if ( pionCand->charge() * kaonCand->charge() != -1 ) continue;

        if ( reco::deltaR( *pionCand, *kaonCand) > maxDeltaR_ ) continue;

        tracks.clear();
        reco::TransientTrack pionTrack = trackBuilder->build( pionCand->pseudoTrack());
        reco::TransientTrack kaonTrack = trackBuilder->build( kaonCand->pseudoTrack());

        KalmanVertexFitter fitter(true);
        TransientVertex t_vertex;
        tracks.push_back( pionTrack);
        tracks.push_back( kaonTrack);
      
        try{
          t_vertex = fitter.vertex(tracks);
        }catch(std::exception& e) { std::cerr<<"Kalman Vertex Fitting error for D0: "<<e.what()<<std::endl; }

        Point vx;
        reco::Vertex vertex;
        double vtxChi2=0.0;
        int vtxNdof=0;
        bool fit_d0 = false; 
        if ( t_vertex.isValid() && t_vertex.totalChiSquared() > 0. )  {
          vertex = t_vertex;
          vx = Point(vertex.x(), vertex.y(), vertex.z());
          vtxChi2 = vertex.chi2(); 
          vtxNdof = (int)vertex.ndof();
          fit_d0 = true;
          //printf(" D0 vertex => x : %e y: %e z: %e\n",vx.x(),vx.y(),vx.z());
        }
        else vx = Point(0,0,0);


        auto D0 = pionCand->p4()+ kaonCand->p4();
        if ( abs(D0.M() - gD0Mass) > d0MassCut_) continue;
        const math::XYZTLorentzVector lv( D0.px(), D0.py(), D0.pz(), D0.E());
        auto vc = VertexCompositeCandidate(0, lv, vx, 421) ;  // + pdgId,
        cat::SecVertex D0Cand(vc);
        if ( fit_d0) { 
          D0Cand.setVProb( TMath::Prob( vtxChi2, vtxNdof));

          SVector3 distanceVectorXY(vertex.x() - pv.position().x(), vertex.y() - pv.position().y(), 0.);
          SVector3 distanceVector3D(vertex.x() - pv.position().x(), vertex.y() - pv.position().y(), vertex.z()- pv.position().z());
          double rVtxMag = ROOT::Math::Mag(distanceVectorXY);
          double rVtxMag3D = ROOT::Math::Mag(distanceVector3D);
          D0Cand.setLxy(rVtxMag);
          D0Cand.setL3D(rVtxMag3D);
        }
 
        D0Cand.addDaughter( *pionCand );
        D0Cand.addDaughter( *kaonCand );
        mcMatching( d0s, D0Cand);

        D0_Out->push_back( D0Cand );
        if ( abs( D0.M() - gD0Mass) < d0MassWindow_ ) {
          for( unsigned int extra_pion_idx = 0 ;  extra_pion_idx < dau_size ; extra_pion_idx++) {
            if ( extra_pion_idx== pion_idx || extra_pion_idx == kaon_idx) continue;
            const pat::PackedCandidate* pion2Cand = dynamic_cast<const pat::PackedCandidate*>( aPatJet.daughter(extra_pion_idx));
            if ( abs(pion2Cand->pdgId()) != 211) continue;
            if ( reco::deltaR(D0Cand, *pion2Cand  )> maxDeltaR_) continue;
            auto Dstar = D0Cand.p4() + pion2Cand->p4();
            const math::XYZTLorentzVector lv2( Dstar.Px(), Dstar.Py(), Dstar.Pz(), Dstar.E());
            reco::TransientTrack pion2Track = trackBuilder->build( pion2Cand->pseudoTrack());
            tracks.clear();

            tracks.push_back( pionTrack);
            tracks.push_back( kaonTrack);
            tracks.push_back( pion2Track );
           
            bool fit_dstar = false; 
            try{
              t_vertex = fitter.vertex(tracks);
            }catch(std::exception& e) { std::cerr<<"Kalman Vertex Fitting error for D*: "<<e.what()<<std::endl; }
            if ( t_vertex.isValid() && t_vertex.totalChiSquared() > 0. )  {
              const reco::Vertex vertex = t_vertex; 
              vx = Point(vertex.x(), vertex.y(), vertex.z()); 
              vtxChi2 = vertex.chi2(); 
              vtxNdof = vertex.ndof();
              fit_dstar = true;
              //printf(" D* vertex => x : %e y: %e z: %e\n",vx.x(),vx.y(),vx.z());
            }
            else vx = Point(0,0,0);
            
            auto vc2 = VertexCompositeCandidate(pion2Cand->charge(), lv2, vx, pion2Cand->charge()*413) ;  // + pdgId,
            cat::SecVertex DstarCand(vc2);
            DstarCand.addDaughter( *pionCand );
            DstarCand.addDaughter( *kaonCand );
            DstarCand.addDaughter( *pion2Cand );
            mcMatching( dstars, DstarCand );
            if ( fit_dstar) {
              DstarCand.setVProb( TMath::Prob( vtxChi2, (int) vtxNdof));

              SVector3 distanceVectorXY(vertex.x() - pv.position().x(), vertex.y() - pv.position().y(), 0.);
              SVector3 distanceVector3D(vertex.x() - pv.position().x(), vertex.y() - pv.position().y(), vertex.z()- pv.position().z());
              double rVtxMag = ROOT::Math::Mag(distanceVectorXY);
              double rVtxMag3D = ROOT::Math::Mag(distanceVector3D);
              DstarCand.setLxy(rVtxMag);
              DstarCand.setL3D(rVtxMag3D);
            
            } 
      
            Dstar_Out->push_back( DstarCand );
          }
        }
      }

    }
  }
  iEvent.put(D0_Out   , "D0Cand");
  iEvent.put(Dstar_Out, "DstarCand");
}

#include "FWCore/Framework/interface/MakerMacros.h"
using namespace cat;
DEFINE_FWK_MODULE(CATDStarProducer);
