#pragma once

#include <fstream>
#include <iomanip>

#include <BWAPI.h>
#include <BWTA.h>

#define SECONDS_SINCE_LAST_ATTACK 13
#define MAX_ATTACK_RADIUS 21.0*TILE_SIZE
#define MIN_ATTACK_RADIUS 7.0*TILE_SIZE
#define ARMY_TACTICAL_IMPORTANCE 1.0
#define OFFENDER_WIN_COEFFICIENT 2.0

#define RESOURCES_REFRESH 25
#define LOCATION_REFRESH 100

//#define __DEBUG_OUTPUT__
//#define __DEBUG_CDR__
//#define __DEBUG_CDR_FULL__

// FILE LOG
// ==========================================
extern std::ofstream fileLog;
#define DEBUG(Message) fileLog << __FILE__ ":" << __LINE__ << ": " << Message << std::endl
#define LOG(Message) fileLog << Message << std::endl

// A "promise" of classes that we will have
// ==========================================
class TerrainAnalyzer;
class CombatTracker;

// A "promise" of global variables
// ==========================================
extern bool CREATE_RGD;
extern bool CREATE_RLD;
extern bool CREATE_ROD;
extern bool CREATE_RCD;
extern bool CREATE_ASD;

extern int REPLAY_TIME_LIMIT;

extern int SECONDS_SINCE_LAST_ATTACK_2;
extern int ATTACK_RANGE;
extern int FRAMES_UNTIL_REINFORCEMENT;

extern TerrainAnalyzer* terrain;
extern CombatTracker* combatTracker;
extern BWAPI::Playerset activePlayers; // real Players (removing neutrals and observers) 
									   // to be used instead of Broodwar->getPlayers()
extern bool unitDestroyedThisTurn;


bool isInofensiveUnit(BWAPI::Unit u);
bool isMilitaryUnit(BWAPI::Unit unit);
std::map<BWAPI::Player, BWAPI::Unitset> getPlayerMilitaryUnits(const BWAPI::Unitset& unitsAround);
bool isGatheringResources(BWAPI::Unit u);