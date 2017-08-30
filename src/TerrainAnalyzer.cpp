#include "TerrainAnalyzer.h"

#define MIN_CDREGION_RADIUS 9

using namespace BWAPI;

int hash(const BWAPI::TilePosition& p)
{
	return (((p.x + 1) << 16) | p.y);
}

int TerrainAnalyzer::hashRegionCenter(BWTA::Region* r)
{
	/// Max size for a map is 512x512 build tiles => 512*32 = 16384 = 2^14 pixels
	/// Unwalkable regions will map to 0
	TilePosition p(r->getPolygon().getCenter());
	return hash(p);
}

BWAPI::Position TerrainAnalyzer::regionsPFCenters(BWTA::Region* r)
{
	int tmp = hashRegionCenter(r);
	return Position(_pfMaps.regionsPFCenters[tmp].first, _pfMaps.regionsPFCenters[tmp].second);
}

BWAPI::TilePosition TerrainAnalyzer::cdrCenter(ChokeDepReg c)
{
	/// /!\ This is will give centers out of the ChokeDepRegions for some coming from BWTA::Region
	return TilePosition(((0xFFFF0000 & c) >> 16) - 1, 0x0000FFFF & c);
}

BWTA::Region* TerrainAnalyzer::findClosestRegion(const TilePosition& tp)
{
	double m = DBL_MAX;
	Position tmp(tp);
	BWTA::Region* ret = BWTA::getRegion(tp);
	for (const auto& r : BWTA::getRegions()) {
		if (r->getCenter().getDistance(tmp) < m) {
			m = r->getCenter().getDistance(tmp);
			ret = r;
		}
	}
	return ret;
}

ChokeDepReg TerrainAnalyzer::findClosestCDR(const TilePosition& tp)
{
	ChokeDepReg ret = regionData.chokeDependantRegion[tp.x][tp.y];
	double m = DBL_MAX;
	for (const auto& cdr : allChokeDepRegs) {
		if (cdrCenter(cdr).getDistance(tp) < m) {
			m = cdrCenter(cdr).getDistance(tp);
			ret = cdr;
		}
	}
	return ret;
}

BWTA::Region* TerrainAnalyzer::findClosestReachableRegion(BWTA::Region* q, BWTA::Region* r)
{
	double m = DBL_MAX;
	BWTA::Region* ret = q;
	for (const auto& rr : BWTA::getRegions()) {
		if (r->getReachableRegions().count(rr)
			&& _pfMaps.distRegions[hashRegionCenter(rr)][hashRegionCenter(q)] < m)
		{
			m = _pfMaps.distRegions[hashRegionCenter(rr)][hashRegionCenter(q)];
			ret = rr;
		}
	}
	return ret;
}

ChokeDepReg TerrainAnalyzer::findClosestReachableCDR(ChokeDepReg q, ChokeDepReg cdr)
{
	double m = DBL_MAX;
	ChokeDepReg ret = q;
	for (const auto& cdrr : allChokeDepRegs) {
		if (_pfMaps.distCDR[cdr][cdrr] && _pfMaps.distCDR[q][cdrr] < m) {
			m = _pfMaps.distCDR[cdrr][q];
			ret = cdrr;
		}
	}
	return ret;
}

TerrainAnalyzer::TerrainAnalyzer()
{
	// Build Tiles resolution
	_lowResWalkability = new bool[Broodwar->mapWidth() * Broodwar->mapHeight()];
	for (int x = 0; x < Broodwar->mapWidth(); ++x) {
		for (int y = 0; y < Broodwar->mapHeight(); ++y) {
			_lowResWalkability[x + y*Broodwar->mapWidth()] = true;
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					_lowResWalkability[x + y*Broodwar->mapWidth()] &= Broodwar->isWalkable(x * 4 + i, y * 4 + j);
				}
			}
		}
	}
	createChokeDependantRegions();

	std::string locationfilepath = Broodwar->mapPathName() + ".rld";
	replayLocationDat.open(locationfilepath);

	// save static region (RLD file)
	std::string tmpColumns("Regions,");
	for (const auto& regDists : _pfMaps.distRegions) {
		tmpColumns += std::to_string(regDists.first) + ",";
	}
	tmpColumns[tmpColumns.size() - 1] = '\n';
	replayLocationDat << tmpColumns;

	for (std::map<int, std::map<int, double> >::const_reverse_iterator it = _pfMaps.distRegions.rbegin();
		it != _pfMaps.distRegions.rend(); ++it)
	{
		replayLocationDat << it->first;
		for (const auto& rD : it->second) {
			if (it->first == rD.first) break; // line == column, symmetrical => useless to write from now on
			replayLocationDat << "," << (int)rD.second;
		}
		replayLocationDat << "\n";
	}
	tmpColumns = "ChokeDepReg,";
	for (const auto& cdrDists : _pfMaps.distCDR) {
		tmpColumns += std::to_string(cdrDists.first) + ",";
	}
	tmpColumns[tmpColumns.size() - 1] = '\n';
	replayLocationDat << tmpColumns;
	for (std::map<int, std::map<int, double> >::const_reverse_iterator it = _pfMaps.distCDR.rbegin();
		it != _pfMaps.distCDR.rend(); ++it)
	{
		replayLocationDat << it->first;
		for (const auto& cdrD : it->second) {
			if (it->first == cdrD.first) break; // line == column, symmetrical => useless to write from now on
			replayLocationDat << "," << (int)cdrD.second;
		}
		replayLocationDat << "\n";
	}
	replayLocationDat << "[Replay Start]\n";
}

TerrainAnalyzer::~TerrainAnalyzer()
{
	replayLocationDat.close();
}

void TerrainAnalyzer::createChokeDependantRegions()
{
	// check if the cache folder exist
	std::tr2::sys::path cachePath("bwapi-data/AI/BWRepDumpCache/");
	if (!std::tr2::sys::exists(cachePath)) {
		std::tr2::sys::create_directory(cachePath);
	}

	std::tr2::sys::path filePath(cachePath);
	filePath /= Broodwar->mapHash() + ".cdreg";
	if (std::tr2::sys::exists(filePath)) {
		// fill our own regions data (rd) with the archived file
		std::ifstream ifs(filePath, std::ios::binary);
		boost::archive::binary_iarchive ia(ifs);
		ia >> regionData;
	} else {
		std::map<BWTA::Chokepoint*, int> maxTiles; // max tiles for each CDRegion
		int k = 1; // 0 is reserved for unwalkable regions
		/// 1. for each region, max radius = max(MIN_CDREGION_RADIUS, choke size)
		for (const auto& c : BWTA::getChokepoints()) {
			maxTiles.insert(std::make_pair(c, std::max(MIN_CDREGION_RADIUS, static_cast<int>(c->getWidth()) / TILE_SIZE)));
		}
		/// 2. Voronoi on both choke regions
		for (int x = 0; x < Broodwar->mapWidth(); ++x) {
			for (int y = 0; y < Broodwar->mapHeight(); ++y) {
				TilePosition tmp(x, y);
				BWTA::Region* r = BWTA::getRegion(tmp);
				double minDist = DBL_MAX - 100.0;
				for (const auto& c : BWTA::getChokepoints()) {
					TilePosition chokeCenter(c->getCenter().x / TILE_SIZE, c->getCenter().y / TILE_SIZE);
					double tmpDist = tmp.getDistance(chokeCenter);
					double pathFindDist = DBL_MAX;
					if (tmpDist < minDist && tmpDist <= 1.0 * maxTiles[c]
						&& (pathFindDist = BWTA::getGroundDistance(tmp, chokeCenter)) < minDist
						&& pathFindDist >= 0.0 // -1 means no way to go there
						&& (c->getRegions().first == r || c->getRegions().second == r))
					{
						minDist = pathFindDist;
						regionData.chokeDependantRegion[x][y] = hash(chokeCenter);
					}
				}
			}
		}
		/// 3. Complete with (amputated) BWTA regions
		for (int x = 0; x < Broodwar->mapWidth(); ++x) {
			for (int y = 0; y < Broodwar->mapHeight(); ++y) {
				TilePosition tmp(x, y);
				if (regionData.chokeDependantRegion[x][y] == -1 && BWTA::getRegion(tmp) != NULL) {
					regionData.chokeDependantRegion[x][y] = hashRegionCenter(BWTA::getRegion(tmp));
				}
			}
			std::ofstream ofs(filePath, std::ios::binary);
			{
				boost::archive::binary_oarchive oa(ofs);
				oa << regionData;
			}
		}
	}
	// initialize allChokeDepRegs
	for (int i = 0; i < Broodwar->mapWidth(); ++i) {
		for (int j = 0; j < Broodwar->mapHeight(); ++j) {
			if (regionData.chokeDependantRegion[i][j] != -1) {
				allChokeDepRegs.insert(regionData.chokeDependantRegion[i][j]);
			}
		}
	}

	filePath.replace_extension("pfdrep");
	if (std::tr2::sys::exists(filePath)) {
		std::ifstream ifs(filePath, std::ios::binary);
		boost::archive::binary_iarchive ia(ifs);
		ia >> _pfMaps;
	} else {
		/// Fill regionsPFCenters (regions pathfinding aware centers, 
		/// min of the sum of the distance to chokes on paths between/to chokes)
		for (const auto& r : BWTA::getRegions()) {
			std::list<Position> chokesCenters;
			for (const auto& c : r->getChokepoints()) {
				chokesCenters.push_back(c->getCenter());
			}
			if (chokesCenters.empty()) {
				_pfMaps.regionsPFCenters.insert(std::make_pair(hashRegionCenter(r), std::make_pair(r->getCenter().x, r->getCenter().y)));
			} else {
				std::list<TilePosition> validTilePositions;
				for (const auto& c1 : chokesCenters) {
					for (const auto& c2 : chokesCenters) {
						if (c1 != c2) {
							std::vector<TilePosition> buffer = BWTA::getShortestPath(TilePosition(c1), TilePosition(c2));
							for (const auto& vp : buffer) {
								validTilePositions.push_back(vp);
							}
						}
					}
				}
				double minDist = DBL_MAX;
				TilePosition centerCandidate = TilePosition(r->getCenter());
				for (const auto& vp : validTilePositions) {
					double tmp = 0.0;
					for (const auto& c : chokesCenters) {
						tmp += BWTA::getGroundDistance(TilePosition(c), vp);
					}
					if (tmp < minDist) {
						minDist = tmp;
						centerCandidate = vp;
					}
				}
				Position tmp(centerCandidate);
				_pfMaps.regionsPFCenters.insert(std::make_pair(hashRegionCenter(r), std::make_pair(tmp.x, tmp.y)));
			}
		}

		/// Fill distRegions with the mean distance between each Regions
		/// -1 if the 2 Regions are not mutually/inter accessible by ground
		for (const auto& r : BWTA::getRegions()) {
			_pfMaps.distRegions.insert(std::make_pair(hashRegionCenter(r), std::map<int, double>()));
			for (const auto& r2 : BWTA::getRegions()) {
				if (_pfMaps.distRegions.count(hashRegionCenter(r2)))
					_pfMaps.distRegions[hashRegionCenter(r)].insert(std::make_pair(hashRegionCenter(r2),
					_pfMaps.distRegions[hashRegionCenter(r2)][hashRegionCenter(r)]));
				else
					_pfMaps.distRegions[hashRegionCenter(r)].insert(std::make_pair(hashRegionCenter(r2),
					BWTA::getGroundDistance(TilePosition(regionsPFCenters(r)), TilePosition(regionsPFCenters(r2)))));
				//BWTA::getGroundDistance(TilePosition((r)->getCenter()), TilePosition((r2)->getCenter()))));
			}
		}

		/// Fill distCDR
		/// -1 if the 2 Regions are not mutually/inter accessible by ground
		for (const auto& cdr : allChokeDepRegs) {
			_pfMaps.distCDR.insert(std::make_pair(cdr, std::map<int, double>()));
			for (const auto& cdr2 : allChokeDepRegs) {
				if (_pfMaps.distCDR.count(cdr2)) {
					_pfMaps.distCDR[cdr].insert(std::make_pair(cdr2, _pfMaps.distCDR[cdr2][cdr]));
				} else {
					BWAPI::TilePosition tmp = cdrCenter(cdr);
					BWAPI::TilePosition tmp2 = cdrCenter(cdr2);
					if (!isWalkable(tmp) || regionData.chokeDependantRegion[tmp.x][tmp.y] != cdr) {
						tmp = findClosestWalkableSameCDR(tmp, cdr);
					}
					if (!isWalkable(tmp2) || regionData.chokeDependantRegion[tmp2.x][tmp2.y] != cdr2) {
						tmp2 = findClosestWalkableSameCDR(tmp2, cdr2);
					}
					_pfMaps.distCDR[cdr].insert(std::make_pair(cdr2, 
						BWTA::getGroundDistance(TilePosition(tmp), TilePosition(tmp2))));
				}
			}
		}

		std::ofstream ofs(filePath, std::ios::binary);
		{
			boost::archive::binary_oarchive oa(ofs);
			oa << _pfMaps;
		}
	}
}

bool TerrainAnalyzer::isWalkable(const TilePosition& tp)
{
	return _lowResWalkability[tp.x + tp.y*Broodwar->mapWidth()];
}

BWAPI::TilePosition TerrainAnalyzer::findClosestWalkableSameCDR(const BWAPI::TilePosition& tp, ChokeDepReg c)
{
	/// Finds the closest-to-"p" walkable position in the given "c"
	double minDist = DBL_MAX;
	BWAPI::TilePosition ret(tp);
	int minWidth = std::min(Broodwar->mapWidth(), tp.x + 4);
	int minHeight = std::min(Broodwar->mapHeight(), tp.y + 4);
	for (int x = std::max(0, tp.x - 4); x < minWidth; ++x) {
		for (int y = std::max(0, tp.y - 4); y < minHeight; ++y) {
			if (regionData.chokeDependantRegion[x][y] != c) continue;
			TilePosition tmp(x, y);
			if (isWalkable(tmp)) {
				double currentDist(tp.getDistance(tmp));
				if (currentDist < minDist) {
					minDist = currentDist;
					ret = tmp;
				}
			}
		}
	}
	if (regionData.chokeDependantRegion[ret.x][ret.y] != c) {
		minWidth = std::min(Broodwar->mapWidth(), tp.x + 10);
		minHeight = std::min(Broodwar->mapHeight(), tp.y + 10);
		for (int x = std::max(0, tp.x - 10); x < minWidth; ++x) {
			for (int y = std::max(0, tp.y - 10); y < minHeight; ++y) {
				if (regionData.chokeDependantRegion[x][y] != c) continue;
				TilePosition tmp(x, y);
				if (isWalkable(tmp)) {
					double currentDist(tp.getDistance(tmp));
					if (currentDist < minDist) {
						minDist = currentDist;
						ret = tmp;
					}
				}
			}
		}
	}

	if (ret == tp) // sometimes centers are in convex subregions
		return findClosestWalkable(tp);
	return ret;
}

BWAPI::TilePosition TerrainAnalyzer::findClosestWalkable(const BWAPI::TilePosition& tp)
{
	double minDist = DBL_MAX;
	BWAPI::TilePosition ret(tp);
	int minWidth = std::min(Broodwar->mapWidth(), tp.x + 4);
	int minHeight = std::min(Broodwar->mapHeight(), tp.y + 4);
	for (int x = std::max(0, tp.x - 4); x < minWidth; ++x) {
		for (int y = std::max(0, tp.y - 4); y < minHeight; ++y) {
			TilePosition tmp(x, y);
			if (isWalkable(tmp)) {
				double currentDist(tp.getDistance(tmp));
				if (currentDist < minDist) {
					minDist = currentDist;
					ret = tmp;
				}
			}
		}
	}
	if (ret == tp) {
		minWidth = std::min(Broodwar->mapWidth(), tp.x + 10);
		minHeight = std::min(Broodwar->mapHeight(), tp.y + 10);
		for (int x = std::max(0, tp.x - 10); x < minWidth; ++x) {
			for (int y = std::max(0, tp.y - 10); y < minHeight; ++y) {
				TilePosition tmp(x, y);
				if (isWalkable(tmp)) {
					double currentDist(tp.getDistance(tmp));
					if (currentDist < minDist) {
						minDist = currentDist;
						ret = tmp;
					}
				}
			}
		}
	}
	return ret;
}

void TerrainAnalyzer::displayChokeDependantRegions()
{
#ifdef __DEBUG_CDR_FULL__
	for (int x = 0; x < Broodwar->mapWidth(); x += 4) {
		for (int y = 0; y < Broodwar->mapHeight(); y += 2) {
			Broodwar->drawTextMap(x*TILE_SIZE + 6, y*TILE_SIZE + 2, "%d", rd.chokeDependantRegion[x][y]);
			if (BWTA::getRegion(TilePosition(x, y)) != NULL)
				Broodwar->drawTextMap(x*TILE_SIZE + 6, y*TILE_SIZE + 10, "%d", hashRegionCenter(BWTA::getRegion(TilePosition(x, y))));
		}
	}
#endif
	int n = 0;
	for (const auto& cdr : allChokeDepRegs) {
		if (cdr != -1) {
			Broodwar->drawCircleMap(cdrCenter(cdr).x*TILE_SIZE + TILE_SIZE / 2, cdrCenter(cdr).y*TILE_SIZE + TILE_SIZE / 2, 10, Colors::Green, true);
			Broodwar->drawTextMap(cdrCenter(cdr).x*TILE_SIZE + TILE_SIZE / 2, cdrCenter(cdr).y*TILE_SIZE + TILE_SIZE / 2, "%d", n++);
		}
	}
// 	for (const auto& c : BWTA::getChokepoints()) {
// 		Broodwar->drawBoxMap(c->getCenter().x - 4, c->getCenter().y - 4, c->getCenter().x + 4, c->getCenter().y + 4, Colors::Brown, true);
// 	}
}

void TerrainAnalyzer::onFrame()
{

#ifdef __DEBUG_CDR_FULL__
	for (int x = 0; x < Broodwar->mapWidth(); ++x) {
		for (int y = 0; y < Broodwar->mapHeight(); ++y) {
			if (!_lowResWalkability[x + y*Broodwar->mapWidth()])
				Broodwar->drawBoxMap(32 * x + 2, 32 * y + 2, 32 * x + 30, 32 * y + 30, Colors::Red);
		}
	}
	for (const auto& cdr : allChokeDepRegs) {
		Position p(cdrCenter(cdr));
		Broodwar->drawBoxMap(p.x + 2, p.y + 2, p.x + 30, p.y + 30, Colors::Blue);
	}
#endif

#ifdef __DEBUG_CDR__
	displayChokeDependantRegions();
	for (const auto& r : BWTA::getRegions()) {
		BWTA::Polygon p = r->getPolygon();
		for (int j = 0; j<(int)p.size(); j++) {
			Position point1 = p[j];
			Position point2 = p[(j + 1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map, point1.x, point1.y, point2.x, point2.y, Colors::Green);
		}
	}
	for (const auto& c : BWTA::getChokepoints()) {
		Broodwar->drawLineMap(c->getSides().first.x, c->getSides().first.y, c->getSides().second.x, c->getSides().second.y, Colors::Red);
	}
	for (const auto& cdrDists : _pfMaps.distCDR) {
		BWAPI::TilePosition tp = cdrCenter(cdrDists.first);
		BWAPI::Position p(tp);
		for (const auto& rD : cdrDists.second) {
			if (rD.second >= 0.0) continue;
			BWAPI::TilePosition tp2 = cdrCenter(rD.first);
			BWAPI::Position p2(tp2);
			Broodwar->drawLineMap(p.x, p.y, p2.x, p2.y, Colors::Blue);
			Broodwar->drawTextMap((p.x + p2.x) / 2, (p.y + p2.y) / 2, "%f", rD.second);
		}
	}
#endif

	for (const auto& u : Broodwar->getAllUnits()) {
		if (!isGatheringResources(u) && (Broodwar->getFrameCount() % LOCATION_REFRESH == 0 || unitDestroyedThisTurn)) {
			if (u->exists() && !(u->getPlayer()->getID() == -1) && !(u->getPlayer()->isNeutral())
				&& u->getType() != BWAPI::UnitTypes::Zerg_Larva
				&& u->getPosition().isValid() && unitPositionMap[u] != u->getPosition())
			{
				Position pos(u->getPosition());
				TilePosition tilePos(u->getTilePosition());
				unitPositionMap[u] = pos;
				replayLocationDat << Broodwar->getFrameCount() << "," << u->getID() << "," << pos.x << "," << pos.y << "\n";
				if (unitCDR[u] != regionData.chokeDependantRegion[tilePos.x][tilePos.y]) {
					ChokeDepReg r = regionData.chokeDependantRegion[tilePos.x][tilePos.y];
					if (r >= 0) {
						unitCDR[u] = r;
						replayLocationDat << Broodwar->getFrameCount() << "," << u->getID() << ",CDR," << r << "\n";
					}
				}
				BWTA::Region* r = BWTA::getRegion(pos);
				if (unitRegion[u] != r) {
					if (r != NULL) {
						unitRegion[u] = r;
						replayLocationDat << Broodwar->getFrameCount() << "," << u->getID() << ",Reg," << hashRegionCenter(r) << "\n";
					}
				}
			}
		}
	}
}

void TerrainAnalyzer::onUnitCreate(const BWAPI::Unit& unit)
{
	Position p = unit->getPosition();
	unitPositionMap[unit] = p;
	unitRegion[unit] = BWTA::getRegion(p);
	TilePosition tp = unit->getTilePosition();
	unitCDR[unit] = regionData.chokeDependantRegion[tp.x][tp.y];
}