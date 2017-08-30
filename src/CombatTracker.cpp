#include "CombatTracker.h"

using namespace BWAPI;

bool isUnitAttacking(BWAPI::Unit unit)
{
	return unit->getOrder() == Orders::AttackUnit || 
		unit->getOrder() == Orders::Repair ||
		unit->getOrder() == Orders::MedicHeal ||
		unit->isAttacking();
}

bool isAggressiveUnit(BWAPI::Unit unit)
{
	return isUnitAttacking(unit)
		|| unit->getType().isSpellcaster()
		|| unit->getType() == UnitTypes::Terran_Missile_Turret
		|| unit->getType() == UnitTypes::Terran_Vulture_Spider_Mine
		|| unit->getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode
		|| unit->getType() == UnitTypes::Protoss_Carrier
		|| unit->getType() == UnitTypes::Protoss_Reaver
		|| unit->getType() == UnitTypes::Protoss_Photon_Cannon;
		// add units on guard state?
}

// BWAPI Unit->isUnderAttack cannot handle instakills 
// returns true if a unit is in the weapon range of an enemy
bool isExposed(BWAPI::Unit unit)
{
	Unitset unitsNear = unit->getUnitsInRadius(ATTACK_RANGE);
	for (auto unitNear : unitsNear) {
		if (unitNear->getPlayer() != unit->getPlayer() && isAggressiveUnit(unitNear)) {
			return true;
		}
	}
	return false;
}

bool isUnderAttack(BWAPI::Unit unit)
{
	if (unit->isUnderAttack()) return true;
	Unitset unitsNear = unit->getUnitsInRadius(ATTACK_RANGE);
	for (auto unitNear : unitsNear) {
		if (unitNear->getPlayer() != unit->getPlayer() && 
			(unitNear->getOrderTarget() == unit || unitNear->getTarget() == unit 
			|| (unit->getType() == UnitTypes::Zerg_Scourge && isMilitaryUnit(unitNear)) // Scourges are really weak and easy to kill in one shot, so they are always "in danger"
			|| unitNear->getType().isSpellcaster()
			|| (!unit->isFlying() && unitNear->getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode)
			|| unitNear->getType() == UnitTypes::Protoss_Carrier
			|| (!unit->isFlying() && unitNear->getType() == UnitTypes::Protoss_Reaver)
			|| unitNear->getType() == UnitTypes::Protoss_Photon_Cannon)) {
			return true;
		}
	}
	return false;
}

void pauseGameAtPosition(BWAPI::Position position)
{
	Broodwar->setLocalSpeed(500);
	Broodwar->setScreenPosition(position - Position(320, 240));
}

CombatTracker::CombatTracker()
{
	std::string combatsfilepath = Broodwar->mapPathName() + ".rcd";
	replayCombatData.open(combatsfilepath.c_str());

	// save replay/map analyzed
	replayCombatData << Broodwar->mapPathName() << "," << Broodwar->mapHash().c_str() << '\n';
}

CombatTracker::~CombatTracker()
{
	// close all opened combats
	for (auto combat : combats) {
		endCombat(combat, "GAME_END");
	}
	replayCombatData.close();
}

void CombatTracker::onFrame()
{
	// check if units not in combat start a combat
	for (auto unit : Broodwar->getAllUnits()) {
		// if a military unit is attacking or under attack and not already in a combat, we need to add it
		if (isMilitaryUnit(unit) && (isAggressiveUnit(unit) || isExposed(unit))
			&& unitsInCombat.find(unit) == unitsInCombat.end()) {
			Unitset unitsNear = unit->getUnitsInRadius(ATTACK_RANGE);
// 			Broodwar->drawCircleMap(unit->getPosition(), 5, Colors::Yellow, true);
// 			Broodwar->drawCircleMap(unit->getPosition(), ATTACK_RANGE, Colors::Yellow);
			bool isReinforcement = false;
			for (auto unitNear : unitsNear) {
				if (!isMilitaryUnit(unitNear)) continue; // no relevant near unit
				Broodwar->drawLineMap(unit->getPosition(), unitNear->getPosition(), Colors::Yellow);
				// if a near unit is already in combat, we consider it a reinforcement
				if (unitsInCombat.find(unitNear) != unitsInCombat.end()) {
					isReinforcement = true;
					// only add to combat if is under attack or attacking
					if (isAggressiveUnit(unit) || isUnderAttack(unit)) {
						Combat* combatInProgress = getCombat(unitNear);
						// if reinforcement is near start combat, add to combat
						if (Broodwar->getFrameCount() - combatInProgress->firstFrame <= FRAMES_UNTIL_REINFORCEMENT) {
							addToCombat(unit, combatInProgress);
						} else { // otherwise end combat (REINFORCEMENTS) and start a new one next frame
// 							pauseGameAtPosition(unit->getPosition());
// 							Broodwar << "Reinforcement: " << unit->getType() << std::endl;
							endCombat(combatInProgress, "REINFORCEMENT " + std::to_string(unit->getID()));
						}
					}
					break;
				}
			}

			// it isn't a reinforcement therefore look to create a new combat
			if (!isReinforcement) {
				// if we don't have a military enemy unit near, there is no new combat
				bool militaryEnemyNear = false;
				for (auto unitNear : unitsNear) {
					if (unitNear->getPlayer() != unit->getPlayer() && isMilitaryUnit(unitNear)) {
						militaryEnemyNear = true;
						break;
					}
				}
				if (militaryEnemyNear) startCombat(unit);
			}
		}
	}

	// iterate over all combats
	for (auto combat : combats) {
		// Update if unit participated in combat
		for (auto& playerUnits : combat->battleUnits) {
			for (auto& unitInfo : playerUnits.second) {
				if (!combat->unitParticipatedInCombat[unitInfo]) {
					// check if participated
					if (unitInfo->unit->getGroundWeaponCooldown() > 0 ||
						unitInfo->unit->getAirWeaponCooldown() > 0 ||
						unitInfo->unit->getSpellCooldown() > 0||
						unitInfo->unit->getOrder() == Orders::Repair ||
						unitInfo->unit->getOrder() == Orders::MedicHeal) {
						combat->unitParticipatedInCombat[unitInfo] = true;
					}
				}
			}
		}

		// Check combat end condition
		// TODO if units in different regions from starting combat, finish combat (FLEE)
		// if one army destroyed (ARMY_DESTROYED)
		if (combat->isArmyDestroyed()) {
			endCombat(combat, "ARMY_DESTROYED");
		} else {
			// if no attack in SECONDS_SINCE_LAST_ATTACK_2, finish combat (PEACE)
			if (combat->isAnyUnitAttacking()) {
				combat->lastFrameAttacking = Broodwar->getFrameCount();
			} else if (Broodwar->getFrameCount() - combat->lastFrameAttacking >= SECONDS_SINCE_LAST_ATTACK_2) {
				endCombat(combat, "PEACE");
// 				Broodwar->setLocalSpeed(100);
			}
		}
	}

	// Print debug data
// 	Broodwar->drawTextScreen(5, 16, "Units in combat: %d", unitsInCombat.size());
	for (auto unit : unitsInCombat) {
		BWAPI::Color color = Colors::Green;
		if (isAggressiveUnit(unit)) color = Colors::Red;
		else if (isExposed(unit)) color = Colors::Orange;
		Broodwar->drawCircleMap(unit->getPosition(), 5, color, true);
		Broodwar->drawTextMap(unit->getPosition(), "%s", unit->getOrder().c_str());
		Broodwar->drawTextMap(unit->getPosition(), "%s", unit->getOrder().c_str());
		if (unit->getAirWeaponCooldown() > 0 || unit->getGroundWeaponCooldown() > 0) {
			Broodwar->drawTextMap(unit->getPosition() + Position(0, 15), "Cooldown");
		}
	}
}

void CombatTracker::startCombat(BWAPI::Unit newUnit)
{
	// get all combat units near newUnit
	Unitset unitsNear = newUnit->getUnitsInRadius(ATTACK_RANGE);
	Unitset allUnitsNear;
	allUnitsNear.insert(newUnit);
	for (auto unitNear : unitsNear) {
		if (!isMilitaryUnit(unitNear)) continue;
		allUnitsNear.insert(unitNear);
		Unitset unitsNear2 = unitNear->getUnitsInRadius(ATTACK_RANGE);
		// add to the list if it isn't already in
		for (auto newNearUnit : unitsNear2) {
			if (!isMilitaryUnit(newNearUnit)) continue;
			if (allUnitsNear.find(newNearUnit) == allUnitsNear.end()) {
				allUnitsNear.insert(newNearUnit);
			}
		}
	}
	// mark all the units as "in combat"
	unitsInCombat.insert(allUnitsNear.begin(), allUnitsNear.end());
	Combat* newCombat = new Combat(allUnitsNear);
	combats.insert(newCombat);

// 	pauseGameAtPosition(newUnit->getPosition());
// 	Broodwar << "New combat" << std::endl;
}

void CombatTracker::addToCombat(BWAPI::Unit newUnit, Combat* combatToAdd)
{
	combatToAdd->addUnit(newUnit);
	unitsInCombat.insert(newUnit);
}

// search in what combat the unit belongs
Combat* CombatTracker::getCombat(BWAPI::Unit unitInCombat)
{
	Combat* combatFind = nullptr;
	for (auto combat : combats) {
		if (combat->isUnitInCombat(unitInCombat)) {
			combatFind = combat;
			break;
		}
	}
	return combatFind;
}

void CombatTracker::onUnitDestroy(BWAPI::Unit unit)
{
// 	if (!unit->getLastAttackingPlayer()) return;
	if (!unit->isCompleted()) return;		// Some units are destroyed because player canceled training
	if (unit->getType().isSpell()) return;	// Spells are "auto-self" destroyed
	if (unit->getType() == UnitTypes::Protoss_Interceptor) return; // Interceptors are like Carriers "bullets"
	if (unit->isLoaded()) return; // omit units inside transporters

	if (isMilitaryUnit(unit)) {
		Combat* combat = getCombat(unit);
		if (combat != nullptr) {
			combat->unitsKilled.push_back(KilledInfo(unit, Broodwar->getFrameCount(), unit->isLoaded()));
		} else {
			// sometimes players destroy their own mines
			//if (unit->getType() == UnitTypes::Terran_Vulture_Spider_Mine) return;
			if (unit->getOrder() == Orders::ArchonWarp) return; // warping not in combat (TODO does it make sense during combat?)
			if (unit->getOrder() == Orders::DarkArchonMeld) return; // like before, Templars morphing here
			if (unit->getOrder() == Orders::IncompleteBuilding) return; // drone morphing into a building
			if (unit->getType() == UnitTypes::Zerg_Scourge) return; // Scourge are like kamikaze bombs, so they can die attacking a non military unit 
// 			if (unit->getType().isWorker()) return;				// ignore workers

			bool isError = true;
			std::ostringstream buffer;
			buffer << "[ERROR] Destroyed military " << unit->getType().getName() << " Order: " << unit->getOrder().c_str() << " PlayerID: " << unit->getPlayer()->getID();
			
			Unitset unitsNear = Broodwar->getUnitsInRadius(unit->getPosition(), ATTACK_RANGE);
			for (auto unitNear : unitsNear) {
				if (unitNear->getOrderTarget() == unit || unitNear->getTarget() == unit) {
					if (unitNear->getPlayer() != unit->getPlayer()) {
						buffer << "\n  - Killed by " << unitNear->getType();
					} else { //killed by friend
						isError = false;
					}
				} else { // print all near enemy units
// 					buffer << "\n  - Unit " << unitNear->getType();
// 					if (unitNear->getOrderTarget()) buffer << " oderTarget: " << unitNear->getOrderTarget()->getType();
// 					if (unitNear->getTarget()) buffer << " target: " << unitNear->getTarget()->getType();
// 					if (unitNear->getLastCommand().getTarget()) buffer << " lastCommandTarget: " << unitNear->getLastCommand().getTarget()->getType();
				}
			}
			if (isError) {
// 				pauseGameAtPosition(unit->getPosition());
				Broodwar << buffer.str(); Broodwar.flush();
				DEBUG(buffer.str());
			}
		}
	} else {
		if (unit->getType().isResourceContainer()) return;	// ignore mineral fields
		if (unit->getPlayer()->getID() < 0) return;			// ignore neutral players
// 		if (unit->getType().isWorker()) return;				// ignore workers
		if (unit->getType().isBuilding()) return;			// ignore buildings
		if (unit->getType().spaceProvided() > 0) return;	// ignore transporters
		if (unit->getType() == UnitTypes::Protoss_Scarab) return; // ignore Carrier "bullets"
		if (unit->getType() == UnitTypes::Zerg_Larva) return;
		if (unit->getType() == UnitTypes::Protoss_Observer) return; // observers are "harmless" units

// 		pauseGameAtPosition(unit->getPosition());
		Broodwar << "[NOTICE] Destroyed not military " << unit->getType().getName() << " Order: " << unit->getOrder().c_str() << " PlayerID: " << unit->getPlayer()->getID() << std::endl;
		LOG("[NOTICE] Destroyed not military " << unit->getType().getName() << " Order: " << unit->getOrder() << " PlayerID: " << unit->getPlayer()->getID());
	}
}

void CombatTracker::endCombat(Combat* combatToEnd, std::string condition)
{
	if (combatToEnd != nullptr) {
		// remove all units from unitsInCombat
		for (auto playerUnits : combatToEnd->battleUnits) {
			for (auto combatUnit : playerUnits.second) {
				unitsInCombat.erase(combatUnit->unit);
			}
		}

		// print combat
		// General info [frame_start, frame_end, end_condition]
		replayCombatData << "NEW_COMBAT," << combatToEnd->firstFrame << "," << Broodwar->getFrameCount() << "," << condition << '\n';
		// upgrades
		std::string upgradesReserached;
		for (auto playerUnits : combatToEnd->battleUnits) {
			upgradesReserached.clear();
			for (auto upgradeType : BWAPI::UpgradeTypes::allUpgradeTypes()) {
				if (playerUnits.first->getUpgradeLevel(upgradeType) > 0) {
					upgradesReserached += upgradeType.c_str();
					upgradesReserached += ":";
					upgradesReserached += std::to_string(playerUnits.first->getUpgradeLevel(upgradeType));
					upgradesReserached += ",";
				}
			}
			if (!upgradesReserached.empty()) {
				upgradesReserached.pop_back();
				replayCombatData << "ARMY_UPGRADES " << playerUnits.first->getID() << "," << upgradesReserached << '\n';
			}
		}
		// technologies
		std::string techReserached;
		for (auto playerUnits : combatToEnd->battleUnits) {
			techReserached.clear();
			for (auto techType : BWAPI::TechTypes::allTechTypes()) {
				if (techType == BWAPI::TechTypes::Scanner_Sweep
					|| techType == BWAPI::TechTypes::Defensive_Matrix
					|| techType == BWAPI::TechTypes::Infestation
					|| techType == BWAPI::TechTypes::Dark_Swarm
					|| techType == BWAPI::TechTypes::Parasite
					|| techType == BWAPI::TechTypes::Archon_Warp
					|| techType == BWAPI::TechTypes::Dark_Archon_Meld
					|| techType == BWAPI::TechTypes::Feedback
					|| techType == BWAPI::TechTypes::Healing
					|| techType == BWAPI::TechTypes::Nuclear_Strike)
					continue;
				if (playerUnits.first->hasResearched(techType)) {
					techReserached += techType.c_str();
					techReserached += ",";
				}
			}
			if (!techReserached.empty()) {
				techReserached.pop_back();
				replayCombatData << "ARMY_TECHS " << playerUnits.first->getID() << "," << techReserached << '\n';
			}
		}
		// Army X start [unitID, unitType, position, HP, shield, energy]
		for (auto playerUnits : combatToEnd->battleUnits) {
			replayCombatData << "ARMY_START " << playerUnits.first->getID() << '\n';
			for (auto combatUnit : playerUnits.second) {
				// Don't track UnitTypes::Protoss_Interceptor
				if (combatUnit->unitType == UnitTypes::Protoss_Interceptor) continue;
				replayCombatData << combatUnit->unitID << "," << combatUnit->unitType.c_str() << ","
					<< combatUnit->initialTilePosition.x << "," << combatUnit->initialTilePosition.y << ","
					<< combatUnit->initialHP << "," << combatUnit->initialShields << ","
					<< combatUnit->initialEnergy << '\n';
			}
		}
		// Army X end [unitID, unitType, position, HP, shield, energy]
		for (auto playerUnits : combatToEnd->battleUnits) {
			replayCombatData << "ARMY_END " << playerUnits.first->getID() << '\n';
			for (auto combatUnit : playerUnits.second) {
				// Don't track UnitTypes::Protoss_Interceptor
				if (combatUnit->unitType == UnitTypes::Protoss_Interceptor) continue;
				if (combatUnit->unit->exists()) {
					replayCombatData << combatUnit->unit->getID() << "," << combatUnit->unit->getType().c_str() << ","
						<< combatUnit->unit->getTilePosition().x << "," << combatUnit->unit->getTilePosition().y << ","
						<< combatUnit->unit->getHitPoints() << "," << combatUnit->unit->getShields() << ","
						<< combatUnit->unit->getEnergy() << '\n';
				}
			}
		}
		// kills in action
		if (!combatToEnd->unitsKilled.empty()) {
			replayCombatData << "KILLS" << '\n';
			for (auto unitKilled : combatToEnd->unitsKilled) {
				replayCombatData << unitKilled.unit->getID() << "," << unitKilled.frameKilled;
				if (unitKilled.isLoaded) replayCombatData << ",LOADED";
				replayCombatData << '\n';
			}
		}

		// units that NOT participated
		replayCombatData << "UNITS_NOT_PARTICIPATED" << '\n';
		std::ostringstream buffer;
		for (auto& playerUnits : combatToEnd->battleUnits) {
			for (auto& unitInfo : playerUnits.second) {
				if (!combatToEnd->unitParticipatedInCombat[unitInfo]) {
					buffer << unitInfo->unitID << ",";
				}
			}
		}
		std::string bufferString(buffer.str());
		if (!bufferString.empty()) {
			bufferString.pop_back(); // erase last ","
			replayCombatData << bufferString << '\n';
		}

		replayCombatData.flush();
		// delete combat
		combats.erase(combatToEnd);
		delete combatToEnd;
	} else {
		// TODO print error
	}
}

Combat::Combat(BWAPI::Unitset unitsInCombat)
{
	for (auto unit : unitsInCombat) {
		UnitInfo* unitInfo = new UnitInfo(unit);
		battleUnits[unit->getPlayer()].insert(unitInfo);
	}
	firstFrame = Broodwar->getFrameCount();
	lastFrameAttacking = Broodwar->getFrameCount();
}

Combat::~Combat()
{
	// TODO memory leak, delete all UnitInfo
// 	for (auto battleUnitsPlayer : battleUnits) {
// 		for (auto unitInfo : battleUnitsPlayer.second) {
// 			delete unitInfo;
// 		}
// 	}
}

void Combat::addUnit(BWAPI::Unit newUnit)
{
	UnitInfo* unitInfo = new UnitInfo(newUnit);
	battleUnits[newUnit->getPlayer()].insert(unitInfo);
}

bool Combat::isUnitInCombat(BWAPI::Unit unit)
{
	for (auto playerUnits : battleUnits) {
		for (auto combatUnit : playerUnits.second) {
			if (combatUnit->unit == unit) return true;
		}
	}
	return false;
}

bool Combat::isArmyDestroyed()
{
	for (auto playerUnits : battleUnits) {
		bool allUnitsDestroyed = true;
		for (auto combatUnit : playerUnits.second) {
			if (combatUnit->unit->exists()) allUnitsDestroyed = false;
		}
		if (allUnitsDestroyed) return true;
	}
	return false;
}

bool Combat::isAnyUnitAttacking() {
	for (auto playerUnits : battleUnits) {
		for (auto combatUnit : playerUnits.second) {
			if (isUnitAttacking(combatUnit->unit)) return true;
		}
	}
	return false;
}