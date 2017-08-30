#pragma once

#include "Utils.h"
#include "TerrainAnalyzer.h"

enum AttackType {
	DROP,
	GROUND,
	AIR,
	INVIS
};

struct Attack
{
	std::set<AttackType> types;
	int frame;
	int firstFrame;
	BWAPI::Position position;
	BWAPI::Position initPosition;
	double radius;
	// maximum number of units of each type "engaged" in the attack
	std::map<BWAPI::Player, std::map<BWAPI::UnitType, int> > unitTypes;
	std::map<BWAPI::Player, std::set<BWAPI::Unit> > battleUnits;
	std::map<BWAPI::Player, std::set<BWAPI::Unit> > workers;
	BWAPI::Player defender;
	double scoreGroundCDR;
	double scoreGroundRegion;
	double scoreAirCDR;
	double scoreAirRegion;
	double scoreDetectCDR;
	double scoreDetectRegion;
	double economicImportanceCDR;
	double economicImportanceRegion;
	double tacticalImportanceCDR;
	double tacticalImportanceRegion;
	
	Attack(const std::set<AttackType>& at, int f, BWAPI::Position p, double r, BWAPI::Player d,
		const std::map<BWAPI::Player, BWAPI::Unitset>& units);
	void addUnit(BWAPI::Unit u);
	void computeScores();
};

struct HeuristicsAnalyzer
{
	BWAPI::Player p;
	std::map<BWTA::Region*, std::set<BWAPI::Unit> > unitsByRegion;
	std::map<ChokeDepReg, std::set<BWAPI::Unit> > unitsByCDR;
	std::map<BWTA::Region*, double> ecoRegion;
	std::map<ChokeDepReg, double> ecoCDR;
	std::map<BWTA::Region*, double> tacRegion;
	std::map<ChokeDepReg, double> tacCDR;
	std::set<ChokeDepReg> cdrSet;
	std::set<BWAPI::Unit> emptyUnitsSet;

	HeuristicsAnalyzer::HeuristicsAnalyzer(BWAPI::Player pl);
	const std::set<BWAPI::Unit>& getUnitsCDRegion(ChokeDepReg cdr);
	const std::set<BWAPI::Unit>& getUnitsRegion(BWTA::Region* r);
	// ground forces
	double scoreUnitsGround(const std::set<BWAPI::Unit>& eUnits);
	double scoreGround(ChokeDepReg cdr) { return scoreUnitsGround(getUnitsCDRegion(cdr)); }
	double scoreGround(BWTA::Region* r) { return scoreUnitsGround(getUnitsRegion(r)); }
	// air forces
	double scoreUnitsAir(const std::set<BWAPI::Unit>& eUnits);
	double scoreAir(ChokeDepReg cdr) { return scoreUnitsAir(getUnitsCDRegion(cdr)); }
	double scoreAir(BWTA::Region* r) { return scoreUnitsAir(getUnitsRegion(r)); }
	// detection
	int countDetectorUnits(const std::set<BWAPI::Unit>& units);
	double scoreDetect(ChokeDepReg cdr) { return countDetectorUnits(getUnitsCDRegion(cdr)); }
	double scoreDetect(BWTA::Region* r) { return countDetectorUnits(getUnitsRegion(r)); }
	// economy
	int countWorkingPeons(const std::set<BWAPI::Unit>& units);
	double economicImportance(BWTA::Region* r);
	double economicImportance(ChokeDepReg cdr);
	/// tactical importance = normalized relative importance of sum of the square distances
	/// from this region to the baseS of the player + from this region to the mean position of his army
	BWAPI::Unitset getTownhalls(const BWAPI::Unitset& units);
	double tacticalImportance(BWTA::Region* r);
	double tacticalImportance(ChokeDepReg cdr);
};

class GameData
{
public:
	GameData(); // Generates RLD file
	~GameData();
	void onFrame();
	void onReceiveText(BWAPI::Player player, std::string text);
	void onPlayerLeft(BWAPI::Player player);
	void onNukeDetect(BWAPI::Position target);
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitDestroy(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);
	void onUnitRenegade(BWAPI::Unit unit);

private:
	std::ofstream replayDat;
	std::list<Attack> attacks;
	std::map<BWAPI::Player, int> lastDropOrderByPlayer;
	
	std::map<BWAPI::Player, std::set<std::pair<BWAPI::Unit, BWAPI::UnitType> > > unseenUnits;

	std::map<BWAPI::Player, std::list<BWAPI::TechType> > listCurrentlyResearching;
	std::map<BWAPI::Player, std::list<BWAPI::TechType> > listResearched;
	std::map<BWAPI::Player, std::list<BWAPI::UpgradeType> > listCurrentlyUpgrading;
	std::map<BWAPI::Player, std::list<std::pair<BWAPI::UpgradeType, int> > > listUpgraded;


	void onUpdateAttacks();
	void onNewAttack(BWAPI::Unit unitKilled);
	std::map<BWAPI::Player, BWAPI::Unitset> getPlayerMilitaryUnitsNotInAttack(const BWAPI::Unitset& unitsAround);
	void endAttack(std::list<Attack>::iterator it, BWAPI::Player loser, BWAPI::Player winner);

	void handleVisionEvents();
	void handleTechEvents();
	
};