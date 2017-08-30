#include "BWRepDump.h"

using namespace BWAPI;

// TODO maybe it's a better idea to get unit->getReplayID() instead of unit->getID()

void BWRepDump::onStart()
{
	//Broodwar->enableFlag(Flag::UserInput); // Enable some cheat flags
	//Broodwar->enableFlag(Flag::CompleteMapInformation); // Uncomment to enable complete map information
	Broodwar->setLocalSpeed(0);
	Broodwar->setFrameSkip(0);
	Broodwar->setGUI(false);

	// Analyze map with BWTA
	BWTA::readMap();
	BWTA::analyze();

	fileLog.open("bwapi-data\\logs\\BWRepDump.log", std::ios_base::app); //append the output
	LOG("[NEW REPLAY] " << Broodwar->mapPathName() << "," << Broodwar->mapHash());

	if (CREATE_RLD) terrain = new TerrainAnalyzer;
	if (CREATE_RGD) gameData = new GameData;
	if (CREATE_ROD) orderData = new OrderData;
	if (CREATE_RCD) combatTracker = new CombatTracker;
	if (CREATE_ASD) actionSelection = new ActionSelection;

	showBullets = false;
	showVisibilityData = false;
	unitDestroyedThisTurn = false;
}

void BWRepDump::onEnd(bool isWinner)
{
	if (CREATE_RGD) delete gameData;
	if (CREATE_RLD) delete terrain;
	if (CREATE_ROD) delete orderData;
	if (CREATE_RCD) delete combatTracker;
	if (CREATE_ASD) delete actionSelection;

	fileLog.close();
}

void BWRepDump::onFrame()
{
	if (showVisibilityData) drawVisibilityData();
	if (showBullets) drawBullets();

	if (REPLAY_TIME_LIMIT && Broodwar->getFrameCount() > REPLAY_TIME_LIMIT)
		Broodwar->leaveGame();

	if (CREATE_RGD) gameData->onFrame();
	if (CREATE_RCD) combatTracker->onFrame();
	if (CREATE_RLD) terrain->onFrame();
	if (CREATE_ROD) orderData->onFrame();
	if (CREATE_ASD) actionSelection->onFrame();

	unitDestroyedThisTurn = false;
}

void BWRepDump::onSendText(std::string text)
{
	if		(text == "n")			Broodwar->setLocalSpeed(50);
	else if (text == "/bullets")	showBullets = !showBullets;
	else if (text == "/players")	showPlayers();
	else if (text == "/forces")		showForces();
	else if (text == "/visibility") showVisibilityData = !showVisibilityData;
	else {
		Broodwar->printf("You typed '%s'!", text.c_str());
		Broodwar->sendText("%s", text.c_str());
	}
}

void BWRepDump::onReceiveText(BWAPI::Player player, std::string text)
{
	if (CREATE_RGD) gameData->onReceiveText(player, text);
}

void BWRepDump::onPlayerLeft(BWAPI::Player player)
{
	if (CREATE_RGD) gameData->onPlayerLeft(player);
}

void BWRepDump::onNukeDetect(BWAPI::Position target)
{
	if (CREATE_RGD) gameData->onNukeDetect(target);
}

void BWRepDump::onUnitDiscover(BWAPI::Unit unit){}
void BWRepDump::onUnitEvade(BWAPI::Unit unit){}
void BWRepDump::onUnitShow(BWAPI::Unit unit){}
void BWRepDump::onUnitHide(BWAPI::Unit unit){}

void BWRepDump::onUnitCreate(BWAPI::Unit unit)
{
	if (CREATE_RLD) terrain->onUnitCreate(unit);
	if (CREATE_RGD) gameData->onUnitCreate(unit);
	if (CREATE_ASD) actionSelection->onUnitCreate(unit);
}

void BWRepDump::onUnitDestroy(BWAPI::Unit unit)
{
	unitDestroyedThisTurn = true;
	if (CREATE_RGD) gameData->onUnitDestroy(unit);
	if (CREATE_RCD) combatTracker->onUnitDestroy(unit);
	if (CREATE_ASD) actionSelection->onUnitDestroy(unit);
}

void BWRepDump::onUnitMorph(BWAPI::Unit unit)
{
	if (CREATE_RGD) gameData->onUnitMorph(unit);
}

void BWRepDump::onUnitRenegade(BWAPI::Unit unit)
{
	if (CREATE_RGD) gameData->onUnitRenegade(unit);
}

void BWRepDump::drawStats()
{
	Unitset myUnits = Broodwar->self()->getUnits();
	Broodwar->drawTextScreen(5,0,"I have %d units:",myUnits.size());
	std::map<UnitType, int> unitTypeCounts;
	for (const auto& u : myUnits) {
		if (unitTypeCounts.find(u->getType())==unitTypeCounts.end()) {
			unitTypeCounts.insert(std::make_pair(u->getType(),0));
		}
		unitTypeCounts.find(u->getType())->second++;
	}
	int line = 1;
	for (const auto& uCount : unitTypeCounts) {
		Broodwar->drawTextScreen(5, 16 * line, "- %d %ss", uCount.second, uCount.first.getName().c_str());
		line++;
	}
}

void BWRepDump::drawBullets()
{
	for (const auto& b : Broodwar->getBullets()) {
		Position p(b->getPosition());
		double velocityX = b->getVelocityX();
		double velocityY = b->getVelocityY();
		if (b->getPlayer() == Broodwar->self()) {
			Broodwar->drawLineMap(p.x,p.y,p.x+(int)velocityX,p.y+(int)velocityY,Colors::Green);
			Broodwar->drawTextMap(p.x,p.y,"\x07%s",b->getType().getName().c_str());
		} else {
			Broodwar->drawLineMap(p.x,p.y,p.x+(int)velocityX,p.y+(int)velocityY,Colors::Red);
			Broodwar->drawTextMap(p.x,p.y,"\x06%s",b->getType().getName().c_str());
		}
	}
}

void BWRepDump::drawVisibilityData()
{
	for (int x = 0; x < Broodwar->mapWidth(); x++) {
		for (int y = 0; y < Broodwar->mapHeight(); y++) {
			if (Broodwar->isExplored(x, y)) {
				if (Broodwar->isVisible(x, y))
					Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Colors::Green);
				else
					Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Colors::Blue);
			} else {
				Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Colors::Red);
			}
		}
	}
}

void BWRepDump::drawTerrainData()
{
	//we will iterate through all the base locations, and draw their outlines.
	for(const auto& base : BWTA::getBaseLocations()) {
		TilePosition p(base->getTilePosition());
		Position c(base->getPosition());

		//draw outline of center location
		Broodwar->drawBox(CoordinateType::Map,p.x*32,p.y*32,p.x*32+4*32,p.y*32+3*32,Colors::Blue,false);

		//draw a circle at each mineral patch
		for (const auto& mineral : base->getStaticMinerals()) {
			Position q = mineral->getInitialPosition();
			Broodwar->drawCircle(CoordinateType::Map, q.x, q.y, 30, Colors::Cyan, false);
		}

		//draw the outlines of Vespene geysers
		for (const auto& geyser : base->getGeysers()) {
			TilePosition q = geyser->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map,q.x*32,q.y*32,q.x*32+4*32,q.y*32+2*32,Colors::Orange,false);
		}

		//if this is an island expansion, draw a yellow circle around the base location
		if (base->isIsland()) {
			Broodwar->drawCircle(CoordinateType::Map, c.x, c.y, 80, Colors::Yellow, false);
		}
	}

	//we will iterate through all the regions and draw the polygon outline of it in green.
	for (const auto& r : BWTA::getRegions()) {
		BWTA::Polygon p = r->getPolygon();
		for (int j = 0; j<(int)p.size(); j++) {
			Position point1=p[j];
			Position point2=p[(j+1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map,point1.x,point1.y,point2.x,point2.y,Colors::Green);
		}
	}

	//we will visualize the chokepoints with red lines
	for (const auto& r : BWTA::getRegions()) {
		for (const auto& c : r->getChokepoints()) {
			Position point1 = c->getSides().first;
			Position point2 = c->getSides().second;
			Broodwar->drawLine(CoordinateType::Map,point1.x,point1.y,point2.x,point2.y,Colors::Red);
		}
	}
}

void BWRepDump::showPlayers()
{
	for (const auto& p : Broodwar->getPlayers()) {
		Broodwar->printf("Player [%d]: %s is in force: %s", p->getID(), p->getName().c_str(), p->getForce()->getName().c_str());
	}
}

void BWRepDump::showForces()
{
	for (const auto& f : Broodwar->getForces()) {
		Broodwar->printf("Force %s has the following players:", f->getName().c_str());
		for (const auto& p : f->getPlayers()) {
			Broodwar->printf("  - Player [%d]: %s", p->getID(), p->getName().c_str());
		}
	}
}
