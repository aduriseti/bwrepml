#include "Utils.h"

// config variables
bool CREATE_RGD = false;
bool CREATE_RLD = false;
bool CREATE_ROD = false;
bool CREATE_RCD = false;
bool CREATE_ASD = true;

int REPLAY_TIME_LIMIT = 60 * 45 * 24;

// Global variables for CombatTracker
int SECONDS_SINCE_LAST_ATTACK_2 = 24 * 6;
int ATTACK_RANGE = 12 * TILE_SIZE; // siege tanks have the biggest range
int FRAMES_UNTIL_REINFORCEMENT = 12;

// global variables
std::ofstream fileLog;
TerrainAnalyzer* terrain;
CombatTracker* combatTracker;
BWAPI::Playerset activePlayers;
bool unitDestroyedThisTurn;

// global functions
bool isInofensiveUnit(BWAPI::Unit u)
{
	return (u->getPlayer()->isNeutral() || u->getPlayer()->isObserver()
		|| u->isGatheringGas() || u->isGatheringMinerals() || u->isRepairing()
		|| (u->getType().isBuilding() && !u->getType().canAttack())
		|| u->getType() == BWAPI::UnitTypes::Zerg_Larva
		|| u->getType() == BWAPI::UnitTypes::Zerg_Broodling
		|| u->getType() == BWAPI::UnitTypes::Zerg_Egg
		|| u->getType() == BWAPI::UnitTypes::Zerg_Cocoon
		|| u->getType() == BWAPI::UnitTypes::Protoss_Scarab
		|| u->getType() == BWAPI::UnitTypes::Terran_Nuclear_Missile
		);
}

bool isMilitaryUnit(BWAPI::Unit unit)
{
	if (unit->getPlayer()->getID() < 0) return false; // omit neutral players
	if (!unit->isCompleted()) return false;
	if (unit->isLoaded()) return false; // omit units inside transporters
	BWAPI::UnitType uType = unit->getType();
	return (uType.canAttack()
		// REMOVE units that can attack
		&& uType != BWAPI::UnitTypes::Protoss_Interceptor	// Interceptors are Carrier's "weapon"
		&& uType != BWAPI::UnitTypes::Protoss_Scarab		// Scarabs are Reaver's "weapon"
		)
		// INCLUDE units that cannot attack
		|| (uType.isSpellcaster() && uType != BWAPI::UnitTypes::Terran_Comsat_Station) // exclude Comsat Station
		|| (uType == BWAPI::UnitTypes::Terran_Bunker && unit->getSpaceRemaining() < uType.spaceProvided())
		|| uType == BWAPI::UnitTypes::Protoss_Carrier
		|| uType == BWAPI::UnitTypes::Protoss_Reaver;
}

std::map<BWAPI::Player, BWAPI::Unitset> getPlayerMilitaryUnits(const BWAPI::Unitset& unitsAround)
{
	std::map<BWAPI::Player, BWAPI::Unitset> playerUnits;
	for (const auto& p : activePlayers) playerUnits.insert(make_pair(p, BWAPI::Unitset()));
	for (const auto& u : unitsAround) {
		if (isInofensiveUnit(u)) continue;
		playerUnits[u->getPlayer()].insert(u);
	}
	return playerUnits;
}

bool isGatheringResources(BWAPI::Unit u)
{
	return u->isGatheringMinerals() || u->isGatheringGas();
}