#pragma once

#include "Utils.h"

struct UnitInfo
{
	BWAPI::Unit unit;
	int unitID;
	BWAPI::UnitType unitType;
	BWAPI::TilePosition initialTilePosition;
	int initialHP;
	int initialShields;
	int initialEnergy;

	UnitInfo(BWAPI::Unit bwapiUnit)
		:unit(bwapiUnit), unitID(bwapiUnit->getID()), unitType(bwapiUnit->getType()),
		initialTilePosition(bwapiUnit->getTilePosition()),
		initialHP(bwapiUnit->getHitPoints()), initialShields(bwapiUnit->getShields()),
		initialEnergy(bwapiUnit->getEnergy()) {}
};

struct KilledInfo {
	BWAPI::Unit unit;
	int frameKilled;
	bool isLoaded;

	KilledInfo(BWAPI::Unit unit, int frame, bool isLoaded) :unit(unit), frameKilled(frame), isLoaded(isLoaded){}
};

class Combat
{
public:
	std::map< BWAPI::Player, std::set<UnitInfo*> > battleUnits;
	int firstFrame;
	int lastFrameAttacking;
	std::vector<KilledInfo> unitsKilled;
	std::map<UnitInfo*, bool> unitParticipatedInCombat;

	Combat(BWAPI::Unitset unitsInCombat);
	~Combat();
	void addUnit(BWAPI::Unit newUnit);
	bool isUnitInCombat(BWAPI::Unit unit);
	bool isArmyDestroyed();
	bool isAnyUnitAttacking();

private:

};

class CombatTracker
{
public:
	std::set<Combat*> combats;
	std::set<BWAPI::Unit> unitsInCombat;

	CombatTracker();
	~CombatTracker();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void startCombat(BWAPI::Unit newUnit);
	void endCombat(Combat* combatToEnd, std::string condition);

private:
	std::ofstream replayCombatData;

	Combat* getCombat(BWAPI::Unit unitInCombat);
	void addToCombat(BWAPI::Unit newUnit, Combat* combatToAdd);
};


