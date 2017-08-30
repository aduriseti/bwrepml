#pragma once

#include "Utils.h"

class OrderData
{
public:
	OrderData(); // Generates ROD file
	~OrderData();
	void onFrame();

private:
	std::ofstream replayOrdersDat;
	std::map<BWAPI::Unit, BWAPI::Order> unitOrders;
	std::map<BWAPI::Unit, BWAPI::Unit> unitOrdersTargets;
	std::map<BWAPI::Unit, BWAPI::Position> unitOrdersTargetPositions;
	std::map<BWAPI::Unit, int> minerResourceGroup;
};