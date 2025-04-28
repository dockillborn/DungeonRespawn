#ifndef MODULE_DUNGEONRESPAWN_H
#define MODULE_DUNGEONRESPAWN_H

#include "ScriptMgr.h"
#include "LFGMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include <vector>

struct DungeonData
{
    int32 map;
    float x;
    float y;
    float z;
    float o;
};

struct PlayerRespawnData
{
    ObjectGuid guid;
    DungeonData dungeon;
    bool isTeleportingNewMap;
    bool inDungeon;
};

using GuidVector = std::vector<ObjectGuid>;

extern std::vector<PlayerRespawnData> respawnData;
extern GuidVector playersToTeleport;

extern bool drEnabled;
extern float respawnHpPct;

class DSPlayerScript : public PlayerScript
{
public:
    DSPlayerScript() : PlayerScript("DSPlayerScript") { }

    void OnPlayerReleasedGhost(Player* player) override;
    bool OnPlayerBeforeTeleport(Player* player, uint32 mapid, float x, float y, float z, float orientation, uint32 options, Unit* target) override;
    void OnPlayerLogin(Player* player) override;
    void OnPlayerLogout(Player* player) override;

private:
    bool IsInsideDungeonRaid(Player* player);
    void ResurrectPlayer(Player* player);
    PlayerRespawnData* GetOrCreateRespawnData(Player* player);
    void CreateRespawnData(Player* player);
    void OnMapChanged(Player* player);
};

class DSWorldScript : public WorldScript
{
public:
    DSWorldScript() : WorldScript("DSWorldScript") { }

    void OnAfterConfigLoad(bool reload) override;
    void OnShutdown() override;

private:
    void SaveRespawnData();
};

#endif // MODULE_DUNGEONRESPAWN_H
