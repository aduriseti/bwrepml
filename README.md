# Description
BWRepDump is a tool to extract a wide variety of data from **StarCraft: Brood War** replays. It's a [BWAPI](https://github.com/bwapi/bwapi/)  module to be injected in StarCraft replays using Chaoslauncher. For now, from a given replay it produces the following files: Replay Game Data (RGD), Replay Order Data (ROD), Replay Location Data (RLD) and Replay Combat Data (RCD). Notice that BWRepDump is a fork of ([bwrepdump](https://github.com/SnippyHolloW/bwrepdump))

----

# Requirements
* [StarCraft: Brood War](https://us.battle.net/shop/en/product/starcraft)
* [BWAPI 4.1.2](https://github.com/bwapi/bwapi/releases/download/v4.1.2/BWAPI_412_Setup.exe)
* To run it
    * [BWTA2 windows libraries](https://bitbucket.org/auriarte/bwta2/downloads/BWTAlib_2.2.7z)
    * [BWRepDump DLL](https://bitbucket.org/auriarte/bwrepdump/downloads/BWRepDump2.0_BWAPI4.1.2.7z)
* For developers
    * MS Visual C++ 2013
    * [BWTA2](https://bitbucket.org/auriarte/bwta2)
    * [Boost 1.56.0](https://sourceforge.net/projects/boost/files/boost-binaries/1.56.0)
    * BWRepDump code from this repository (remember to define the following environment variables: `BWAPI_DIR`, `BWTA_DIR`, `BOOST_DIR`)


# How to run
* Install StarCraft and BWAPI.
* Move BWTA2 windows DLLs to your Windows folder.
* Move BWRepDump DLL to your BWAPI AI folder (usually at `c:\StarCraft\bwapi-data\AI\`).
* Get some replays (for example you can use this [scripts](https://github.com/SnippyHolloW/Broodwar_replays_scrappers)).
* Configure BWAPI to execute BWRepDump through all the replays by editing the config file `bwapi.ini` (usually at `c:\StarCraft\bwapi-data\`).
~~~~
ai = bwapi-data\AI\BWRepDump.dll
auto_menu = SINGLE_PLAYER
auto_restart = ON
map = maps\replays\some_folder\*.rep
mapiteration = SEQUENCE
~~~~
* Use ChaosLauncher (it's already installed with BWAPI) to inject BWAPI in StarCraft, in **Release** mode (Debug will not be able to deal with unanalyzed/unserialized maps).



# Data Extracted From One Replay
Data is partly redundant, to make the analysis easier.

## RGD file
~~~~
[Replay Start]
RepPath: $replayPath
MapName: $mapName
NumStartPositions: $n
The following players are in this replay:
{$playerID, $playerName, $startLoc}
Begin replay data:
{$action}
[EndGame]
~~~~
        
Action can be:
~~~~
$frame,$playerID,Created,$unitId,$unitType,($posX,$posY) 
$frame,$playerID,Destroyed,$unitId,$unitType,($posX,$posY)  
$frame,$playerID,Discovered,$unitId,$unitType  
$frame,$playerID,R,$minerals,$gas,$gatheredMinerals,$gatheredGas,$supplyUsed,$supplyTotal  
$frame,$playerID,ChangedOwnership,$unitID  
$frame,$playerID,Morph,$unitID,$unitType,($posX,$posY)
$frame,$playerID,StartResearch,$researchType
$frame,$playerID,FinishResearch,$researchType
$frame,$playerID,CancelResearch,$researchType
$frame,$playerID,StartUpgrade,$upgradeType,%upgradeLevel
$frame,$playerID,FinishUpgrade,$upgradeType,%upgradeLevel
$frame,$playerID,CancelUpgrade,$upgradeType,%upgradeLevel
$frame,$playerID,SendMessage,$messageText
$frame,$playerID,PlayerLeftGame
$frame,$playerID,NuclearLaunch,($posX,$posY)
$firstFrame,$playerDefenderID,IsAttacked,($attackType),($initPosX,$initPosY),$initCDR,$initRegion,{$playerID:{$unitType:$maxNumberInvolved}},  
	($scoreGroundCDR,$scoreGroundRegion,$scoreAirCDR,$scoreAirRegion,$scoreDetectCDR,$scoreDetectRegion,
	$ecoImportanceCDR,$ecoImportanceRegion,$tactImportanceCDR,$tactImportanceRegion),  
	{$playerID:{$unitType:$numberAtEnd}},($lastPosX,$lastPosY),{$playerID:$numWorkersDead},$lastFrame,$winnerID(OPTIONAL)  
~~~~

$attackType are in {DropAttack, GroundAttack, AirAttack, InvisAttack, UnknownAttackError}.  

[$tactImportance](https://github.com/SnippyHolloW/bwrepdump/blob/master/BWRepDump.cpp#L700) and [$ecoImportance](https://github.com/SnippyHolloW/bwrepdump/blob/master/BWRepDump.cpp#L666) are from in-game heuristics.  

## ROD file
~~~~
{$frame,$unitID,$orderName,[T|P],$posX,$posY}
~~~~
*T* means **unit target** position, *P* means order position  

## RLD file
~~~~
Regions,{$regionID}
{$regionID,{$distToRegion}}
ChokeDepReg,{$CDR_ID}
{$CDR_ID,{$distToCDR}}
[Replay Start]
{$frame,$unitID,$posX,$posY}
{$frame,$unitID,Reg,$regionID}
{$frame,$unitID,CDR,$CDR_ID}
~~~~

With new lines uniquely when the unit moved (of Position and/or Region and/or ChokeDepReg) in the last refresh rate frames (100 atm).

## RCD file
Replay Combat Data, detects and track combats during the replay
~~~~
$replayPath,$replayHash
{NEW_COMBAT,$startFrame,$endFrame,$reasonToEnd
{ARMY_UPGRADES $playerID, {$upgradeName:$upgradeLevel}}
{ARMY_TECHS $playerID, {$techName}}
{ARMY_START $playerID
{$unitID,$UnitName,$initilPosX,$initialPosY,$initialHP,$initialShield,$initialEnergy}}
{ARMY_END $playerID
{$unitID,$UnitName,$finalPosX,$finalPosY,$finalHP,$finalShield,$finalEnergy}}
KILLS
{$unitIdKilled,$frameKilled}
UNITS_NOT_PARTICIPATED
{$unitId}}
~~~~
$reasonToEnd can be: GAME_END, REINFORCEMENT $unitID, ARMY_DESTROYED, PEACE

# Regions
## Serialization
To serialize, we [hash](https://github.com/SnippyHolloW/bwrepdump/blob/master/BWRepDump.cpp#L40-43) BWTA's regions and ChokeDepReg regions on their TilePosition center.

## ChokeDepReg: Choke dependant regions
ChokeDepReg are regions created from the center of chokes to MAX(MIN\_CDR\_RADIUS(currently 9), CHOKE\_WIDTH) build tiles (TilePositions) away, in a Voronoi tiling fashion. Once that is done, ChokeDepRegs are completed with BWTA::Regions minus existing ChokeDepRegs.


# Tuning
[You can tune these defines.](https://github.com/SnippyHolloW/bwrepdump/blob/master/BWRepDump.cpp#L7-14)