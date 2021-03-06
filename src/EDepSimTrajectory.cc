#include <string>
#include <sstream>
#include <cmath>

#include <G4VProcess.hh>
#include <G4ParticleTable.hh>
#include <G4AttDefStore.hh>
#include <G4AttDef.hh>
#include <G4AttValue.hh>
#include <G4UnitsTable.hh>

#include "EDepSimTrajectory.hh"
#include "EDepSimTrajectoryPoint.hh"
#include "EDepSimTrajectoryMap.hh"

G4Allocator<EDepSim::Trajectory> aTrajAllocator;

EDepSim::Trajectory::Trajectory()
    : fPositionRecord(0), fTrackID(0), fParentID(0),
      fPDGEncoding(0), fPDGCharge(0.0), fParticleName(""),
      fProcessName(""), fInitialMomentum(G4ThreeVector()),
      fSDEnergyDeposit(0), fSDTotalEnergyDeposit(0), fSDLength(0), 
      fSaveTrajectory(false) {;}

EDepSim::Trajectory::Trajectory(const G4Track* aTrack) {
    fPositionRecord = new TrajectoryPointContainer();
    // Following is for the first trajectory point
    fPositionRecord->push_back(new EDepSim::TrajectoryPoint(aTrack));
    fTrackID = aTrack->GetTrackID();
    fParentID = aTrack->GetParentID();
    G4ParticleDefinition * fpParticleDefinition = aTrack->GetDefinition();
    fPDGEncoding = fpParticleDefinition->GetPDGEncoding();
    fPDGCharge = fpParticleDefinition->GetPDGCharge();
    fParticleName = fpParticleDefinition->GetParticleName();
    const G4VProcess* proc = aTrack->GetCreatorProcess();
    if (proc) fProcessName = proc->GetProcessName();
    else fProcessName = "primary";
    fInitialMomentum = aTrack->GetMomentum();
    fSDEnergyDeposit = 0.0;
    fSDTotalEnergyDeposit = 0.0;
    fSDLength = 0.0;
    fSaveTrajectory = false;
}

EDepSim::Trajectory::Trajectory(EDepSim::Trajectory & right) : G4VTrajectory() {
    fTrackID = right.fTrackID;
    fParentID = right.fParentID;
    fPDGEncoding = right.fPDGEncoding;
    fPDGCharge = right.fPDGCharge;
    fParticleName = right.fParticleName;
    fProcessName = right.fProcessName;
    fInitialMomentum = right.fInitialMomentum;
    fSDEnergyDeposit = right.fSDEnergyDeposit;
    fSDTotalEnergyDeposit = right.fSDTotalEnergyDeposit;
    fSDLength = right.fSDLength;
    fSaveTrajectory = right.fSaveTrajectory;

    fPositionRecord = new TrajectoryPointContainer();
    for(size_t i=0;i<right.fPositionRecord->size();++i) {
        EDepSim::TrajectoryPoint* rightPoint 
            = (EDepSim::TrajectoryPoint*)((*(right.fPositionRecord))[i]);
        fPositionRecord->push_back(new EDepSim::TrajectoryPoint(*rightPoint));
    }
}

EDepSim::Trajectory::~Trajectory() {
    for(size_t i=0;i<fPositionRecord->size();++i){
        delete  (*fPositionRecord)[i];
    }
    fPositionRecord->clear();
    
    delete fPositionRecord;
}

G4double EDepSim::Trajectory::GetInitialKineticEnergy() const {
    const G4ParticleDefinition* p = GetParticleDefinition();
    double mom = GetInitialMomentum().mag();
    if (!p) return mom;
    double mass = p->GetPDGMass();
    double kin = std::sqrt(mom*mom + mass*mass) - mass;
    if (kin<0.0) return 0.0;
    return kin;
}

G4double EDepSim::Trajectory::GetRange() const {
    if (GetPointEntries()<2) return 0.0;
    G4ThreeVector first 
        = dynamic_cast<EDepSim::TrajectoryPoint*>(GetPoint(0))->GetPosition();
    G4ThreeVector last 
        = dynamic_cast<EDepSim::TrajectoryPoint*>(GetPoint(GetPointEntries()-1))
            ->GetPosition();
    return (last - first).mag();
}

void EDepSim::Trajectory::MarkTrajectory(bool save) {
    fSaveTrajectory = true;
    if (!save) return;
    // Mark all parents to be saved as well.
    G4VTrajectory* g4Traj = EDepSim::TrajectoryMap::Get(fParentID);
    if (!g4Traj) return;
    EDepSim::Trajectory* traj = dynamic_cast<EDepSim::Trajectory*>(g4Traj);
    if (!traj) return;
    // Protect against infinite loops.
    if (this == traj) return;
    traj->MarkTrajectory(save);
}

const std::map<G4String,G4AttDef>* EDepSim::Trajectory::GetAttDefs() const {
    G4bool isNew;
    std::map<G4String,G4AttDef>* store
        = G4AttDefStore::GetInstance("EDepSim::Trajectory",isNew);
    
    if (isNew) {
        G4String ID("ID");
        (*store)[ID] = G4AttDef(ID,
                                "Track ID",
                                "Bookkeeping","","G4int");
        
        G4String PID("PID");
        (*store)[PID] = G4AttDef(PID,
                                 "Parent ID",
                                 "Bookkeeping","","G4int");
        
        G4String PN("PN");
        (*store)[PN] = G4AttDef(PN,
                                "Particle Name",
                                "Physics","","G4String");
        
        G4String Ch("Ch");
        (*store)[Ch] = G4AttDef(Ch,
                                "Charge",
                                "Physics","","G4double");
        
        G4String PDG("PDG");
        (*store)[PDG] = G4AttDef(PDG,
                                 "PDG Encoding",
                                 "Physics","","G4int");
        
        G4String IMom("IMom");
        (*store)[IMom] = G4AttDef(IMom, 
                                  "Momentum of track at start of trajectory",
                                  "Physics","","G4ThreeVector");
        
        G4String NTP("NTP");
        (*store)[NTP] = G4AttDef(NTP,
                                 "No. of points",
                                 "Physics","","G4int");
        
    }
    return store;
}

std::vector<G4AttValue>* EDepSim::Trajectory::CreateAttValues() const {
    std::string c;
    std::ostringstream s(c);
    
    std::vector<G4AttValue>* values = new std::vector<G4AttValue>;
    
    s.seekp(std::ios::beg);
    s << fTrackID << std::ends;
    values->push_back(G4AttValue("ID",c.c_str(),""));
    
    s.seekp(std::ios::beg);
    s << fParentID << std::ends;
    values->push_back(G4AttValue("PID",c.c_str(),""));
    
    values->push_back(G4AttValue("PN",fParticleName,""));
    
    s.seekp(std::ios::beg);
    s << fPDGCharge << std::ends;
    values->push_back(G4AttValue("Ch",c.c_str(),""));
    
    s.seekp(std::ios::beg);
    s << fPDGEncoding << std::ends;
    values->push_back(G4AttValue("PDG",c.c_str(),""));
    
    s.seekp(std::ios::beg);
    s << G4BestUnit(fInitialMomentum,"Energy") << std::ends;
    values->push_back(G4AttValue("IMom",c.c_str(),""));
    
    s.seekp(std::ios::beg);
    s << GetPointEntries() << std::ends;
    values->push_back(G4AttValue("NTP",c.c_str(),""));
    
    return values;
}

void EDepSim::Trajectory::AppendStep(const G4Step* aStep) {
    EDepSim::TrajectoryPoint* point = new EDepSim::TrajectoryPoint(aStep);
    fPositionRecord->push_back(point);
}
  
G4ParticleDefinition* EDepSim::Trajectory::GetParticleDefinition() const {
    return (G4ParticleTable::GetParticleTable()->FindParticle(fParticleName));
}

void EDepSim::Trajectory::MergeTrajectory(G4VTrajectory* secondTrajectory) {
    if(!secondTrajectory) return;
    EDepSim::Trajectory* second = (EDepSim::Trajectory*)secondTrajectory;
    G4int ent = second->GetPointEntries();
    // initial point of the second trajectory should not be merged
    for(G4int i=1; i<ent; ++i) { 
        fPositionRecord->push_back((*(second->fPositionRecord))[i]);
    }
    delete (*second->fPositionRecord)[0];
    second->fPositionRecord->clear();
}


