#pragma once

#include <fstream>
#include <filesystem>

#include "boost/archive/binary_oarchive.hpp"
#include "boost/archive/binary_iarchive.hpp"
#include "boost/serialization/map.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/serialization/utility.hpp"

#include <BWAPI.h>
#include <BWTA.h>

#include "Utils.h"

typedef int ChokeDepReg;

struct PathAwareMaps
{
	friend class boost::serialization::access;
	template <class archive>
	void serialize(archive & ar, const unsigned int version)
	{
		ar & regionsPFCenters;
		ar & distRegions;
		ar & distCDR;
	}
	std::map<int, std::pair<int, int> > regionsPFCenters; // Pathfinding wise region centers
	std::map<int, std::map<int, double> > distRegions; // distRegions[R1][R2] w.r.t regionsPFCenters
	std::map<int, std::map<int, double> > distCDR;
};
BOOST_CLASS_TRACKING(PathAwareMaps, boost::serialization::track_never);
BOOST_CLASS_VERSION(PathAwareMaps, 1);

struct RegionsData
{
	friend class boost::serialization::access;
	template <class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & chokeDependantRegion;
	}
	// -1 -> unwalkable regions
	std::vector<std::vector<ChokeDepReg> > chokeDependantRegion;
	RegionsData()
		: chokeDependantRegion(std::vector<std::vector<ChokeDepReg> >(BWAPI::Broodwar->mapWidth(), std::vector<ChokeDepReg>(BWAPI::Broodwar->mapHeight(), -1)))
	{}
	RegionsData(const std::vector<std::vector<ChokeDepReg> >& cdr)
		: chokeDependantRegion(cdr)
	{}
};
BOOST_CLASS_TRACKING(RegionsData, boost::serialization::track_never);
BOOST_CLASS_VERSION(RegionsData, 2);

// Neither Region* (of course) nor the ordering in the Regions set is
// deterministic, so we have a map which maps Region* to a unique int
// which is region's center (0)<x Position + 1><y Position>
// on                               16 bits      16 bits

class TerrainAnalyzer
{
public:
	RegionsData regionData;
	PathAwareMaps _pfMaps;
	std::set<ChokeDepReg> allChokeDepRegs;

	TerrainAnalyzer(); // Generates RLD file
	~TerrainAnalyzer();
	void onFrame();
	void onUnitCreate(const BWAPI::Unit& unit);

	bool isWalkable(const BWAPI::TilePosition& tp);
	BWAPI::TilePosition findClosestWalkable(const BWAPI::TilePosition& tp);
	BWAPI::TilePosition findClosestWalkableSameCDR(const BWAPI::TilePosition& p, ChokeDepReg c);
	BWTA::Region* findClosestRegion(const BWAPI::TilePosition& tp);
	ChokeDepReg findClosestCDR(const BWAPI::TilePosition& tp);
	BWTA::Region* findClosestReachableRegion(BWTA::Region* q, BWTA::Region* r);
	ChokeDepReg findClosestReachableCDR(ChokeDepReg q, ChokeDepReg cdr);
	int hashRegionCenter(BWTA::Region* r);

private:
	std::ofstream replayLocationDat;
	std::ofstream replayOrdersDat;

	std::map<BWAPI::Unit, BWAPI::Position> unitPositionMap;
	std::map<BWAPI::Unit, ChokeDepReg> unitCDR;
	std::map<BWAPI::Unit, BWTA::Region*> unitRegion;

	bool* _lowResWalkability;

	void createChokeDependantRegions();
	BWAPI::Position regionsPFCenters(BWTA::Region* r);
	BWAPI::TilePosition cdrCenter(ChokeDepReg c);
	void displayChokeDependantRegions();
};