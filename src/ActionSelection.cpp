#include "ActionSelection.h"

using namespace BWAPI;

AbstractOrder::Order getAbstractOrder(const BWAPI::Order& order, const RegionID& targetRegion, const RegionID& actualRegion)
{
	if (order == BWAPI::Orders::Move
		|| (order == BWAPI::Orders::AttackMove && targetRegion != actualRegion)
		|| order == BWAPI::Orders::ComputerReturn
		|| order == BWAPI::Orders::EnterTransport
		|| order == BWAPI::Orders::Follow
		|| order == BWAPI::Orders::ResetCollision
		|| order == BWAPI::Orders::HealMove
		|| order == BWAPI::Orders::Patrol)
	{
		return AbstractOrder::Move;
	} else if (order == BWAPI::Orders::AttackUnit
		|| (order == BWAPI::Orders::AttackMove && targetRegion == actualRegion)
		|| order == BWAPI::Orders::CastStasisField
		|| order == BWAPI::Orders::CastPsionicStorm
		|| order == BWAPI::Orders::CastNuclearStrike
		|| order == BWAPI::Orders::NukeUnit
		|| order == BWAPI::Orders::NukeTrack
		|| order == BWAPI::Orders::CastLockdown
		|| order == BWAPI::Orders::CastEMPShockwave
		|| order == BWAPI::Orders::CastDefensiveMatrix
		|| order == BWAPI::Orders::FireYamatoGun)
	{
		return AbstractOrder::Attack;
	} else if (order == BWAPI::Orders::Stop
		|| order == BWAPI::Orders::PlayerGuard 
		|| order == BWAPI::Orders::Guard
		|| order == BWAPI::Orders::HoldPosition
// 		|| order == BWAPI::Orders::PlaceMine
// 		|| order == BWAPI::Orders::ArchonWarp
// 		|| order == BWAPI::Orders::CompletingArchonSummon
// 		|| order == BWAPI::Orders::Sieging
		)
	{
		return AbstractOrder::Idle;
	} else if (order == BWAPI::Orders::Nothing) {
		return AbstractOrder::Nothing;
	} else {
		DEBUG("Unknown action: " << order.getName());
		return AbstractOrder::Unknown;
	}
}

ActionSelection::ActionSelection()
{
	// creating the output file
	std::string outFilePath = Broodwar->mapPathName() + ".asd";
	outFile.open(outFilePath);

	// Sort regions
	const std::set<BWTA::Region*>& unsortedRegions = BWTA::getRegions();
	std::set<BWTA::Region*, SortByXY> sortedRegions(unsortedRegions.begin(), unsortedRegions.end());

	// Fill map to regionID variable
	RegionID id = 0;
	for (const auto& r : sortedRegions) {
		regionID[r] = id;
		regionFromID[id] = r;
		id++;
	}

	// creating regionIdMap
	regionIdMap.resize(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
	regionIdMap.setTo(0);
	for (int x = 0; x < Broodwar->mapWidth(); ++x) {
		for (int y = 0; y < Broodwar->mapHeight(); ++y) {
			BWTA::Region* tileRegion = BWTA::getRegion(x, y);
			if (tileRegion == nullptr) tileRegion = getNearestRegion(x, y);
			regionIdMap[x][y] = regionID[tileRegion];
		}
	}

	// Calculate distance between regions
	distanceBetweenRegions.resize(sortedRegions.size(), sortedRegions.size());
	for (const auto& j : sortedRegions) {
		for (const auto& k : sortedRegions) {
			int dist;
			Position pos1 = j->getCenter();
			Position pos2 = k->getCenter();
			// sometimes the center of the region is in an unwalkable area
			if (Broodwar->isWalkable(BWAPI::WalkPosition(pos1)) &&
				Broodwar->isWalkable(BWAPI::WalkPosition(pos2))) {
				dist = (int)BWTA::getGroundDistance(BWAPI::TilePosition(pos1), BWAPI::TilePosition(pos2));
			} else { // TODO improve this to get a proper position and compute groundDistance
				dist = (int)pos1.getDistance(pos2);
			}
			RegionID id1 = regionID[j];
			RegionID id2 = regionID[k];
			distanceBetweenRegions[id1][id2] = dist;
		}
	}

	// detect real players
	for (const auto& player : Broodwar->getPlayers()){
		if (!player->getUnits().empty() && !player->isNeutral()) {
			activePlayers.insert(player);
		}
	}
}

ActionSelection::~ActionSelection()
{
	outFile.close();
}

void ActionSelection::onFrame()
{
	updateRegionOccupancyMap();

	// create abstract groups
	AbstractGroupVector abstractGroup;
	for (const auto& u : Broodwar->getAllUnits()) {
		if (u->getType().isWorker()) continue; // ignore workers
		if (!u->getType().canMove()) continue; // ignore units that cannot move
		if (u->getType() == UnitTypes::Terran_Vulture_Spider_Mine
			|| u->getType() == UnitTypes::Terran_Nuclear_Missile) continue;
		if (!isMilitaryUnit(u)) continue;
		if (u->getOrder() == BWAPI::Orders::Follow && !u->getTarget()) continue; // following unknown unit
		// ignoring some "transition" orders
		if (u->getOrder() == BWAPI::Orders::PlaceMine
			|| u->getOrder() == BWAPI::Orders::MedicHeal
			|| u->getOrder() == BWAPI::Orders::Medic
			|| u->getOrder() == BWAPI::Orders::MedicHealToIdle
			|| u->getOrder() == BWAPI::Orders::ArchonWarp
			|| u->getOrder() == BWAPI::Orders::CompletingArchonSummon
			|| u->getOrder() == BWAPI::Orders::CastRestoration
			|| u->getOrder() == BWAPI::Orders::Sieging)
		{
			continue;
		}

// 		outFile << u->getType() << "[" << getRegionID(u) << "]" 
// 			<< "," << u->getOrder() <<  "[" << getRegionID(u->getTargetPosition()) << "]" 
// 			<< '\n';

		// adding group
		BWAPI::Order order = u->getOrder();
		RegionID currentRegionID = getRegionID(u);
		RegionID targetRegionID = getRegionID(u->getTargetPosition());

		// mapping RightClickAction
		if (order == BWAPI::Orders::RightClickAction) {
			if (u->getTarget() && u->getTarget()->getPlayer() != u->getPlayer()) {
				order = BWAPI::Orders::AttackUnit;
			} else {
				order = BWAPI::Orders::Move;
			}
		}
		// following is like moving to target position
		if (order == BWAPI::Orders::Follow && u->getTarget()) {
			order = BWAPI::Orders::Move;
			targetRegionID = getRegionID(u->getTargetPosition());
		}
		// if attackMove but enemies in same region, change to attack this region
		if (order == BWAPI::Orders::AttackMove) {
			if (isEnemyAtRegion(u->getPlayer(), currentRegionID)) {
				order = BWAPI::Orders::AttackUnit;
				targetRegionID = getRegionID(u->getPosition());
			} else {
				order = BWAPI::Orders::Move;
			}
		}

		AbstractOrder::Order abstractOrder = getAbstractOrder(order, targetRegionID, currentRegionID);
		// ignoring moving to the same region
		if (abstractOrder == AbstractOrder::Order::Move && targetRegionID == currentRegionID) {
			abstractOrder = AbstractOrder::Order::Idle;
			continue;
		}
		// if IDLE when enemies in the same region, mark as ATTACK (usually they move towards enemy)
		if (abstractOrder == AbstractOrder::Order::Idle && isEnemyAtRegion(u->getPlayer(), currentRegionID)) {
			abstractOrder = AbstractOrder::Order::Attack;
			continue;
		}


		// testing action
		if (order == BWAPI::Orders::Follow) {
			LOG("Unit " << u->getType() << " at " << u->getPosition() << " is following to pos " << u->getTargetPosition());
			if (u->getTarget()) {
				LOG(" - unit " << u->getTarget()->getType() << " at " << u->getTarget()->getPosition() << " (can move: " << u->getTarget()->getType().canMove()  << ") with target: " << u->getTarget()->getTargetPosition());
			}
			else LOG(" - unknown unit and target position: " << u->getTargetPosition());
		}
		if (order == BWAPI::Orders::RightClickAction) {
			LOG("Unit " << u->getType() << " at " << u->getPosition() << " rightClick to pos " << u->getTargetPosition());
			if (u->getTarget()) {
				LOG(" - unit " << u->getTarget()->getType() << " at " << u->getTarget()->getPosition() << " with target pos: " << u->getTarget()->getTargetPosition());
			} else LOG(" - unknown unit");
		}

		abstractGroup[u->getPlayer()][u->getType()][currentRegionID].addOrder(abstractOrder, targetRegionID);
	}

	
	for (auto& it : abstractGroup) {
		Player p(it.first);
		for (auto& it2 : it.second) {
			UnitType ut(it2.first);
			for (auto& it3 : it2.second) {
				RegionID regId = it3.first;

				// -- calculate most common abstract order
				// compute orders frequencies
				std::map<AbstractOrder::Order, std::map<RegionID, int>> orderFreq;
				for (size_t i = 0; i < it3.second.orderVector.size(); ++i) {
					orderFreq[it3.second.orderVector[i]][it3.second.targetRegionVector[i]] += 1;
				}

				// get order with max frequency
				int maxFreq = 0;
				AbstractOrder::Order bestOrder;
				RegionID bestTargetReg;
				for (auto const& mapFreq : orderFreq) {
					for (auto const& mapFreq2 : mapFreq.second) {
						if (mapFreq2.second > maxFreq) {
							maxFreq = mapFreq2.second;
							bestOrder = mapFreq.first;
							bestTargetReg = mapFreq2.first;
						}
					}
				}
				it3.second.setCommonOrder(bestOrder, bestTargetReg);


				// -- if group doesn't exist on previous state, output abstract action decision
				if (!groupExist(p, ut, regId, it3.second)) {
					if (!isEqualToLastPrintedOrder(p, ut, regId, bestOrder)) {
						// if order is move, get a neighbor region
						if (bestOrder == AbstractOrder::Move) {
							bestTargetReg = getBestNeighbor(regId, bestTargetReg);
						}
						std::string posibleActions = getPossibleActions(regId, p);

						// print current action
						outFile << ut.getID() << "," << ut << "," << regId << "," 
							<< AbstractOrder::getName[bestOrder] << "," << bestTargetReg;

						// print possible actions
						outFile << "#" << posibleActions;

						// print regions features
						outFile << "#" << getRegionProperties(regId, p, regId);
						// for each neighbor region
						for (const auto& r : getNeighbors(regId)) {
							RegionID nr = regionID.at(r);
							outFile << "#" << getRegionProperties(nr, p, regId);
						}

// 						outFile << ",Enemy:" << isEnemyAtRegion(p, regId) << ",Size:" << it3.second.orderVector.size() << ",Frame:" << Broodwar->getFrameCount();
						outFile << '\n';

						// update last print
						lastAbstractGroupOrder[p][ut][regId] = bestOrder;
					}
				}
			}
		}
	}

	// update lastAbstractGroup
	lastAbstractGroup = abstractGroup;
}

const bool ActionSelection::isMovingToSameRegion(AllOrders order, RegionID regId) const
{
	return order.commonOrder == AbstractOrder::Move && regId == order.commonTargetRegion;
}

const bool ActionSelection::groupExist(BWAPI::Player p, BWAPI::UnitType ut, RegionID regId, AllOrders order) const
{
	return lastAbstractGroup.count(p)
		&& lastAbstractGroup.at(p).count(ut)
		&& lastAbstractGroup.at(p).at(ut).count(regId)
		&& lastAbstractGroup.at(p).at(ut).at(regId) == order;
}

const bool ActionSelection::isEqualToLastPrintedOrder(BWAPI::Player p, BWAPI::UnitType ut, RegionID regId, AbstractOrder::Order order) const
{
	return lastAbstractGroupOrder.count(p)
		&& lastAbstractGroupOrder.at(p).count(ut)
		&& lastAbstractGroupOrder.at(p).at(ut).count(regId)
		&& lastAbstractGroupOrder.at(p).at(ut).at(regId) == order;
}

BWTA::Region* ActionSelection::getNearestRegion(int x, int y)
{
	//searches outward in a spiral.
	int length = 1;
	int j = 0;
	bool first = true;
	int dx = 0;
	int dy = 1;
	BWTA::Region* tileRegion = nullptr;
	while (length < Broodwar->mapWidth()) //We'll ride the spiral to the end
	{
		//if is a valid regions, return it
		tileRegion = BWTA::getRegion(x, y);
		if (tileRegion != nullptr) return tileRegion;

		//otherwise, move to another position
		x = x + dx;
		y = y + dy;
		//count how many steps we take in this direction
		j++;
		if (j == length) { //if we've reached the end, its time to turn
			j = 0;	//reset step counter

			//Spiral out. Keep going.
			if (!first)
				length++; //increment step counter if needed


			first = !first; //first=true for every other turn so we spiral out at the right rate

			//turn counter clockwise 90 degrees:
			if (dx == 0) {
				dx = dy;
				dy = 0;
			} else {
				dy = -dx;
				dx = 0;
			}
		}
		//Spiral out. Keep going.
	}
	return tileRegion;
}

const RegionID ActionSelection::getRegionID(const BWAPI::Unit& u) const
{
	return getRegionID(u->getTilePosition());
}

const RegionID ActionSelection::getRegionID(const BWAPI::TilePosition& tilePos) const
{
	return regionIdMap[tilePos.x][tilePos.y];
}

void ActionSelection::updateRegionOccupancyMap()
{
	playerRegionOccupancyMap.clear();
	for (const auto& p : activePlayers) {
		for (const auto& u : p->getUnits()) {
			playerRegionOccupancyMap[getRegionID(u)].insert(p);
		}
	}
}

const bool ActionSelection::isEnemyAtRegion(BWAPI::Player p, RegionID r) const
{
	if (!playerRegionOccupancyMap.count(r)) return false;
	for (const auto& p2 : playerRegionOccupancyMap.at(r)) {
		if (p != p2) return true;
	}
	return false;
}

const bool ActionSelection::isFriendAtRegion(BWAPI::Player p, RegionID r) const
{
	if (!playerRegionOccupancyMap.count(r)) return false;
	for (const auto& p2 : playerRegionOccupancyMap.at(r)) {
		if (p == p2) return true;
	}
	return false;
}

RegionID ActionSelection::getBestNeighbor(RegionID fromRegId, RegionID toRegId) const
{
	// get neighbors
	std::set<BWTA::Region*> neighbors = getNeighbors(fromRegId);

	// find closest region
	Position toPos = regionFromID.at(toRegId)->getCenter();
	BWTA::Region* bestReg = nullptr;
	int minDist = std::numeric_limits<int>::max();
	for (const auto& r : neighbors) {
// 		if (!regionID.count(r)) {
// 			DEBUG("Not valid region, possible bug in BWTA");
// 			continue;
// 		}
		if (toRegId == regionID.at(r)) {
			return toRegId;
		}
		int dist = distanceBetweenRegions[toRegId][regionID.at(r)];
		if (dist < minDist) {
			bestReg = r;
			minDist = dist;
		}
	}
	if (bestReg == nullptr) return toRegId; // the region has not neighbors
	return regionID.at(bestReg);
}

std::set<BWTA::Region*> ActionSelection::getNeighbors(RegionID regId) const
{
	BWTA::Region* region = regionFromID.at(regId);
	auto chokes = region->getChokepoints();
	std::set<BWTA::Region*> neighbors;
	for (const auto& c : chokes) {
		BWTA::Region* r = nullptr;
		if (c->getRegions().first != region) r = c->getRegions().first;
		else r = c->getRegions().second;
		if (!regionID.count(r)) {
			DEBUG("Not valid region, possible bug in BWTA");
			continue;
		}
		neighbors.insert(r);
	}
	return neighbors;
}

std::string ActionSelection::getPossibleActions(RegionID regId, BWAPI::Player p) const
{
	// IDLE action is omitted since it's always possible
	std::stringstream actions;
	if (isEnemyAtRegion(p, regId)) {
		actions << "ATTACK,";
	}

	std::set<BWTA::Region*> neighbors = getNeighbors(regId);
	if (!neighbors.empty()) {
		auto last = neighbors.end();
		--last;
		for (auto it = neighbors.begin(); it != last; ++it) {
			if (!regionID.count(*it)) DEBUG("Region not found");
			actions << "MOVE:" << regionID.at(*it) << ",";
		}
		if (!regionID.count(*last)) DEBUG("Region not found");
		actions << "MOVE:" << regionID.at(*last);
	}
	return actions.str();
}

std::string ActionSelection::getRegionProperties(RegionID regId, BWAPI::Player p, RegionID fromRegId) const
{
	std::stringstream ret;
	ret << regId << ":";

	// are friendly units?
	ret << isFriendAtRegion(p, regId) << ",";
	// are enemy units?
	ret << isEnemyAtRegion(p, regId) << ",";

	if (regId == fromRegId) {
		ret << "0,0"; // we are not advancing regions
	} else {
		int minDistToEnemyBaseActualReg = std::numeric_limits<int>::max();
		int minDistToEnemyBaseTargetReg = std::numeric_limits<int>::max();
		int minDistToFriendBaseActualReg = std::numeric_limits<int>::max();
		int minDistToFriendBaseTargetReg = std::numeric_limits<int>::max();
		for (const auto& b : bases) {
			BWTA::Region* baseReg = BWTA::getRegion(b->getPosition());
			if (!regionID.count(baseReg)) {
// 				DEBUG("Region not found"); // usually because the building is lifted in a non walkable region 
				continue;
			}
			RegionID regionBase = regionID.at(baseReg);
			int distFromActualReg = distanceBetweenRegions[fromRegId][regionBase];
			int distFromTargetReg = distanceBetweenRegions[regId][regionBase];
			if (b->getPlayer() == p) { // is a friendly base
				minDistToFriendBaseActualReg = std::min(minDistToFriendBaseActualReg, distFromActualReg);
				minDistToFriendBaseTargetReg = std::min(minDistToFriendBaseTargetReg, distFromTargetReg);
			} else { // is an enemy base
				minDistToEnemyBaseActualReg = std::min(minDistToEnemyBaseActualReg, distFromActualReg);
				minDistToEnemyBaseTargetReg = std::min(minDistToEnemyBaseTargetReg, distFromTargetReg);
			}
		}
		// moving to our base?
		ret << (minDistToFriendBaseTargetReg < minDistToFriendBaseActualReg) << ",";
		// moving to enemy base?
		ret << (minDistToEnemyBaseTargetReg < minDistToEnemyBaseActualReg);
	}
	return ret.str();
}

void ActionSelection::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getType().isResourceDepot()) {
// 		if (activePlayers.find(unit->getPlayer()) != activePlayers.end()) {
			bases.insert(unit);
// 		}
	}
}

void ActionSelection::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getType().isResourceDepot()) {
		bases.erase(unit);
	}
}