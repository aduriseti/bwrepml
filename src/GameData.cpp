#include "GameData.h"

using namespace BWAPI;

std::string attackTypeToStr(AttackType at)
{
	switch (at) {
	case DROP:		return "DropAttack";
	case GROUND:	return "GroundAttack";
	case AIR:		return "AirAttack";
	case INVIS:		return "InvisAttack";
	default:		return "UnknownAttackError";
	}
}

// ====================================================================================
// Attack struct
// ====================================================================================

Attack::Attack(const std::set<AttackType>& at, int f, BWAPI::Position p, double r, BWAPI::Player d,
	const std::map<BWAPI::Player, BWAPI::Unitset>& units)
	: types(at), 
	frame(f), 
	firstFrame(BWAPI::Broodwar->getFrameCount()), 
	position(p), 
	initPosition(p), 
	radius(r), 
	defender(d)
{
	for (const auto& pu : units) {
		unitTypes.insert(std::make_pair(pu.first, std::map<BWAPI::UnitType, int>()));
		battleUnits.insert(std::make_pair(pu.first, std::set<BWAPI::Unit>()));
		workers.insert(std::make_pair(pu.first, std::set<BWAPI::Unit>()));
		for (const auto& u : pu.second) {
			addUnit(u);
		}
	}

	computeScores();
}

void Attack::addUnit(BWAPI::Unit u)
{
	if (!battleUnits[u->getPlayer()].count(u)) {
		if (unitTypes[u->getPlayer()].count(u->getType())) {
			unitTypes[u->getPlayer()][u->getType()] += 1;
		} else {
			unitTypes[u->getPlayer()].insert(std::make_pair(u->getType(), 1));
		}
		battleUnits[u->getPlayer()].insert(u);
	}
}

void Attack::computeScores()
{
	if (defender == NULL || defender->isObserver() || defender->isNeutral() || !initPosition.isValid()) {
		scoreGroundCDR = -1.0;
		scoreGroundRegion = -1.0;
		scoreAirCDR = -1.0;
		scoreAirRegion = -1.0;
		scoreDetectCDR = -1.0;
		scoreDetectRegion = -1.0;
		economicImportanceCDR = -1.0;
		economicImportanceRegion = -1.0;
		tacticalImportanceCDR = -1.0;
		tacticalImportanceRegion = -1.0;
		return;
	}

	HeuristicsAnalyzer ha(defender);

	TilePosition tp(initPosition);
	if (!terrain->isWalkable(tp)) tp = terrain->findClosestWalkable(tp);

	BWTA::Region* r = BWTA::getRegion(tp);
	if (r == NULL) r = terrain->findClosestRegion(tp);

	ChokeDepReg cdr = terrain->regionData.chokeDependantRegion[tp.x][tp.y];
	if (cdr == -1) cdr = terrain->findClosestCDR(tp);

	scoreGroundCDR = ha.scoreGround(cdr);
	scoreGroundRegion = ha.scoreGround(r);
	scoreAirCDR = ha.scoreAir(cdr);
	scoreAirRegion = ha.scoreAir(r);
	scoreDetectCDR = ha.scoreDetect(cdr);
	scoreDetectRegion = ha.scoreDetect(r);
	economicImportanceCDR = ha.economicImportance(cdr);
	economicImportanceRegion = ha.economicImportance(r);
	tacticalImportanceCDR = ha.tacticalImportance(cdr);
	tacticalImportanceRegion = ha.tacticalImportance(r);
}

// ====================================================================================
// HeuristicsAnalyzer struct
// ====================================================================================

HeuristicsAnalyzer::HeuristicsAnalyzer(BWAPI::Player pl)
	: p(pl)
{
	for (const auto& u : p->getUnits()) {
		TilePosition tp(u->getTilePosition());
		BWTA::Region* r = BWTA::getRegion(tp);
		if (unitsByRegion.count(r)) {
			unitsByRegion[r].insert(u);
		} else {
			// TODO auto insert []=...
			std::set<Unit> tmpSet;
			tmpSet.insert(u);
			unitsByRegion.insert(make_pair(r, tmpSet));
		}
		ChokeDepReg cdr = terrain->regionData.chokeDependantRegion[tp.x][tp.y];
		cdrSet.insert(cdr);
		if (unitsByCDR.count(cdr)) {
			unitsByCDR[cdr].insert(u);
		} else {
			// TODO auto insert []=...
			std::set<Unit> tmpSet;
			tmpSet.insert(u);
			unitsByCDR.insert(make_pair(cdr, tmpSet));
		}
	}
}

const std::set<Unit>& HeuristicsAnalyzer::getUnitsCDRegion(ChokeDepReg cdr)
{
	if (unitsByCDR.count(cdr)) return unitsByCDR[cdr];
	return emptyUnitsSet;
}

const std::set<Unit>& HeuristicsAnalyzer::getUnitsRegion(BWTA::Region* r)
{
	if (unitsByRegion.count(r)) return unitsByRegion[r];
	return emptyUnitsSet;
}

double scoreUnits(const std::list<Unit>& eUnits)
{
	double minPrice = 0.0;
	double gasPrice = 0.0;
	double supply = 0.0;
	for (const auto& u : eUnits) {
		UnitType ut(u->getType());
		minPrice += ut.mineralPrice();
		gasPrice += ut.gasPrice();
		supply += ut.supplyRequired();
	}
	return minPrice + (4.0 / 3) * gasPrice + 25 * supply;
}

double HeuristicsAnalyzer::scoreUnitsGround(const std::set<Unit>& eUnits)
{
	double minPrice = 0.0;
	double gasPrice = 0.0;
	double supply = 0.0;
	for (const auto& u : eUnits) {
		UnitType ut(u->getType());
		if (ut.groundWeapon() == WeaponTypes::None
			&& ut != UnitTypes::Protoss_High_Templar
			&& ut != UnitTypes::Protoss_Dark_Archon
			&& ut != UnitTypes::Zerg_Defiler
			&& ut != UnitTypes::Zerg_Queen
			&& ut != UnitTypes::Terran_Medic
			&& ut != UnitTypes::Terran_Science_Vessel
			&& ut != UnitTypes::Terran_Bunker)
			continue;
		minPrice += ut.mineralPrice();
		gasPrice += ut.gasPrice();
		supply += ut.supplyRequired();
		if (ut == UnitTypes::Terran_Siege_Tank_Siege_Mode) // a small boost for sieged tanks and lurkers
			supply += ut.supplyRequired();
	}
	return minPrice + (4.0 / 3) * gasPrice + 25 * supply;
}

double HeuristicsAnalyzer::scoreUnitsAir(const std::set<Unit>& eUnits)
{
	double minPrice = 0.0;
	double gasPrice = 0.0;
	double supply = 0.0;
	for (const auto& u : eUnits) {
		UnitType ut(u->getType());
		if (ut.airWeapon() == WeaponTypes::None
			&& ut != UnitTypes::Protoss_High_Templar
			&& ut != UnitTypes::Protoss_Dark_Archon
			&& ut != UnitTypes::Zerg_Defiler
			&& ut != UnitTypes::Zerg_Queen
			&& ut != UnitTypes::Terran_Medic
			&& ut != UnitTypes::Terran_Science_Vessel
			&& ut != UnitTypes::Terran_Bunker)
			continue;
		minPrice += ut.mineralPrice();
		gasPrice += ut.gasPrice();
		supply += ut.supplyRequired();
	}
	return minPrice + (4.0 / 3) * gasPrice + 25 * supply;
}

int HeuristicsAnalyzer::countDetectorUnits(const std::set<Unit>& units)
{
	int count = 0;
	for (const auto& unit : units) {
		if (unit->getType().isDetector()) ++count;
	}
	return count;
}

int HeuristicsAnalyzer::countWorkingPeons(const std::set<Unit>& units)
{
	int count = 0;
	for (const auto& unit : units) {
		if (unit->getType().isWorker() &&
			(unit->isGatheringGas() || unit->isGatheringMinerals() || unit->isConstructing() || unit->isRepairing()))
			++count;
	}
	return count;
}

double HeuristicsAnalyzer::economicImportance(BWTA::Region* r)
{
	if (ecoRegion.empty()) {
		double s = 0.0;
		for (const auto& rr : BWTA::getRegions()) {
			int c = countWorkingPeons(getUnitsRegion(rr));
			ecoRegion.insert(std::make_pair(rr, c));
			s += c;
		}
		for (const auto& rr : BWTA::getRegions()) {
			ecoRegion[rr] = ecoRegion[rr] / s;
		}
	}
	return ecoRegion[r];
}

double HeuristicsAnalyzer::economicImportance(ChokeDepReg cdr)
{
	if (ecoCDR.empty()) {
		double s = 0.0;
		for (const auto& cdrr : cdrSet) {
			int c = countWorkingPeons(getUnitsCDRegion(cdrr));
			ecoCDR.insert(std::make_pair(cdrr, c));
			s += c;
		}
		for (const auto& cdrr : cdrSet) {
			ecoCDR[cdrr] = ecoCDR[cdrr] / s;
		}
	}
	return ecoCDR[cdr];
}

BWAPI::Unitset HeuristicsAnalyzer::getTownhalls(const BWAPI::Unitset& units)
{
	BWAPI::Unitset ret;
	for (const auto& unit : units) {
		if (unit->exists() && unit->getType().isResourceDepot()) {
			ret.insert(unit);
		}
	}
	return ret;
}

double HeuristicsAnalyzer::tacticalImportance(BWTA::Region* r)
{
	if (tacRegion.count(r)) return tacRegion[r];
	BWAPI::Unitset ths = getTownhalls(p->getUnits());
	BWAPI::Unitset army = getPlayerMilitaryUnits(p->getUnits())[p];
	Position mean(0, 0);
	for (const auto& u : army) mean += u->getPosition();
	mean = Position(mean.x / army.size(), mean.y / army.size());
	TilePosition meanWalkable(mean);
	if (!terrain->isWalkable(meanWalkable)) {
		meanWalkable = terrain->findClosestWalkable(meanWalkable);
	}
	BWTA::Region* meanArmyReg = BWTA::getRegion(mean);
	if (meanArmyReg == NULL) {
		meanArmyReg = terrain->findClosestRegion(meanWalkable);
	}
	double s = 0.0;
	for (const auto& rr : BWTA::getRegions()) {
		tacRegion.insert(std::make_pair(rr, 0.0));
		for (const auto& th : ths) {
			BWTA::Region* thr = BWTA::getRegion(th->getTilePosition());
			if (thr != NULL && thr->getReachableRegions().count(rr)) {
				double tmp = terrain->_pfMaps.distRegions[terrain->hashRegionCenter(rr)][terrain->hashRegionCenter(thr)];
				tacRegion[rr] += tmp*tmp;
			} else { // if rr is an island, it will be penalized a lot
				tacRegion[rr] += Broodwar->mapWidth() * Broodwar->mapHeight();
			}
		}
		double tmp = 0.0;
		if (rr->getReachableRegions().count(meanArmyReg)) {
			tmp = terrain->_pfMaps.distRegions[terrain->hashRegionCenter(rr)][terrain->hashRegionCenter(meanArmyReg)];
		} else {
			tmp = terrain->_pfMaps.distRegions[terrain->hashRegionCenter(rr)][terrain->hashRegionCenter(terrain->findClosestReachableRegion(meanArmyReg, rr))];
		}
		tacRegion[rr] += tmp*tmp * ARMY_TACTICAL_IMPORTANCE;
		s += tacRegion[rr];
	}
	for (const auto& it : tacRegion) {
		tacRegion[it.first] = s - it.second;
	}
	return tacRegion[r];
}

double HeuristicsAnalyzer::tacticalImportance(ChokeDepReg cdr)
{
	if (tacCDR.count(cdr)) return tacCDR[cdr];
	BWAPI::Unitset ths = getTownhalls(p->getUnits());
	BWAPI::Unitset army = getPlayerMilitaryUnits(p->getUnits())[p];
	Position mean(0, 0);
	for (const auto& u : army) {
		mean += u->getPosition();
	}
	mean = Position(mean.x / army.size(), mean.y / army.size());
	TilePosition m(mean);
	if (!terrain->isWalkable(m)) {
		m = terrain->findClosestWalkable(m);
	}
	ChokeDepReg meanArmyCDR = terrain->regionData.chokeDependantRegion[m.x][m.y];
	if (meanArmyCDR == -1) {
		meanArmyCDR = terrain->findClosestCDR(m);
	}
	double s = 0.0;
	for (const auto& cdrr : terrain->allChokeDepRegs) {
		tacCDR.insert(std::make_pair(cdrr, 0.0));
		for (const auto& th : ths) {
			ChokeDepReg thcdr = terrain->regionData.chokeDependantRegion[th->getTilePosition().x][th->getTilePosition().y];
			if (thcdr != -1 && terrain->_pfMaps.distCDR[thcdr][cdrr] >= 0.0) { // is reachable
				double tmp = terrain->_pfMaps.distCDR[thcdr][cdrr];
				tacCDR[cdrr] += tmp*tmp;
			} else { // if rr is an island, it will be penalized a lot
				tacCDR[cdrr] += Broodwar->mapWidth() * Broodwar->mapHeight();
			}
		}
		double tmp = 0.0;
		if (terrain->_pfMaps.distCDR[cdrr][meanArmyCDR] >= 0.0) { // is reachable
			tmp = terrain->_pfMaps.distCDR[cdrr][meanArmyCDR];
		} else {
			tmp = terrain->_pfMaps.distCDR[cdrr][terrain->findClosestReachableCDR(meanArmyCDR, cdrr)];
		}
		tacCDR[cdrr] += tmp*tmp * ARMY_TACTICAL_IMPORTANCE;
		s += tacCDR[cdrr];
	}
	for (const auto& it : tacCDR) {
		tacCDR[it.first] = s - it.second;
	}
	return tacCDR[cdr];
}

// ====================================================================================
// GameData class
// ====================================================================================

GameData::GameData()
{
	// Create files to save data
	std::string filepath = Broodwar->mapPathName() + ".rgd";
	replayDat.open(filepath.c_str());

	replayDat << "[Replay Start]\n" << std::fixed << std::setprecision(4);
	replayDat << "RepPath: " << Broodwar->mapPathName() << '\n';
	replayDat << "MapName: " << Broodwar->mapName() << '\n';
	replayDat << "NumStartPositions: " << Broodwar->getStartLocations().size() << '\n';
	replayDat << "The following players are in this replay:\n";

	for (const auto& player : Broodwar->getPlayers()){
		if (!player->getUnits().empty() && !player->isNeutral()) {
			// TODO you cannot trust this startLocID
			int startLocID = -1;
			for (const auto& startLocation : Broodwar->getStartLocations()) {
				startLocID++;
				if (player->getStartLocation() == startLocation) break;
			}
			replayDat << player->getID() << ", " << player->getName() << ", " << player->getRace().getName() << ", " << startLocID << '\n';
			activePlayers.insert(player);
		}
	}

	replayDat << "Begin replay data:\n";
}

GameData::~GameData()
{
	// close all unfinished attacks
	for (std::list<Attack>::iterator it = attacks.begin(); it != attacks.end();) {
		endAttack(it, NULL, NULL);
		attacks.erase(it++);
	}
	replayDat << "[EndGame]\n";
	replayDat.close();
}

void GameData::onUpdateAttacks()
{
	if (attacks.size() > 20) Broodwar->printf("Bug, attacks is bigger than 20");
	for (std::list<Attack>::iterator it = attacks.begin(); it != attacks.end(); ) {
#ifdef __DEBUG_OUTPUT__
		Broodwar->drawCircleMap(it->position.x, it->position.y, static_cast<int>(it->radius), Colors::Red);
		Broodwar->drawBoxMap(it->position.x - 6, it->position.y - 6, it->position.x + 6, it->position.y + 6, Colors::Red, true);
		int i = 0;
		for (const auto& at : it->types) {
			Broodwar->drawTextMap(std::max(0, it->position.x - 2 * TILE_SIZE), std::max(0, it->position.y - TILE_SIZE + (i * 16)),
				"%s on %s (race %s)",
				attackTypeToStr(at).c_str(), it->defender->getName().c_str(), it->defender->getRace().c_str());
			++i;
		}
#endif
		std::map<Player, Unitset> playerUnits = getPlayerMilitaryUnits(
			Broodwar->getUnitsInRadius(it->position, static_cast<int>(it->radius))
			);
		for (const auto& pp : playerUnits) {
			if (!it->unitTypes.count(pp.first))
				it->unitTypes.insert(std::make_pair(pp.first, std::map<BWAPI::UnitType, int>()));
			if (!it->battleUnits.count(pp.first))
				it->battleUnits.insert(std::make_pair(pp.first, std::set<BWAPI::Unit>()));
			for (const auto& uu : pp.second) {
				UnitType ut = uu->getType();
				if ((ut.canAttack() && !ut.isWorker()) // non workers non casters (counts interceptors)
					|| uu->isAttacking() // attacking workers
					|| ut == UnitTypes::Protoss_High_Templar || ut == UnitTypes::Protoss_Dark_Archon || ut == UnitTypes::Protoss_Observer || ut == UnitTypes::Protoss_Shuttle || ut == UnitTypes::Protoss_Carrier
					|| ut == UnitTypes::Zerg_Defiler || ut == UnitTypes::Zerg_Queen || ut == UnitTypes::Zerg_Lurker || ut == UnitTypes::Zerg_Overlord
					|| ut == UnitTypes::Terran_Medic || ut == UnitTypes::Terran_Dropship || ut == UnitTypes::Terran_Science_Vessel)
					it->addUnit(uu);
				if (ut.isWorker()) {
					it->workers[uu->getPlayer()].insert(uu);
				}
			}
		}
		// TODO modify, currently 2 players (1v1) only
		BWAPI::Player winner = NULL;
		BWAPI::Player loser = NULL;
		BWAPI::Player offender = NULL;
		for (const auto& p : it->unitTypes) {
			if (p.first != it->defender) offender = p.first;
		}
		BWAPI::Position pos(0, 0);
		int attackers = 0;
		Unitset tmp = playerUnits[offender];
		tmp.insert(playerUnits[it->defender].begin(), playerUnits[it->defender].end());
		for (const auto& u : tmp) {
			if (u && u->exists() && (u->isAttacking() || u->isUnderAttack())) {
				pos += u->getPosition();
				++attackers;
			}
		}
		if (attackers > 0) {
			it->position = BWAPI::Position(pos.x / attackers, pos.y / attackers);
			for (const auto& u : tmp) {
				double range_and_dist = u->getDistance(it->position) +
					std::max(u->getType().groundWeapon().maxRange(),
					u->getType().airWeapon().maxRange());
				if ((u->isAttacking() || u->isUnderAttack()) && u->getDistance(it->position) > it->radius)
					it->radius = u->getDistance(it->position);
			}
			if (it->radius < MIN_ATTACK_RADIUS)
				it->radius = MIN_ATTACK_RADIUS;
			if (it->radius > MAX_ATTACK_RADIUS)
				it->radius = MAX_ATTACK_RADIUS;
			it->frame = Broodwar->getFrameCount();
			++it;
		} else if (Broodwar->getFrameCount() - it->frame >= 24 * SECONDS_SINCE_LAST_ATTACK) {
			// Attack is finished, who won the battle ? (this is not essential, as we output enough data to recompute it)
			std::map<BWAPI::Player, std::list<BWAPI::Unit> > aliveUnits;
			for (const auto& p : it->battleUnits) {
				aliveUnits.insert(std::make_pair(p.first, std::list<Unit>()));
				for (const auto& u : p.second) {
					if (u && u->exists()) aliveUnits[p.first].push_back(u);
				}
			}
			if (scoreUnits(aliveUnits[it->defender]) * OFFENDER_WIN_COEFFICIENT < scoreUnits(aliveUnits[offender])) {
				winner = offender;
				loser = it->defender;
			} else {
				loser = offender;
				winner = it->defender;
			}
			endAttack(it, loser, winner);
			// if the currently examined attack is too old and too far,
			// remove it (no longer a real attack)
			attacks.erase(it++);
		} else {
			++it;
		}
	}
}

void GameData::endAttack(std::list<Attack>::iterator it, BWAPI::Player loser, BWAPI::Player winner)
{
#ifdef __DEBUG_OUTPUT__
	if (winner != NULL && loser != NULL)
	{
		Broodwar->printf("Player %s (race %s) won the battle against player %s (race %s) at Position (%d,%d)",
			winner->getName().c_str(), winner->getRace().c_str(),
			loser->getName().c_str(), loser->getRace().c_str(),
			it->position.x, it->position.y);
	}
#endif

	std::string tmpAttackType("(");
	if (it->types.empty()) {
		tmpAttackType += ")";
	} else {
		for (const auto& t : it->types) {
			tmpAttackType += attackTypeToStr(t) + ",";
		}
		tmpAttackType[tmpAttackType.size() - 1] = ')';
	}

	std::string tmpUnitTypes("{");
	for (const auto& put : it->unitTypes) {
		std::string tmpUnitTypesPlayer(":{");
		for (const auto& pp : put.second) {
			tmpUnitTypesPlayer += pp.first.getName() + ":" + std::to_string(pp.second) + ",";
		}
		if (tmpUnitTypesPlayer[tmpUnitTypesPlayer.size() - 1] == '{') {
			tmpUnitTypesPlayer += "}";
		} else {
			tmpUnitTypesPlayer[tmpUnitTypesPlayer.size() - 1] = '}';
		}
		tmpUnitTypes += std::to_string(put.first->getID()) + tmpUnitTypesPlayer + ",";
	}
	if (tmpUnitTypes[tmpUnitTypes.size() - 1] == '{') {
		tmpUnitTypes += "}";
	} else {
		tmpUnitTypes[tmpUnitTypes.size() - 1] = '}';
	}
	std::string tmpUnitTypesEnd("{");
	for (const auto& pu : it->battleUnits) {
		std::map<BWAPI::UnitType, int> tmp;
		for (const auto& u : pu.second) {
			if (!u->exists()) continue;
			if (tmp.count(u->getType())) {
				tmp[u->getType()] += 1;
			} else {
				tmp.insert(std::make_pair(u->getType(), 1));
			}
		}
		std::string tmpUnitTypesPlayer(":{");
		for (const auto& pp : tmp) {
			tmpUnitTypesPlayer += pp.first.getName() + ":" + std::to_string(pp.second) + ",";
		}
		if (tmpUnitTypesPlayer[tmpUnitTypesPlayer.size() - 1] == '{') {
			tmpUnitTypesPlayer += "}";
		} else {
			tmpUnitTypesPlayer[tmpUnitTypesPlayer.size() - 1] = '}';
		}
		tmpUnitTypesEnd += std::to_string(pu.first->getID()) + tmpUnitTypesPlayer + ",";
	}
	if (tmpUnitTypesEnd[tmpUnitTypesEnd.size() - 1] == '{') {
		tmpUnitTypesEnd += "}";
	} else {
		tmpUnitTypesEnd[tmpUnitTypesEnd.size() - 1] = '}';
	}
	std::string tmpWorkersDead("{");
	for (const auto& pu : it->workers) {
		int c = 0;
		tmpWorkersDead += std::to_string(pu.first->getID()) + ":";
		for (const auto& u : pu.second) {
			if (u && !u->exists()) ++c;
		}
		tmpWorkersDead += std::to_string(c) + ",";
	}
	tmpWorkersDead[tmpWorkersDead.size() - 1] = '}';
	/// $firstFrame, $defenderId, isAttacked, $attackType, 
	/// ($initPosition.x, $initPosition.y), {$playerId:{$type:$maxNumberInvolved}}, 
	/// ($scoreGroundCDR, $scoreGroundRegion, $scoreAirCDR, $scoreAirRegion, $scoreDetectCDR, $scoreDetectRegion,
	/// $ecoImportanceCDR, $ecoImportanceRegion, $tactImportanceCDR, $tactImportanceRegion),
	/// {$playerId:{$type:$numberAtEnd}}, ($lastPosition.x, $lastPosition.y),
	/// {$playerId:$nbWorkersDead},$lastFrame, $winnerId

	BWAPI::TilePosition tmptp(it->initPosition);
	replayDat << it->firstFrame << "," << it->defender->getID() << ",IsAttacked," << tmpAttackType << ",("
		<< it->initPosition.x << "," << it->initPosition.y << "),"
		<< terrain->regionData.chokeDependantRegion[tmptp.x][tmptp.y] << ","
		<< terrain->hashRegionCenter(BWTA::getRegion(tmptp)) << ","
		<< tmpUnitTypes << ",("
		<< it->scoreGroundCDR << "," << it->scoreGroundRegion << ","
		<< it->scoreAirCDR << "," << it->scoreAirRegion << ","
		<< it->scoreDetectCDR << "," << it->scoreDetectRegion << ","
		<< it->economicImportanceCDR << "," << it->economicImportanceRegion << ","
		<< it->tacticalImportanceCDR << "," << it->tacticalImportanceRegion
		<< ")," << tmpUnitTypesEnd << ",(" << it->position.x << "," << it->position.y << "),"
		<< tmpWorkersDead << "," << Broodwar->getFrameCount();
	if (winner != NULL) {
		replayDat << ",winner:" << winner->getID() << "\n";
	} else {
		replayDat << "\n";
	}
	replayDat.flush();
}

void GameData::handleVisionEvents()
{
	std::map<BWAPI::Player, BWAPI::Unitset> seenThisTurn;

	for (const auto& p1 : activePlayers) {
		for (const auto& u : p1->getUnits()) {
			for (const auto& p2 : activePlayers) {
				if (p1 == p2) continue;
				for (const auto& visionPair : unseenUnits[p1]) {
					Unit visionTarget = visionPair.first;
					int sight = u->getType().sightRange();
					sight = sight * sight;
					BWAPI::Position diff = u->getPosition() - visionTarget->getPosition();
					int dist = (diff.x * diff.x) + (diff.y * diff.y);
					if (dist <= sight && seenThisTurn[p1].find(visionTarget) == seenThisTurn[p1].end()) {
						replayDat << Broodwar->getFrameCount() << "," << p2->getID() << ",Discovered," << visionTarget->getID() << "," << visionTarget->getType().getName() << "\n";
						//Event - Discovered
						seenThisTurn[p1].insert(visionTarget);
					}
				}
			}
		}
	}

	// remove from unseen, units seen this turn
	for (const auto& p : activePlayers) {
		for (const auto& u : seenThisTurn[p]) {
			unseenUnits[p].erase(std::pair<Unit, UnitType>(u, u->getType()));
		}
	}
}

void GameData::handleTechEvents()
{
	for (const auto& p : activePlayers) {
		std::map<Player, std::list<TechType>>::iterator currentTechIt = listCurrentlyResearching.find(p);
		for (const auto& currentResearching : BWAPI::TechTypes::allTechTypes()) {
			std::list<TechType>* techListPtr;
			if (currentTechIt != listCurrentlyResearching.end()) {
				techListPtr = &((*currentTechIt).second);
			} else {
				listCurrentlyResearching[p] = std::list<TechType>();
				techListPtr = &listCurrentlyResearching[p];
			}
			std::list<TechType> techList = (*techListPtr);

			bool wasResearching = false;
			for (const auto& lastFrameResearching : techList) {
				if (lastFrameResearching.getID() == currentResearching.getID()) {
					wasResearching = true;
					break;
				}
			}
			if (p->isResearching(currentResearching)) {
				if (!wasResearching) {
					listCurrentlyResearching[p].push_back(currentResearching);
					replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",StartResearch," << currentResearching.getName() << "\n";
					//Event - researching new tech
				}
			} else {
				if (wasResearching) {
					if (p->hasResearched(currentResearching)) {
						replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",FinishResearch," << currentResearching.getName() << "\n";
						if (listResearched.count(p) > 0) {
							listResearched[p].push_back(currentResearching);
						}
						listCurrentlyResearching[p].remove(currentResearching);
						//Event - research complete
					} else {
						replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",CancelResearch," << currentResearching.getName() << "\n";
						listCurrentlyResearching[p].remove(currentResearching);
						//Event - research canceled
					}
				}
			}

		}

		std::map<Player, std::list<UpgradeType>>::iterator currentUpgradeIt = listCurrentlyUpgrading.find(p);
		for (const auto& checkedUpgrade : BWAPI::UpgradeTypes::allUpgradeTypes()) {
			std::list<UpgradeType>* upgradeListPtr;
			if (currentUpgradeIt != listCurrentlyUpgrading.end()) {
				upgradeListPtr = &((*currentUpgradeIt).second);
			} else {
				listCurrentlyUpgrading[p] = std::list<UpgradeType>();
				upgradeListPtr = &listCurrentlyUpgrading[p];
			}
			std::list<UpgradeType> upgradeList = (*upgradeListPtr);


			bool wasResearching = false;
			for (const auto& lastFrameUpgrading : upgradeList) {
				if (lastFrameUpgrading.getID() == checkedUpgrade.getID()) {
					wasResearching = true;
					break;
				}
			}
			if (p->isUpgrading(checkedUpgrade)) {
				if (!wasResearching) {
					listCurrentlyUpgrading[p].push_back(checkedUpgrade);
					replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",StartUpgrade," << checkedUpgrade.getName() << "," << (p->getUpgradeLevel(checkedUpgrade) + 1) << "\n";
					//Event - researching new upgrade
				}
			} else {
				if (wasResearching) {
					int lastlevel = 0;
					for (const auto& upgradePair : listUpgraded[p]) {
						if (upgradePair.first == checkedUpgrade && upgradePair.second > lastlevel) {
							lastlevel = upgradePair.second;
						}
					}
					if (p->getUpgradeLevel(checkedUpgrade) > lastlevel) {
						replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",FinishUpgrade," << checkedUpgrade.getName() << "," << p->getUpgradeLevel(checkedUpgrade) << "\n";
						if (listUpgraded.count(p) > 0) {
							listUpgraded[p].push_back(std::pair<UpgradeType, int>(checkedUpgrade, p->getUpgradeLevel(checkedUpgrade)));
						}
						listCurrentlyUpgrading[p].remove(checkedUpgrade);
						//Event - upgrade complete
					} else {
						replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",CancelUpgrade," << checkedUpgrade.getName() << "," << (p->getUpgradeLevel(checkedUpgrade) + 1) << "\n";
						listCurrentlyUpgrading[p].remove(checkedUpgrade);
						//Event - upgrade canceled
					}
				}
			}
		}
	}
}

void GameData::onFrame()
{
	if (CREATE_RLD) onUpdateAttacks();

	// Update resources
	if (Broodwar->getFrameCount() % RESOURCES_REFRESH == 0) {
		for (const auto& p : activePlayers) {
			replayDat << Broodwar->getFrameCount() << "," << p->getID() << ",R," << p->minerals() << "," << p->gas() << "," << p->gatheredMinerals() << "," << p->gatheredGas() << "," << p->supplyUsed() << "," << p->supplyTotal() << "\n";
		}
	}

	handleTechEvents();
	if (Broodwar->getFrameCount() % 12 == 0) {
		handleVisionEvents();
	}

	// check last drop order (for attack type)
	for (const auto& u : Broodwar->getAllUnits()) {
		if (!u->getType().isBuilding() && u->getType().spaceProvided()
			&& (u->getOrder() == Orders::Unload || u->getOrder() == Orders::MoveUnload))
			lastDropOrderByPlayer[u->getPlayer()] = Broodwar->getFrameCount();
	}

}

void GameData::onReceiveText(BWAPI::Player player, std::string text)
{
	replayDat << Broodwar->getFrameCount() << "," << player->getID() << ",SendMessage," << text << "\n";
}

void GameData::onPlayerLeft(BWAPI::Player player)
{
	replayDat << Broodwar->getFrameCount() << "," << player->getID() << ",PlayerLeftGame\n";
}

void GameData::onNukeDetect(BWAPI::Position target)
{
	replayDat << Broodwar->getFrameCount() << "," << "-1" << ",NuclearLaunch,(" << target.x << "," << target.y << ")\n";
}

void GameData::onUnitCreate(BWAPI::Unit unit)
{
	replayDat << Broodwar->getFrameCount() << "," << unit->getPlayer()->getID() << ",Created," << unit->getID() << "," << unit->getType().getName() << ",(" << unit->getPosition().x << "," << unit->getPosition().y << ")\n";

	if (unit->getType() != BWAPI::UnitTypes::Zerg_Larva) {
		if (activePlayers.find(unit->getPlayer()) != activePlayers.end()) { // is from an active Player
			for (const auto& p : activePlayers) {
				if (p == unit->getPlayer()) continue;
				unseenUnits[p].insert(std::pair<Unit, UnitType>(unit, unit->getType()));
			}
		}
	}
}

void GameData::onUnitDestroy(BWAPI::Unit unit)
{
	if (CREATE_RLD) onNewAttack(unit);

	replayDat << Broodwar->getFrameCount() << "," << unit->getPlayer()->getID() << ",Destroyed," << unit->getID() << "," << unit->getType().getName() << ",(" << unit->getPosition().x << "," << unit->getPosition().y << ")\n";
	for (const auto& p : activePlayers) {
		if (p != unit->getPlayer()) {
			unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, unit->getType()));
		}
	}
}

void GameData::onNewAttack(BWAPI::Unit unitKilled)
{
	// A somewhat biased (because it waits for a unit to die to declare there 
	// was an attack) heuristic to detect who attacks who

	// Check if it is part of an existing attack (a continuation)
	for (const auto& attack : attacks) {
		if (unitKilled->getPosition().getDistance(attack.position) < attack.radius)
			return;
	}

	// Initialization
	std::map<Player, Unitset> playerUnits = getPlayerMilitaryUnitsNotInAttack(
		Broodwar->getUnitsInRadius(unitKilled->getPosition(), (int)MAX_ATTACK_RADIUS));

	// Removes lonely scout (Probes, Zerglings, Obs) dying or attacks with one unit which did NO kill (epic fails)
	if (playerUnits[unitKilled->getPlayer()].empty() || playerUnits[unitKilled->getLastAttackingPlayer()].empty())
		return;

	// It's a new attack, seek for attacking players
	Playerset attackingPlayers = activePlayers;
	std::map<BWAPI::Player, int> scorePlayersPosition;
	for (const auto& p : activePlayers) {
		scorePlayersPosition.insert(std::make_pair(p, 0)); // could put a prior on the start location distance
	}
	for (const auto& p : activePlayers) {
		for (Unit u : playerUnits[p]) {
			if (u->getType().isResourceContainer()) {
				scorePlayersPosition[p] += 12;
			} else if (u->getType().isWorker()) {
				scorePlayersPosition[p] += 1;
			} else if (u->getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode) {
				scorePlayersPosition[p] += 1;
			}
		}
	}
	Player defender = unitKilled->getPlayer();
	int maxScore = 0;
	for (const auto& ps : scorePlayersPosition) {
		if (ps.second > maxScore) {
			maxScore = ps.second;
			defender = ps.first;
		}
	}
	attackingPlayers.erase(defender);

	// Determine the position of the attack
	BWAPI::Position tmpPos(0, 0);
	int attackers = 0;
	for (const auto& p : activePlayers) {
		for (const auto& u : playerUnits[p]) {
			if (u->isAttacking() || u->isUnderAttack()) {
				tmpPos += u->getPosition();
				++attackers;
			}
		}
	}
	BWAPI::Position attackPos = unitKilled->getPosition();
	double radius = MAX_ATTACK_RADIUS;
	if (attackers > 0) {
		radius = 0.0;
		attackPos = BWAPI::Position(tmpPos.x / attackers, tmpPos.y / attackers);
		for (const auto& p : activePlayers) {
			for (const auto& tmp : playerUnits[p]) {
				double range_and_dist = tmp->getDistance(attackPos) +
					std::max(tmp->getType().groundWeapon().maxRange(),
					tmp->getType().airWeapon().maxRange());
				if ((tmp->isAttacking() || tmp->isUnderAttack())
					&& range_and_dist > radius)
					radius = range_and_dist;
			}
		}
		if (radius < MIN_ATTACK_RADIUS)
			radius = MIN_ATTACK_RADIUS;
	}
#ifdef __DEBUG_OUTPUT__
	Broodwar->setScreenPosition(max(0, attackPos.x - 320), max(0, attackPos.y - 240));
#endif

	/// Determine the attack type
	std::set<AttackType> currentAttackType;
	for (const auto& p : attackingPlayers) {
		for (Unit tmp : playerUnits[p]) {
			UnitType ut = tmp->getType();
			std::string nameStr = ut.getName();
			if (ut.canAttack()
				&& !ut.isBuilding() // ruling out tower rushes :(
				&& !ut.isWorker())
			{
				if (ut.isFlyer()) {
					if (ut.spaceProvided() > 0
						&& (Broodwar->getFrameCount() - lastDropOrderByPlayer[p]) < 24 * SECONDS_SINCE_LAST_ATTACK * 2)
						currentAttackType.insert(DROP);
					else if (ut.canAttack()
						|| ut == UnitTypes::Terran_Science_Vessel
						|| ut == UnitTypes::Zerg_Queen)
						currentAttackType.insert(AIR);
					// not DROP nor AIR for observers / overlords
				} else { // not a flier (ruling out obs)
					if (tmp->isCloaked() || tmp->getType() == UnitTypes::Zerg_Lurker) {
						currentAttackType.insert(INVIS);
					} else if (ut.canAttack()) {
						currentAttackType.insert(GROUND);
					}
				}
			}
		}
	}

	// Create the attack to the corresponding players
	attacks.push_back(Attack(currentAttackType, Broodwar->getFrameCount(), attackPos, radius, defender, playerUnits));

#ifdef __DEBUG_OUTPUT__
	// and record it
	for each (AttackType at in currentAttackType)
	{
		Broodwar->printf("Player %s is attacked at Position (%d,%d) type %d, %s",
			defender->getName().c_str(), attackPos.x, attackPos.y, at, attackTypeToStr(at).c_str());
		//replayDat << Broodwar->getFrameCount() << "," << attackTypeToStr(at).c_str() << "," << defender << "," << ",(" << attackPos.x << "," << attackPos.y <<")\n";
	}
#endif
}

std::map<BWAPI::Player, BWAPI::Unitset> GameData::getPlayerMilitaryUnitsNotInAttack(const Unitset& unitsAround)
{
	std::map<Player, Unitset> playerUnits;
	for (const auto& p : activePlayers) playerUnits.insert(make_pair(p, Unitset()));
	for (const auto& u : unitsAround) {
		if (isInofensiveUnit(u)) continue;
		bool found = false;
		for (auto& a : attacks) {
			if (a.battleUnits[u->getPlayer()].count(u)) {
				found = true;
				break;
			}
		}
		if (!found) playerUnits[u->getPlayer()].insert(u);
	}
	return playerUnits;
}

void GameData::onUnitMorph(BWAPI::Unit unit)
{
	replayDat << Broodwar->getFrameCount() << "," << unit->getPlayer()->getID() << ",Morph," << unit->getID() << "," << unit->getType().getName() << ",(" << unit->getPosition().x << "," << unit->getPosition().y << ")\n";
	for (const auto& p : activePlayers) {
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Egg) {
			if (p != unit->getPlayer()) {
				if (unit->getType().getRace() == BWAPI::Races::Zerg) {
					if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker) {
						unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Hydralisk));
						unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Lurker_Egg));
					} else if (unit->getType() == BWAPI::UnitTypes::Zerg_Devourer || unit->getType() == BWAPI::UnitTypes::Zerg_Guardian) {
						unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Mutalisk));
						unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Cocoon));
					} else if (unit->getType().getRace() == BWAPI::Races::Zerg && unit->getType().isBuilding()) {
						if (unit->getType() == BWAPI::UnitTypes::Zerg_Lair) {
							unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Hatchery));
						} else if (unit->getType() == BWAPI::UnitTypes::Zerg_Hive) {
							unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Lair));
						} else if (unit->getType() == BWAPI::UnitTypes::Zerg_Greater_Spire) {
							unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Spire));
						} else if (unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
							unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Creep_Colony));
						} else if (unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony) {
							unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Creep_Colony));
						} else {
							unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Zerg_Drone));
						}
					}
				} else if (unit->getType().getRace() == BWAPI::Races::Terran) {
					if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
						unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode));
					} else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
						unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode));
					}
				}
				if (activePlayers.find(unit->getPlayer()) != activePlayers.end()) {
					unseenUnits[p].insert(std::pair<Unit, UnitType>(unit, unit->getType()));
				}
			}
		}
	}
}

void GameData::onUnitRenegade(BWAPI::Unit unit)
{
	replayDat << Broodwar->getFrameCount() << "," << unit->getPlayer()->getID() << ",ChangedOwnership," << unit->getID() << "\n";
	for (const auto& p : activePlayers) {
		if (p != unit->getPlayer()) {
			if (activePlayers.find(unit->getPlayer()) != activePlayers.end()) {
				unseenUnits[p].insert(std::pair<Unit, UnitType>(unit, unit->getType()));
			}
		} else {
			unseenUnits[p].erase(std::pair<Unit, UnitType>(unit, unit->getType()));
		}
	}
}