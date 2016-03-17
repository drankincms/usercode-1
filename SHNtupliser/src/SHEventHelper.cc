#include "SHarper/SHNtupliser/interface/SHEventHelper.h"
#include "SHarper/SHNtupliser/interface/SHEvent.hh"
#include "SHarper/SHNtupliser/interface/HackedFuncs.h"
#include "SHarper/HEEPAnalyzer/interface/HEEPEvent.h"
#include "SHarper/SHNtupliser/interface/SHEleCMSSWStructs.hh"
#include "SHarper/SHNtupliser/interface/GeomFuncs.hh"
#include "SHarper/SHNtupliser/interface/SHTrigSumMaker.h"
#include "SHarper/SHNtupliser/interface/PFFuncs.h"
#include "SHarper/SHNtupliser/interface/GenFuncs.h"
#include "SHarper/SHNtupliser/interface/LogErr.hh"

#include "DataFormats/EcalDetId/interface/EBDetId.h"
#include "DataFormats/EcalDetId/interface/EEDetId.h"
#include "DataFormats/HcalDetId/interface/HcalDetId.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"
#include "DataFormats/HcalRecHit/interface/HcalRecHitCollections.h"

#include "DataFormats/MuonReco/interface/MuonCocktails.h"

#include "TrackingTools/GsfTools/interface/MultiTrajectoryStateTransform.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"

#include "TVector3.h"
#include "TBits.h"

#include "DataFormats/RecoCandidate/interface/RecoEcalCandidate.h"
#include "DataFormats/EgammaCandidates/interface/Electron.h"
#include "DataFormats/RecoCandidate/interface/RecoEcalCandidateFwd.h"
#include "DataFormats/EgammaCandidates/interface/ElectronFwd.h"
#include "DataFormats/L1Trigger/interface/L1EmParticleFwd.h"
#include "DataFormats/L1Trigger/interface/L1EmParticle.h"

#include "DataFormats/METReco/interface/PFMET.h"

#include "FWCore/Common/interface/TriggerNames.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "DataFormats/Scalers/interface/DcsStatus.h"
#include "DataFormats/RecoCandidate/interface/RecoEcalCandidateIsolation.h"

//horrible hack of a function to fix annoying things
void fixClusterShape(const reco::CaloCluster& seedCluster,const heep::Event& heepEvent,SHElectron& ele);

#include "RecoEgamma/EgammaTools/interface/ConversionFinder.h"
SHEventHelper::SHEventHelper():
  isMC_(false),
  minEtToPromoteSC_(20),
  fillFromGsfEle_(true),
  applyMuonId_(true)
{
  initEcalHitVec_();
  initHcalHitVec_();
}

void SHEventHelper::setup(const edm::ParameterSet& conf,edm::ConsumesCollector && cc)
{ 
  minEtToPromoteSC_ = conf.getParameter<double>("minEtToPromoteSC");
  eventWeight_ = conf.getParameter<double>("sampleWeight");
  datasetCode_ = conf.getParameter<int>("datasetCode");    
  applyMuonId_ = conf.getParameter<bool>("applyMuonId");
  fillFromGsfEle_ = conf.getParameter<bool>("fillFromGsfEle");


  isMC_=datasetCode_!=0;
  branches_.setup(conf);
  if(branches_.addHLTDebug) cc.consumesMany<reco::RecoEcalCandidateIsolationMap>();
  
}



void SHEventHelper::makeSHEvent(const heep::Event & heepEvent, SHEvent& shEvent)const

{   
 
  shEvent.clear(); //reseting the event 
  
  addEventPara(heepEvent,shEvent); //this must be filled before (ele + mu need beam spot info)
  //it is currently critical that calo hits are filled first as they are used in constructing the basic clusters
  if(branches_.addCaloHits) addCaloHits(heepEvent,shEvent);
  if(branches_.addCaloTowers) addCaloTowers(heepEvent,shEvent);
  if(branches_.addSuperClus) addSuperClusters(heepEvent,shEvent);
  if(branches_.addEles) addElectrons(heepEvent,shEvent);
  if(branches_.addPreShowerClusters) addPreShowerClusters(heepEvent,shEvent);
  if(branches_.addMuons) addMuons(heepEvent,shEvent); 
  if(branches_.addTrigSum) addTrigInfo(heepEvent,shEvent);
  if(branches_.addJets) addJets(heepEvent,shEvent);
  if(branches_.addMet) addMet(heepEvent,shEvent);
  if(branches_.addIsolTrks) addIsolTrks(heepEvent,shEvent);
  if(branches_.addGenInfo) addGenInfo(heepEvent,shEvent);
  if(branches_.addPUInfo) addPUInfo(heepEvent,shEvent);
  if(branches_.addPFCands) addPFCands(heepEvent,shEvent);
  if(branches_.addPFClusters) addPFClusters(heepEvent,shEvent);
}

void SHEventHelper::addEventPara(const heep::Event& heepEvent, SHEvent& shEvent)const
{
  shEvent.setRunnr(heepEvent.runnr());
  shEvent.setEventnr(heepEvent.eventnr());
  shEvent.setIsMC(isMC_);
  shEvent.setDatasetCode(datasetCode_);
  shEvent.setWeight(eventWeight_);
  shEvent.setGenEventPtHat(heepEvent.genEventPtHat());
  shEvent.setBX(heepEvent.bx());
  shEvent.setLumiSec(heepEvent.lumiSec());
  shEvent.setTime(heepEvent.time());
  shEvent.setOrbitNumber(heepEvent.orbitNumber());
  shEvent.setNrVertices(-1);
  shEvent.setPreScaleCol(heepEvent.preScaleColumn());
   TVector3 vtxPos;
   if(heepEvent.handles().vertices.isValid() && heepEvent.handles().vertices->size()>0){  
     const reco::Vertex& vertex = heepEvent.handles().vertices->front();
     vtxPos.SetXYZ(vertex.x(),vertex.y(),vertex.z()); 
     shEvent.setNrVertices(heepEvent.handles().vertices->size());
     const std::vector<reco::Vertex>& vertices = *(heepEvent.handles().vertices);
     for(size_t vtxNr=0;vtxNr<vertices.size();vtxNr++){
       shEvent.addVertex(vertices[vtxNr]);
     }

   }
   shEvent.setVertex(vtxPos);
   TVector3 bs;
   if(heepEvent.handles().beamSpot.isValid()){
     math::XYZPoint bsCMSSW = heepEvent.handles().beamSpot->position();
     bs.SetXYZ(bsCMSSW.x(),bsCMSSW.y(),bsCMSSW.z());
   }
   shEvent.setBeamSpot(bs);
  
   if(heepEvent.handles().eleRhoCorr.isValid()) shEvent.setEleRhoCorr(heepEvent.eleRhoCorr());
   else shEvent.setEleRhoCorr(-999);
   if(heepEvent.handles().eleRhoCorr2012.isValid()) shEvent.setRhoCorr(heepEvent.eleRhoCorr2012());
   else shEvent.setRhoCorr(-999);

   if(heepEvent.passEcalLaserFilter()) shEvent.setFlags(1);
   else shEvent.setFlags(0);

    
}

//this now handles the sc promotion via photons
//photons have just a H/E<0.5 cut which is fine for our needs
void SHEventHelper::addElectrons(const heep::Event& heepEvent, SHEvent& shEvent)const
{  

  if(!heepEvent.handles().gsfEle.isValid()) return; //protection when the colleciton doesnt exist

  // const std::vector<heep::Ele>& electrons = heepEvent.heepEles();
  const std::vector<reco::GsfElectron>& electrons = heepEvent.gsfEles();
  const std::vector<reco::Photon>& photons = heepEvent.recoPhos();
  //std::cout <<"nr electrons "<<electrons.size()<<std::endl;
  // std::cout <<"nr photons "<<photons.size()<<std::endl;
  for(size_t phoNr=0;phoNr<photons.size();phoNr++){
    const reco::SuperCluster& sc = *photons[phoNr].superCluster();
    size_t eleNr = matchToEle(sc,electrons);
    if(eleNr<electrons.size()) addElectron(heepEvent,shEvent,electrons[eleNr]);
    
    else if(sc.energy()*sin(sc.position().theta())>minEtToPromoteSC_ && minEtToPromoteSC_<10000) addElectron(heepEvent,shEvent,photons[phoNr]);
  }
  
}

void SHEventHelper::addElectron(const heep::Event& heepEvent,SHEvent& shEvent,const reco::GsfElectron& gsfEle)const
{
  MultiTrajectoryStateTransform trajStateTransform(heepEvent.handles().trackGeom.product(),heepEvent.handles().bField.product());
  shEvent.addElectron(gsfEle,shEvent.getCaloHits());
  SHElectron* shEle = shEvent.getElectron(shEvent.nrElectrons()-1); 
  shEle->setPassMVAPreSel(shEle->isolMVA()>=-0.1);
  shEle->setPassPFlowPreSel(gsfEle.mvaOutput().status==3); 
 
  if(shEle->seedId()!=0 && false){
    
    const TVector3& seedPos = GeomFuncs::getCell(shEle->seedId()).pos();
    GlobalPoint posGP(seedPos.X(),seedPos.Y(),seedPos.Z());  
    
    //  std::cout <<" pt "<<gsfEle.gsfTrack()->pt()<<std::endl;
    TrajectoryStateOnSurface outTSOS = trajStateTransform.outerStateOnSurface(*gsfEle.gsfTrack());
    TrajectoryStateOnSurface innTSOS = trajStateTransform.innerStateOnSurface(*gsfEle.gsfTrack());
    TrajectoryStateOnSurface outToSeedTSOS = trajStateTransform.extrapolatedState(outTSOS,posGP);
    TrajectoryStateOnSurface innToSeedTSOS = trajStateTransform.extrapolatedState(innTSOS,posGP);
    
    TVector3 dummy; 
    dummy.SetPtEtaPhi(0.00001,-10,0);
    if(outToSeedTSOS.isValid()){
      TVector3 outToSeedPos(outToSeedTSOS.globalPosition().x(),outToSeedTSOS.globalPosition().y(),outToSeedTSOS.globalPosition().z()); 
      shEle->setPosTrackOutToSeed(outToSeedPos); 
    }else shEle->setPosTrackOutToSeed(dummy);
    
    if(innToSeedTSOS.isValid()){
      TVector3 innToSeedPos(innToSeedTSOS.globalPosition().x(),innToSeedTSOS.globalPosition().y(),innToSeedTSOS.globalPosition().z());
      shEle->setPosTrackInnToSeed(innToSeedPos);  
    }else shEle->setPosTrackInnToSeed(dummy);
   
  }
}


void SHEventHelper::addElectron(const heep::Event& heepEvent,SHEvent& shEvent,const reco::Photon& photon)const
{
  shEvent.addElectron(photon,shEvent.getCaloHits());
}


void SHEventHelper::addMuons(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  if(heepEvent.handles().muon.isValid()){
    for(size_t muNr=0;muNr<heepEvent.muons().size();muNr++){
      const reco::Muon& muon = heepEvent.muons()[muNr];
      
      if(muon.isGlobalMuon() && (!applyMuonId_ || passMuonId(muon,heepEvent))) shEvent.addMuon(muon);
    }
  }
}

bool SHEventHelper::passMuonId(const reco::Muon& muon,const heep::Event& heepEvent)
{
  if(muon.isGlobalMuon() && 
     muon.globalTrack()->hitPattern().numberOfValidMuonHits()>0 &&
     muon.numberOfMatchedStations()>1 &&
     muon.globalTrack()->hitPattern().numberOfValidPixelHits()>0 && 
     muon.globalTrack()->hitPattern().trackerLayersWithMeasurement() > 5 ){
    // reco::TrackRef cktTrackRef = (muon::tevOptimized(muon, 200, 17., 40., 0.25)).first; //53X
    reco::TrackRef cktTrackRef = (muon::tevOptimized(muon)).first; //60X
    const reco::Track& cktTrack = *cktTrackRef;
    const reco::Vertex& vertex = heepEvent.handles().vertices->front();
    if(cktTrack.ptError()/cktTrack.pt()<0.3 && 
       fabs(cktTrack.dxy(vertex.position())) < 0.2 &&
       fabs(cktTrack.dz(vertex.position())) < 0.5) return true;
    
  }
  return false;
}

size_t SHEventHelper::matchToEle(const reco::SuperCluster& superClus,const std::vector<reco::GsfElectron> eles)const
{
  for(size_t eleNr=0;eleNr<eles.size();eleNr++){
    const reco::GsfElectron& ele = eles[eleNr];
    if(ele.superCluster()->seed()->hitsAndFractions()[0].first==superClus.seed()->hitsAndFractions()[0].first) return eleNr;
  }
  return eles.size();
}

size_t SHEventHelper::matchToEle(const reco::SuperCluster& superClus,const std::vector<heep::Ele> eles)const
{
  for(size_t eleNr=0;eleNr<eles.size();eleNr++){
    const reco::GsfElectron& ele = eles[eleNr].gsfEle();
    if(ele.superCluster()->seed()->hitsAndFractions()[0].first==superClus.seed()->hitsAndFractions()[0].first) return eleNr;
  }
  return eles.size();
}

void SHEventHelper::addSuperClusters(const heep::Event& heepEvent, SHEvent& shEvent)const
{
  if(heepEvent.handles().superClusEB.isValid()){
    const std::vector<reco::SuperCluster>& superClusEB = heepEvent.superClustersEB(); 
    for(size_t superClusNr=0;superClusNr<superClusEB.size();superClusNr++){
      shEvent.addSuperCluster(superClusEB[superClusNr],shEvent.getCaloHits());
    }
  }
  if(heepEvent.handles().superClusEB.isValid()){
  //now endcap
    const std::vector<reco::SuperCluster>& superClusEE = heepEvent.superClustersEE(); 
    for(size_t superClusNr=0;superClusNr<superClusEE.size();superClusNr++){
      shEvent.addSuperCluster(superClusEE[superClusNr],shEvent.getCaloHits());
    }
  }
}

void SHEventHelper::addPreShowerClusters(const heep::Event& heepEvent, SHEvent& shEvent)const
{
  if(heepEvent.handles().preShowerClusX.isValid()){
    const std::vector<reco::PreshowerCluster>& preShowerClus = *(heepEvent.handles().preShowerClusX);
    for(size_t clusNr=0;clusNr<preShowerClus.size();clusNr++){
      shEvent.addPreShowerCluster(preShowerClus[clusNr]);
    }
  }
  if(heepEvent.handles().preShowerClusY.isValid()){
    const std::vector<reco::PreshowerCluster>& preShowerClus = *(heepEvent.handles().preShowerClusY); 
    for(size_t clusNr=0;clusNr<preShowerClus.size();clusNr++){
      shEvent.addPreShowerCluster(preShowerClus[clusNr]);
    }
  }
}

void SHEventHelper::addCaloHits(const heep::Event& heepEvent, SHEvent& shEvent)const
{
  addEcalHits(heepEvent,shEvent);
  addHcalHits(heepEvent,shEvent);
}

//we first zero the vector of the ecal hits (note we keep this persistant because we dont want to keep allocating memory)
//we then loop over the barrel and endcap hits and add them to the vector
//we then add the vector to the event
void SHEventHelper::addEcalHits(const heep::Event& heepEvent, SHEvent& shEvent)const
{
  for(size_t i=0;i<ecalHitVec_.size();i++) ecalHitVec_[i].setNrgy(-999.);
  
  const EcalRecHitCollection* ebHits =heepEvent.handles().ebRecHits.isValid() ? heepEvent.ebHitsFull() : 
    heepEvent.handles().ebReducedRecHits.isValid() ? &(*heepEvent.handles().ebReducedRecHits) : NULL;
  const EcalRecHitCollection* eeHits =heepEvent.handles().eeRecHits.isValid() ? heepEvent.eeHitsFull() : 
    heepEvent.handles().eeReducedRecHits.isValid() ? &(*heepEvent.handles().eeReducedRecHits) : NULL;
  


  if(ebHits!=NULL){
    for(EcalRecHitCollection::const_iterator hitIt = ebHits->begin();
	hitIt!=ebHits->end();++hitIt){
      SHCaloHit& shHit = ecalHitVec_[ecalHitHash_(hitIt->detid())];
      shHit.setNrgy(hitIt->energy());
      shHit.setTime(hitIt->time());
      //shHit.setFlag(hitIt->flags());
      shHit.setFlag(0);//temp fix
      shHit.setFlagBits(getEcalFlagBits_(*hitIt));
      
    }
  }//end of null check on ebHits

  if(eeHits!=NULL){
    for(EcalRecHitCollection::const_iterator hitIt = eeHits->begin();
	hitIt!=eeHits->end();++hitIt){
      SHCaloHit& shHit = ecalHitVec_[ecalHitHash_(hitIt->detid())];
      shHit.setNrgy(hitIt->energy());
      shHit.setTime(hitIt->time());  
      //shHit.setFlag(hitIt->flags()); 
      shHit.setFlag(0),
      shHit.setFlagBits(getEcalFlagBits_(*hitIt));
      
    }
  }//end of null check on eeHits

  shEvent.addEcalHits(ecalHitVec_);

}

void SHEventHelper::addHcalHits(const heep::Event& heepEvent, SHEvent& shEvent)const
{
  for(size_t i=0;i<hcalHitVec_.size();i++) hcalHitVec_[i].setNrgy(-999.);
  
  const HBHERecHitCollection* hcalHits = heepEvent.handles().hbheRecHits.isValid() ? heepEvent.hbheHits() : NULL;
  
  if(hcalHits!=NULL){
    for(HBHERecHitCollection::const_iterator hitIt = hcalHits->begin();
	hitIt!=hcalHits->end();++hitIt){
      hcalHitVec_[hcalHitHash_(hitIt->detid())].setNrgy(hitIt->energy());
    }
  }//end of null check on hcal hits

  shEvent.addHcalHits(hcalHitVec_);
}

void SHEventHelper::addCaloTowers(const heep::Event& heepEvent, SHEvent& shEvent)const
{

  const CaloTowerCollection* caloTowers = heepEvent.handles().caloTowers.isValid() ? &(*heepEvent.handles().caloTowers) : NULL;
  if(caloTowers!=NULL){
    for(CaloTowerCollection::const_iterator towerIt=caloTowers->begin();
	towerIt!=caloTowers->end();++towerIt){
      if(towerIt->id().ietaAbs()<=29){
	SHCaloTower caloTower(towerIt->id(),towerIt->emEnergy(),
			      towerIt->hadEnergy()-towerIt->hadEnergyHeOuterLayer(),
			      towerIt->hadEnergyHeOuterLayer(),
			      towerIt->eta(),towerIt->phi());
	shEvent.addCaloTower(caloTower);
      }
    }
  }

}

void SHEventHelper::addGenInfo(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  GenFuncs::fillGenInfo(heepEvent,shEvent.getGenInfo());
}

void SHEventHelper::addPFCands(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  if(heepEvent.handles().vertices.isValid()){
    reco::VertexRef mainVtx(heepEvent.handles().vertices,0); 
    if(heepEvent.handles().pfCandidate.isValid() &&
       heepEvent.handles().gsfEle.isValid() && 
       heepEvent.handles().gsfEleToPFCandMap.isValid()){
      PFFuncs::fillPFCands(&shEvent,0.5,shEvent.getPFCands(),heepEvent.handles().pfCandidate,
			   mainVtx,heepEvent.handles().vertices,
			   *(heepEvent.handles().gsfEleToPFCandMap.product()),
			   heepEvent.handles().gsfEle);
    }
  }
}

void SHEventHelper::addPFClusters(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  if(heepEvent.handles().pfClustersECAL.isValid() && heepEvent.handles().pfClustersHCAL.isValid() &&
     heepEvent.handles().superClusEB.isValid() && heepEvent.handles().superClusEE.isValid()){

    fillPFClustersECAL_(&shEvent,0.5,shEvent.getPFClusters(),heepEvent.pfClustersECAL(),heepEvent.superClustersEB(),heepEvent.superClustersEE());
    fillPFClustersHCAL_(&shEvent,0.5,shEvent.getPFClusters(),heepEvent.pfClustersHCAL());
  }
}

void SHEventHelper::addPUInfo(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  if(heepEvent.handles().pileUpMCInfo.isValid()){
    for(auto& puInfo : *heepEvent.handles().pileUpMCInfo){
      shEvent.getPUSum().addPUInfo( puInfo.getBunchCrossing(),puInfo.getPU_NumInteractions(),puInfo.getTrueNumInteractions());
    }
  }
}

void SHEventHelper::addTrigInfo(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  SHTrigSumMaker::makeSHTrigSum(heepEvent,shEvent.getTrigSum());
  if(branches_.addHLTDebug) SHTrigSumMaker::associateEgHLTDebug(heepEvent,shEvent.getTrigSum());
  
}

void SHEventHelper::addJets(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  if(heepEvent.handles().jet.isValid()){
    for(size_t jetNr=0;jetNr<heepEvent.jets().size();jetNr++){
      if(heepEvent.jets()[jetNr].et()>15) shEvent.addJet(heepEvent.jets()[jetNr]);
    }
  }
}

void SHEventHelper::addIsolTrks(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  TVector3 trkP3,trkVtxPos;
  const float minPtCut=-1;

  if(heepEvent.handles().ctfTrack.isValid()){
    const std::vector<reco::Track>& tracks = heepEvent.ctfTracks();
    
    for(size_t trkNr=0;trkNr<tracks.size();trkNr++){
      const reco::Track& trk = tracks[trkNr];
      trkP3.SetXYZ(trk.px(),trk.py(),trk.pz());
      trkVtxPos.SetXYZ(trk.vx(),trk.vy(),trk.vz());
      int vertexNr=-1;
      if(heepEvent.handles().vertices.isValid()) vertexNr = getVertexNr(trk,*heepEvent.handles().vertices);
      //std::cout <<"trk chi2 "<<trk.chi2()<<" trk err "<<trk.ptError()<<std::endl;
      if(trkP3.Pt()>minPtCut) shEvent.addIsolTrk(trkP3,trkVtxPos,trk.charge()>0,vertexNr,trk.chi2(),trk.ndof());
    }
  } 
}

void SHEventHelper::addMet(const heep::Event& heepEvent,SHEvent& shEvent)const
{
  //why yes I might be throwing away the patty goodness, whoops
  
  SHMet shMet;
  SHMet shPFMet;
  if(heepEvent.handles().met.isValid() && !heepEvent.mets().empty()){
    const pat::MET& met = heepEvent.mets()[0];
    if(met.isPFMET()){
      shPFMet.setMet(met.et()*cos(met.phi()),met.et()*sin(met.phi()));
      shPFMet.setSumEmEt(0,0,0);
      shPFMet.setSumHadEt(0,0,0,0);
    }else{
      shMet.setMet(met.mEtCorr()[0].mex,met.mEtCorr()[0].mey);
      shMet.setSumEmEt(met.emEtInEB(),met.emEtInEE(),met.emEtInHF());
      shMet.setSumHadEt(met.hadEtInHB(),met.hadEtInHE(),met.hadEtInHO(),met.hadEtInHF());
    }
  }
  shEvent.setMet(shMet); 
  shEvent.setPFMet(shPFMet);
 
//   edm::Handle<edm::View<reco::PFMET> > pfMETHandle;
//   heepEvent.event().getByLabel("pfMet",pfMETHandle);
//   if(pfMETHandle.isValid()){
//     const reco::PFMET& pfMET = pfMETHandle->front();
//     shPFMet.setMet(pfMET.et()*cos(pfMET.phi()),pfMET.et()*sin(pfMET.phi()));
//     shPFMet.setSumEmEt(0,0,0);
//     shPFMet.setSumHadEt(0,0,0,0);
//   }
 // std::cout <<"pf met "<<pfMET.et()<<" sh met "<<shMet.mEt()<<" pf phi "<<pfMET.phi()<<" sh phi "<<shMet.phi()<<std::endl;
 

}  


int SHEventHelper::ecalHitHash_(const DetId detId)const
{
  if(detId.det()==DetId::Ecal){
    if(detId.subdetId()==EcalBarrel){
      EBDetId ebDetId(detId);
      return ebDetId.hashedIndex();
    }else if(detId.subdetId()==EcalEndcap){
      EEDetId eeDetId(detId);
      return eeDetId.hashedIndex()+EBDetId::MAX_HASH+1;
    }
  } 
  return -1; //not in ecal barrel or endcap  
}


int SHEventHelper::hcalHitHash_(const DetId detId)const
{
  return DetIdTools::calHashHcal(detId.rawId());
  //HcalDetId hcalDetId(detId);
  //return DetIhashed_index();
}


void SHEventHelper::initEcalHitVec_()const
{
  //first make sure the vector is the approprate size and has all null entries
  SHCaloHit nullHit(0,-999.);
  std::vector<SHCaloHit> tempVector(EBDetId::MAX_HASH+1+(EEDetId::kSizeForDenseIndexing),nullHit);
  ecalHitVec_.swap(tempVector);

  
  //now init barrel, place det ids at the correct positions
  for(int barrelIEta=-85;barrelIEta<=85;barrelIEta++){
    if(barrelIEta==0) continue;
    for(int barrelIPhi=1;barrelIPhi<=360;barrelIPhi++){
      EBDetId detId(barrelIEta,barrelIPhi);
      ecalHitVec_[detId.hashedIndex()].setDetId(detId.rawId());
    }
  }

  //now init endcap
  for(int xNr=1;xNr<=100;xNr++){
    for(int yNr=1;yNr<=100;yNr++){
      for(int zSide=-1;zSide<=1;zSide+=2){
	if(EEDetId::validDetId(xNr,yNr,zSide)){
	  EEDetId detId(xNr,yNr,zSide);
	  ecalHitVec_[detId.hashedIndex()+EBDetId::MAX_HASH+1].setDetId(detId.rawId());
	}//end valid det id check
      }//z side loop
    }//end phi loop
  }//end eta loop

  //std::cout <<"checking indexes"<<std::endl;
  //for(size_t i=0;i<ecalHitVec_.size();i++){
    //if(ecalHitVec_[i].detId()==0) std::cout <<"ecal index "<<i<< " is "<<ecalHitVec_[i].detId()<<std::endl;
    // }

}


void SHEventHelper::initHcalHitVec_()const
{
  SHCaloHit nullHit(0,-999.);
  std::vector<SHCaloHit> tempVector(kNrHcalBarrelCrys_+kNrHcalEndcapCrys_,nullHit);
  hcalHitVec_.swap(tempVector);

  //fill barrel and endcap seperately
  //first barrel
  for(int iEtaAbs=1;iEtaAbs<=16;iEtaAbs++){
    for(int side=0;side<=1;side++){
       int iEta = iEtaAbs*(2*side-1);
       for(int iPhi=1;iPhi<=72;iPhi++){
	 int maxDepth = 1;
	 if(iEtaAbs>=15) maxDepth=2;
	 for(int depth=1;depth<=maxDepth;depth++){
	   if(HackedFuncs::validHcalDetId(HcalBarrel,iEta,iPhi,depth)){
	     HcalDetId detId(HcalBarrel,iEta,iPhi,depth);
	     size_t indx = DetIdTools::calHashHcal(detId.rawId());
	     if(indx>=hcalHitVec_.size()){
	       LogErr<<" error for HCAL barrel with eta "<<iEta<<" "<<iPhi<<" "<<depth<<" has a bad hash "<<indx<<" (max="<<hcalHitVec_.size()<<std::endl;
	     }else{
	       if(hcalHitVec_[indx].detId()!=0){
		 LogErr<<" error for HCAL barrel with eta "<<iEta<<" "<<iPhi<<" "<<depth<<", index "<<indx<<" already used "<<std::endl;
	       }else hcalHitVec_[indx].setDetId(detId.rawId());
	     }	  
	   }//end vaild det id
	 }//end depth loop 
       }//end iphi loop
    }//end side loop
  }//end barrel ieta loop

  for(int iEtaAbs=16;iEtaAbs<=29;iEtaAbs++){
    for(int side=0;side<=1;side++){
      int iEta = iEtaAbs*(2*side-1);
      for(int iPhi=1;iPhi<=72;iPhi++){
	for(int depth=1;depth<=3;depth++){
	  if(HackedFuncs::validHcalDetId(HcalEndcap,iEta,iPhi,depth)){
	    HcalDetId detId(HcalEndcap,iEta,iPhi,depth);
	    size_t indx = DetIdTools::calHashHcal(detId.rawId());
	    if(indx>=hcalHitVec_.size()){
	       LogErr<<" error for HCAL endcap with eta "<<iEta<<" "<<iPhi<<" "<<depth<<" has a bad hash "<<indx<<" (max="<<hcalHitVec_.size()<<std::endl;
	     }else{
	       if(hcalHitVec_[indx].detId()!=0){
		 LogErr<<" error for HCAL endcap with eta "<<iEta<<" "<<iPhi<<" "<<depth<<", index "<<indx<<" already used "<<std::endl;
	       }else hcalHitVec_[indx].setDetId(detId.rawId());
	     }	      
	  }
	}//end depth loop
      }//end iphi loop
    }//end side loop
  }//end iEtaAbs loop
  std::cout <<"checking indexes"<<std::endl;
  for(size_t i=0;i<hcalHitVec_.size();i++){
    if(hcalHitVec_[i].detId()==0) std::cout <<"hcal index "<<i<< " is "<<hcalHitVec_[i].detId()<<std::endl;
  }
}

//because they dont have an accessor :(
uint32_t SHEventHelper::getEcalFlagBits_(const EcalRecHit& hit)
{
  uint32_t bits=0x0;
  for(int bitNr=0;bitNr<32;bitNr++){
    bits |= hit.checkFlag(bitNr)<<bitNr;
  }
  return bits;

}

int SHEventHelper::getVertexNr(const reco::TrackBase& track,const std::vector<reco::Vertex>& vertices)
{ //code inspired by Florian's PFNoPU class
  int returnVal = -1;
 
  float bestweight=0;
  
  for(size_t vertexNr=0;vertexNr<vertices.size();vertexNr++){
    const reco::Vertex& vtx = vertices[vertexNr];
    
    // loop on tracks in vertices
    for(reco::Vertex::trackRef_iterator trkIt=vtx.tracks_begin();trkIt!=vtx.tracks_end(); ++trkIt) {

      const reco::TrackBaseRef& vertexTrkRef = *trkIt;

      // one of the tracks in the vertex is the same as 
      // the track considered in the function
      if(vertexTrkRef.get() ==  &track) {
        float weight = vtx.trackWeight(vertexTrkRef);
        //select the vertex for which the track has the highest weight
        if (weight > bestweight){
          bestweight=weight;
          returnVal = vertexNr;
        }
      }
    }//end loop over vertex tracks
    
  }//end loop over vertices
 
  if(returnVal<0) returnVal = SHEventHelper::getVertexNrClosestZ(track,vertices);
  return returnVal;
}

int SHEventHelper::getVertexNrClosestZ(const reco::TrackBase& track,const std::vector<reco::Vertex>& vertices)
{
  
  double dzMin = 999.;
  int bestVertexNr = -1;
  for(size_t vertexNr=0;vertexNr<vertices.size();vertexNr++){
    double dz = fabs(track.vz() - vertices[vertexNr].z());
    if(dz<dzMin) {
      dzMin = dz;
      bestVertexNr=vertexNr;
    }
  }
  return bestVertexNr;
}
//dummy function for releases without this function
//void fixClusterShape(const reco::CaloCluster& seedCluster,const heep::Event& heepEvent,SHElectron& ele){}

#include "Geometry/CaloTopology/interface/CaloTopology.h"
#include "RecoLocalCalo/EcalRecAlgos/interface/EcalSeverityLevelAlgo.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHit.h"
#include "RecoEcal/EgammaCoreTools/interface/EcalClusterTools.h"
#include "RecoLocalCalo/EcalRecAlgos/interface/EcalSeverityLevelAlgoRcd.h"
#include "CommonTools/Utils/interface/StringToEnumValue.h"
#include "Geometry/Records/interface/CaloTopologyRecord.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
void fixClusterShape(const reco::CaloCluster& seedCluster,const heep::Event& heepEvent,SHElectron& ele)
{
  
  const EcalRecHitCollection* ebHits =heepEvent.handles().ebRecHits.isValid() ? heepEvent.ebHitsFull() : 
    heepEvent.handles().ebReducedRecHits.isValid() ? &(*heepEvent.handles().ebReducedRecHits) : NULL;
  const EcalRecHitCollection* eeHits =heepEvent.handles().eeRecHits.isValid() ? heepEvent.eeHitsFull() : 
    heepEvent.handles().eeReducedRecHits.isValid() ? &(*heepEvent.handles().eeReducedRecHits) : NULL;
  
  float sigmaEtaEta=0;
  float sigmaIEtaIEta=0;
  float e2x5Max=0;
  float e5x5=0;
  float e1x5=0;
  
  if(ebHits && eeHits){
    
    DetId seedXtalId = seedCluster.hitsAndFractions()[0].first ;
    int detector = seedXtalId.subdetId() ;
    const EcalRecHitCollection* recHits = detector==EcalBarrel ? ebHits : eeHits; 
    edm::ESHandle<CaloTopology> topology;
    edm::ESHandle<CaloGeometry> geom;
//     edm::ESHandle<EcalSeverityLevelAlgo> severityLevelAlgo;
//     heepEvent.eventSetup().get<CaloTopologyRecord>().get(topology);
//     heepEvent.eventSetup().get<EcalSeverityLevelAlgoRcd>().get(severityLevelAlgo);
//     heepEvent.eventSetup().get<CaloGeometryRecord>().get(geom);
//     std::vector<std::string> recHitFlagsToBeExcludedBarrelStr = {
//       "kFaultyHardware",
//       "kPoorCalib",
//       "kTowerRecovered",
//       "kDead"
//     };
//     std::vector<std::string> recHitSeverityToBeExcludedStr = {
//       "kWeird",
//       "kBad",
//       "kTime"
//     };
    
//     std::vector<std::string> recHitFlagsToBeExcludedEndcapStr = {
//       "kFaultyHardware",
//       "kPoorCalib",
//       "kSaturated",
//       "kLeadingEdgeRecovered",
//       "kNeighboursRecovered",
//       "kTowerRecovered",
//       "kDead",
//       "kWeird"
//     };
//     std::vector<int> recHitSeverityToBeExcluded = StringToEnumValue<EcalSeverityLevel::SeverityLevel>(recHitSeverityToBeExcludedStr);
//     std::vector<int> recHitFlagsToBeExcludedBarrel= StringToEnumValue<EcalRecHit::Flags>(recHitFlagsToBeExcludedBarrelStr);  
//     std::vector<int> recHitFlagsToBeExcludedEndcap= StringToEnumValue<EcalRecHit::Flags>(recHitFlagsToBeExcludedEndcapStr);
//     std::vector<int>& recHitFlagsToBeExcluded = detector==EcalBarrel ? recHitFlagsToBeExcludedBarrel : recHitFlagsToBeExcludedEndcap;
     
    // std::vector<float> covariances = noZS::EcalClusterTools::covariances(seedCluster,recHits,topology.product(),geom.product(),recHitFlagsToBeExcluded,recHitSeverityToBeExcluded,severityLevelAlgo.product()) ;
    //   sigmaEtaEta = sqrt(covariances[0]) ;
    // std::vector<float> localCovariances = noZS::EcalClusterTools::localCovariances(seedCluster,recHits,topology.product(),recHitFlagsToBeExcluded,recHitSeverityToBeExcluded,severityLevelAlgo.product()) ;
      std::vector<float> covariances = noZS::EcalClusterTools::covariances(seedCluster,recHits,topology.product(),geom.product()) ;
    sigmaEtaEta = sqrt(covariances[0]) ;
    std::vector<float> localCovariances = noZS::EcalClusterTools::localCovariances(seedCluster,recHits,topology.product()) ;
  
    sigmaIEtaIEta = sqrt(localCovariances[0]) ;
    //  e1x5 = noZS::EcalClusterTools::e1x5(seedCluster,recHits,topology.product(),recHitFlagsToBeExcluded,recHitSeverityToBeExcluded,severityLevelAlgo.product());
    //    e2x5Max = noZS::EcalClusterTools::e2x5Max(seedCluster,recHits,topology.product(),recHitFlagsToBeExcluded,recHitSeverityToBeExcluded,severityLevelAlgo.product());
    //  e5x5 = noZS::EcalClusterTools::e5x5(seedCluster,recHits,topology.product(),recHitFlagsToBeExcluded,recHitSeverityToBeExcluded,severityLevelAlgo.product());
    e1x5 = noZS::EcalClusterTools::e1x5(seedCluster,recHits,topology.product());
    e2x5Max = noZS::EcalClusterTools::e2x5Max(seedCluster,recHits,topology.product());
    e5x5 = noZS::EcalClusterTools::e5x5(seedCluster,recHits,topology.product());
  }

  ele.setShowerShape(sigmaEtaEta,sigmaIEtaIEta,e1x5,e2x5Max,e5x5);
  
}


void SHEventHelper::fillPFClustersECAL_(const SHEvent* event,double maxDR,SHPFClusterContainer& shPFClusters,
		    const std::vector<reco::PFCluster>& pfClusters,
		    const std::vector<reco::SuperCluster>& superClustersEB,const std::vector<reco::SuperCluster>& superClustersEE)const
{
  //  std::cout <<"filling candidates "<<std::endl;

  const double maxDR2 = maxDR*maxDR;
  std::vector<std::pair<float,float> > eleEtaPhi;
  for(int eleNr=0;eleNr<event->nrElectrons();eleNr++){
    const SHElectron* ele = event->getElectron(eleNr);
    if(ele->et()>20){
      eleEtaPhi.push_back(std::make_pair(ele->detEta(),ele->detPhi()));
    }
  }

  for(size_t clusNr=0;clusNr<pfClusters.size();clusNr++){
    const reco::PFCluster& pfCluster = pfClusters[clusNr];
   
    bool accept =false;
    for(size_t eleNr=0;eleNr<eleEtaPhi.size();eleNr++){
      if(MathFuncs::calDeltaR2(eleEtaPhi[eleNr].first,eleEtaPhi[eleNr].second,
			       pfCluster.eta(),pfCluster.phi())<maxDR2){
	accept=true;
	break;
      }
    }//end ele loop

    if(accept){
      const std::vector<reco::SuperCluster>& superClusters = pfCluster.caloID().detector(reco::CaloID::DET_ECAL_BARREL) ? superClustersEB : superClustersEE;
      //std::cout <<" pf clus E "<<pfCluster.energy()<<" eta "<<pfCluster.eta()<<" phi "<<pfCluster.phi()<<" seed "<<pfCluster.seed().rawId()<<" address "<<&pfCluster<<std::endl;
      //int scSeedCrysId=0;
      int scSeedCrysId=getSCSeedCrysId_(pfCluster.seed().rawId(),superClusters);
      shPFClusters.addECALCluster(SHPFCluster(pfCluster,scSeedCrysId));
    } 
  }
}

void SHEventHelper::fillPFClustersHCAL_(const SHEvent* event,double maxDR,SHPFClusterContainer& shPFClusters,const std::vector<reco::PFCluster>& pfClusters)const
{
  //  std::cout <<"filling candidates "<<std::endl;

  const double maxDR2 = maxDR*maxDR;
  std::vector<std::pair<float,float> > eleEtaPhi;
  for(int eleNr=0;eleNr<event->nrElectrons();eleNr++){
    const SHElectron* ele = event->getElectron(eleNr);
    if(ele->et()>20){
      eleEtaPhi.push_back(std::make_pair(ele->detEta(),ele->detPhi()));
    }
  }

  for(size_t clusNr=0;clusNr<pfClusters.size();clusNr++){
    const reco::PFCluster& pfCluster = pfClusters[clusNr];

    bool accept =false;
    for(size_t eleNr=0;eleNr<eleEtaPhi.size();eleNr++){
      if(MathFuncs::calDeltaR2(eleEtaPhi[eleNr].first,eleEtaPhi[eleNr].second,
			       pfCluster.eta(),pfCluster.phi())<maxDR2){
	accept=true;
	break;
      }
    }//end ele loop

    if(accept){
      shPFClusters.addHCALCluster(SHPFCluster(pfCluster,0));
    } 
  }
}

int SHEventHelper::getSCSeedCrysId_(uint pfSeedId,const std::vector<reco::SuperCluster>& superClusters)const
{
  // std::cout <<"getting sc seed id "<<std::endl;
  for(size_t scNr=0;scNr<superClusters.size();scNr++){
    // std::cout <<"super clust "<<scNr<<" / "<<superClusters.size()<<std::endl;
    const reco::SuperCluster& sc = superClusters[scNr];
    for(reco::CaloCluster_iterator clusIt  = sc.clustersBegin();clusIt!=sc.clustersEnd();++clusIt){
      //  std::cout <<"clus seed E "<<(*clusIt)->energy()<<" eta "<<(*clusIt)->eta()<<" phi "<<(*clusIt)->phi()<<" "<<(*clusIt)->seed().rawId()<<" pf seed "<<pfSeedId<<" address "<<(&**clusIt)<<std::endl;
      if((*clusIt)->seed().rawId()==pfSeedId) return sc.seed()->seed().rawId();
    }
  }
  return 0;
}
