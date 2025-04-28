#include "DungeonRespawn.h"

// Global variables
std::vector<PlayerRespawnData> respawnData;
GuidVector playersToTeleport;
bool drEnabled = false;
float respawnHpPct = 50.0f;

bool DSPlayerScript::IsInsideDungeonRaid(Player* player)
{
    if (!player || !player->GetMap())
        return false;

    Map* map = player->GetMap();
    return map->IsDungeon() || map->IsRaid();
}

void DSPlayerScript::OnPlayerReleasedGhost(Player* player)
{
    if (!drEnabled || !IsInsideDungeonRaid(player))
        return;

    playersToTeleport.push_back(player->GetGUID());
}

void DSPlayerScript::ResurrectPlayer(Player* player)
{
    player->ResurrectPlayer(respawnHpPct / 100.0f, false);
    player->SpawnCorpseBones();
}

bool DSPlayerScript::OnPlayerBeforeTeleport(Player* player, uint32 mapid, float, float, float, float, uint32, Unit*)
{
    if (!drEnabled || !player)
        return true;

    if (player->GetMapId() != mapid)
    {
        auto prData = GetOrCreateRespawnData(player);
        prData->isTeleportingNewMap = true;
    }

    if (!IsInsideDungeonRaid(player) || !player->isDead())
        return true;

    auto itToRemove = playersToTeleport.end();
    bool canRestore = false;

    for (auto it = playersToTeleport.begin(); it != playersToTeleport.end(); ++it)
    {
        if (*it == player->GetGUID())
        {
            itToRemove = it;
            canRestore = true;
            break;
        }
    }

    if (!canRestore)
        return true;

    playersToTeleport.erase(itToRemove);

    auto prData = GetOrCreateRespawnData(player);
    if (prData && prData->dungeon.map != -1 && prData->dungeon.map == int32(player->GetMapId()))
    {
        player->TeleportTo(prData->dungeon.map, prData->dungeon.x, prData->dungeon.y, prData->dungeon.z, prData->dungeon.o);
        ResurrectPlayer(player);
        return false;
    }

    return true;
}

void DSPlayerScript::OnPlayerLogin(Player* player)
{
    if (player)
        GetOrCreateRespawnData(player);
}

void DSPlayerScript::OnPlayerLogout(Player* player)
{
    if (!player)
        return;

    auto guid = player->GetGUID();
    playersToTeleport.erase(
        std::remove(playersToTeleport.begin(), playersToTeleport.end(), guid),
        playersToTeleport.end()
    );
}

void DSPlayerScript::OnMapChanged(Player* player)
{
    if (!player)
        return;

    auto prData = GetOrCreateRespawnData(player);
    if (!prData)
        return;

    bool inDungeon = IsInsideDungeonRaid(player);
    prData->inDungeon = inDungeon;

    if (!inDungeon || !prData->isTeleportingNewMap)
        return;

    prData->dungeon.map = player->GetMapId();
    prData->dungeon.x = player->GetPositionX();
    prData->dungeon.y = player->GetPositionY();
    prData->dungeon.z = player->GetPositionZ();
    prData->dungeon.o = player->GetOrientation();
    prData->isTeleportingNewMap = false;
}

void DSPlayerScript::CreateRespawnData(Player* player)
{
    DungeonData dData { -1, 0, 0, 0, 0 };
    PlayerRespawnData prData { player->GetGUID(), dData, false, false };
    respawnData.push_back(prData);
}

PlayerRespawnData* DSPlayerScript::GetOrCreateRespawnData(Player* player)
{
    for (auto& data : respawnData)
    {
        if (data.guid == player->GetGUID())
            return &data;
    }

    CreateRespawnData(player);
    return GetOrCreateRespawnData(player);
}

void DSWorldScript::OnAfterConfigLoad(bool reload)
{
    if (reload)
    {
        SaveRespawnData();
        respawnData.clear();
    }

    drEnabled = sConfigMgr->GetOption<bool>("DungeonRespawn.Enable", false);
    respawnHpPct = sConfigMgr->GetOption<float>("DungeonRespawn.RespawnHealthPct", 50.0f);

    QueryResult qResult = CharacterDatabase.Query("SELECT `guid`, `map`, `x`, `y`, `z`, `o` FROM `dungeonrespawn_playerinfo`");
    if (!qResult)
    {
        LOG_INFO("module", "Loaded '0' rows from 'dungeonrespawn_playerinfo' table.");
        return;
    }

    uint32 dataCount = 0;
    do
    {
        Field* fields = qResult->Fetch();

        PlayerRespawnData prData;
        DungeonData dData;
        prData.guid = ObjectGuid(fields[0].Get<uint64>());
        dData.map = fields[1].Get<int32>();
        dData.x = fields[2].Get<float>();
        dData.y = fields[3].Get<float>();
        dData.z = fields[4].Get<float>();
        dData.o = fields[5].Get<float>();
        prData.dungeon = dData;
        prData.isTeleportingNewMap = false;
        prData.inDungeon = false;

        respawnData.push_back(prData);
        dataCount++;

    } while (qResult->NextRow());

    LOG_INFO("module", "Loaded '{}' rows from 'dungeonrespawn_playerinfo' table.", dataCount);
}

void DSWorldScript::OnShutdown()
{
    SaveRespawnData();
}

void DSWorldScript::SaveRespawnData()
{
    for (const auto& prData : respawnData)
    {
        if (prData.inDungeon)
        {
            CharacterDatabase.Execute(R"(
                INSERT INTO `dungeonrespawn_playerinfo` (guid, map, x, y, z, o)
                VALUES ({}, {}, {}, {}, {}, {})
                ON DUPLICATE KEY UPDATE map={}, x={}, y={}, z={}, o={}
            )",
                prData.guid.GetRawValue(),
                prData.dungeon.map,
                prData.dungeon.x,
                prData.dungeon.y,
                prData.dungeon.z,
                prData.dungeon.o,
                prData.dungeon.map,
                prData.dungeon.x,
                prData.dungeon.y,
                prData.dungeon.z,
                prData.dungeon.o);
        }
        else
        {
            CharacterDatabase.Execute("DELETE FROM `dungeonrespawn_playerinfo` WHERE guid = {}", prData.guid.GetRawValue());
        }
    }
}

void SC_AddDungeonRespawnScripts()
{
    new DSWorldScript();
    new DSPlayerScript();
}
