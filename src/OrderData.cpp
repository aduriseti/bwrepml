#include "OrderData.h"

using namespace BWAPI;

OrderData::OrderData()
{
	std::string ordersfilepath = Broodwar->mapPathName() + ".rod";
	replayOrdersDat.open(ordersfilepath.c_str());
}

OrderData::~OrderData()
{
	replayOrdersDat.close();
}

void OrderData::onFrame()
{
	for (const auto& u : Broodwar->getAllUnits()) {
		bool mining = isGatheringResources(u);
		bool newOrders = false;

		if ((!mining || (u->isGatheringMinerals() && u->getOrder() != BWAPI::Orders::WaitForMinerals && u->getOrder() != BWAPI::Orders::MiningMinerals && u->getOrder() != BWAPI::Orders::ReturnMinerals) ||
			(u->isGatheringGas() && u->getOrder() != BWAPI::Orders::WaitForGas && u->getOrder() != BWAPI::Orders::HarvestGas && u->getOrder() != BWAPI::Orders::ReturnGas))
			&&
			(u->getOrder() != BWAPI::Orders::ResetCollision) &&
			(u->getOrder() != BWAPI::Orders::Larva)
			)
		{
			if (unitOrders.count(u) != 0) {
				if (unitOrders[u] != u->getOrder() || unitOrdersTargets[u] != u->getOrderTarget() 
					|| unitOrdersTargetPositions[u] != u->getOrderTargetPosition())
				{
					unitOrders[u] = u->getOrder();
					unitOrdersTargets[u] = u->getOrderTarget();
					unitOrdersTargetPositions[u] = u->getOrderTargetPosition();
					newOrders = true;
				}
			} else {
				unitOrders[u] = u->getOrder();
				unitOrdersTargets[u] = u->getOrderTarget();
				unitOrdersTargetPositions[u] = u->getOrderTargetPosition();
				newOrders = true;
			}
		}

		if (mining) {
			int oldmins = -1;
			if (minerResourceGroup.count(u) != 0) {
				oldmins = minerResourceGroup[u];
			}
			if (u->getOrderTarget() != NULL) {
				if (u->getOrderTarget()->getResourceGroup() == oldmins) {
					newOrders = false;
				} else {
					minerResourceGroup[u] = u->getOrderTarget()->getResourceGroup();
				}
			}
		}

		if (newOrders && Broodwar->getFrameCount() > 0) {
			replayOrdersDat << Broodwar->getFrameCount() << "," << u->getID() << "," << u->getOrder().getName();
			if (u->getTarget() != NULL) {
				replayOrdersDat << ",T," << u->getTarget()->getPosition().x << "," << u->getTarget()->getPosition().y << "\n";
			} else {
				replayOrdersDat << ",P," << u->getTargetPosition().x << "," << u->getTargetPosition().y << "\n";
			}
		}
	}
}