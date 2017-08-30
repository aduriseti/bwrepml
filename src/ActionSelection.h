#pragma once

#include "Utils.h"

using RegionID = size_t;

struct SortByXY
{
	bool operator ()(const BWTA::Region* const &lReg, const BWTA::Region* const &rReg) const
	{
		BWAPI::Position lPos = lReg->getCenter();
		BWAPI::Position rPos = rReg->getCenter();
		if (lPos.x == rPos.x) {
			return lPos.y < rPos.y;
		} else {
			return lPos.x < rPos.x;
		}
	}

	bool operator ()(const BWTA::Chokepoint* const &lReg, const BWTA::Chokepoint* const &rReg) const
	{
		BWAPI::Position lPos = lReg->getCenter();
		BWAPI::Position rPos = rReg->getCenter();
		if (lPos.x == rPos.x) {
			return lPos.y < rPos.y;
		} else {
			return lPos.x < rPos.x;
		}
	}
};

namespace AbstractOrder {
	static enum Order {
		Unknown, Nothing, Idle, Gas, Mineral, Move, Attack, Heal
	};
	static std::string getName[8] = {
		"Unknown", "Nothing", "Idle", "Gas", "Mineral", "Move", "Attack", "Heal"
	};
}
AbstractOrder::Order getAbstractOrder(const BWAPI::Order& order, const RegionID& targetRegion, const RegionID& actualRegion);

struct AllOrders
{
	std::vector<AbstractOrder::Order> orderVector;
	std::vector<RegionID> targetRegionVector;
	AbstractOrder::Order commonOrder;
	RegionID commonTargetRegion;

	AllOrders() :commonOrder(AbstractOrder::Unknown){};

	friend bool operator ==(const AllOrders &o1, const AllOrders &o2) {
		return (o1.commonOrder == o2.commonOrder && o1.commonTargetRegion == o2.commonTargetRegion);
	};

	void addOrder(AbstractOrder::Order order, RegionID regID) {
		orderVector.push_back(order);
		targetRegionVector.push_back(regID);
	};

	void setCommonOrder(AbstractOrder::Order order, RegionID regId) {
		commonOrder = order;
		commonTargetRegion = regId;
	};

};

using AbstractGroupVector = std::map < BWAPI::Player, std::map<BWAPI::UnitType, std::map<RegionID, AllOrders> > > ;
using AbstractGroupOrderVector = std::map < BWAPI::Player, std::map<BWAPI::UnitType, std::map<RegionID, AbstractOrder::Order> > >;

class ActionSelection
{
public:
	ActionSelection();
	~ActionSelection();

	void onFrame();
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitDestroy(BWAPI::Unit unit);

private:
	std::ofstream outFile;
	std::map<BWTA::Region*, RegionID> regionID;
	std::map<RegionID, BWTA::Region*> regionFromID;
	BWTA::RectangleArray<RegionID> regionIdMap;
	BWTA::RectangleArray<int> distanceBetweenRegions;
	AbstractGroupVector lastAbstractGroup;
	AbstractGroupOrderVector lastAbstractGroupOrder;
	std::map<RegionID, std::set<BWAPI::Player>> playerRegionOccupancyMap;
	BWAPI::Unitset bases;

	BWTA::Region* getNearestRegion(int x, int y);
	const RegionID getRegionID(const BWAPI::Unit& u) const;
	const RegionID getRegionID(const BWAPI::TilePosition& tilePos) const;
	const RegionID getRegionID(const BWAPI::Position& pos) const { return getRegionID(BWAPI::TilePosition(pos)); };
	const bool groupExist(BWAPI::Player p, BWAPI::UnitType ut, RegionID regId, AllOrders order) const;
	const bool isEqualToLastPrintedOrder(BWAPI::Player p, BWAPI::UnitType ut, RegionID regId, AbstractOrder::Order order) const;
	const bool isMovingToSameRegion(AllOrders order, RegionID regId) const;
	void updateRegionOccupancyMap();
	const bool isEnemyAtRegion(BWAPI::Player p, RegionID r) const;
	const bool isFriendAtRegion(BWAPI::Player p, RegionID r) const;
	RegionID getBestNeighbor(RegionID fromRegId, RegionID toRegId) const;
	std::set<BWTA::Region*> getNeighbors(RegionID regId) const;
	std::string getPossibleActions(RegionID regId, BWAPI::Player p) const;
	std::string getRegionProperties(RegionID regId, BWAPI::Player p, RegionID fromRegId) const;
};