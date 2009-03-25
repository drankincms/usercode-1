#ifndef SHJET
#define SHJET

//This unsurprisingly is my jet class
//Currently as I'm writing this class, I know *nothing* about
//jets at CMS so I'm basically copying whatever looks interesting
//in the CaloJet class....
//By that you should understand this is very basic

#include "TLorentzVector.h"
#include "TObject.h"

namespace pat{
  class Jet;
}

class SHJet : public TObject {

private: 
  TLorentzVector p4_;
  float hadFrac_;
  float caloArea_;
  float maxNrgyInEmTowers_;
  float maxNrgyInHadTowers_;
  float hadNrgyInH0_;
  float hadNrgyInHB_;
  float hadNrgyInHF_;
  float hadNrgyInHE_;
  float emNrgyInEB_;
  float emNrgyInEE_;
  float emNrgyInHF_;
  float jetNrgyScale_; //new for v2
  float trkCountHighEffBTag_;
  float trkCountHighPureBTag_;
  float jetBProbBTag_;
  float jetProbBTag_;
  int partonFlavour_;
public:
  SHJet();
  SHJet(const pat::Jet & caloJet); //CMSSW hook
  SHJet(const SHJet& rhs);
  ~SHJet(){}
  
  SHJet& operator=(const SHJet& rhs);
  bool operator==(const SHJet& rhs)const;
  bool operator!=(const SHJet& rhs)const{return !(*this==rhs);}

  const TLorentzVector& p4()const{return p4_;}
  float et()const{return p4_.Et();}	
  float etUnCorr()const{return et();}
  float eta()const{return p4_.Eta();}
  float nrgy()const{return p4_.E();}
  float hadFrac()const{return hadFrac_;}
  float caloArea()const{return caloArea_;}
  float maxNrgyInEmTowers()const{return maxNrgyInEmTowers_;}
  float maxNrgyInHadTowers()const{return maxNrgyInHadTowers_;}
  float hadNrgyInH0()const{return hadNrgyInH0_;}
  float hadNrgyInHB()const{return hadNrgyInHB_;}
  float hadNrgyInHF()const{return hadNrgyInHF_;}
  float hadNrgyInHE()const{return hadNrgyInHE_;}
  float emNrgyInEB()const{return emNrgyInEB_;}
  float emNrgyInEE()const{return emNrgyInEE_;}
  float emNrgyInHF()const{return emNrgyInHF_;}

  float jetNrgyScale()const{return jetNrgyScale_;}
  void setJetNrgyScale(float scale){jetNrgyScale_=scale;}
  
  
  ClassDef(SHJet,1)

};

#endif
