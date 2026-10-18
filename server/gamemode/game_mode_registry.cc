#include <server/gamemode/game_mode_registry.hh>

#include <server/gamemode/authority.hh>
#include <server/gamemode/bomb_mode.hh>
#include <server/gamemode/deathmatch_mode.hh>
#include <shared/ammunition.hh>
#include <shared/economy.hh>
#include <shared/fall_death.hh>

#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/object/game_item.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/util.hh>

#include <boost/json/value.hpp>

#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

namespace nx3d::server::gamemode
{

std::unordered_map<uint64_t, std::unique_ptr<GameMode>> GameModeRegistry::m_modes;
std::unordered_map<uint64_t, std::unique_ptr<Authority>> GameModeRegistry::m_authorities;
std::unordered_map<uint64_t, std::unordered_set<uint64_t>> GameModeRegistry::m_fallenPlayers;

namespace
{
std::unique_ptr<GameMode> createGameMode(nx3d::GameMode gameMode)
{
    switch (gameMode)
    {
        case nx3d::GameMode::deathmatch:
            return std::make_unique<DeathmatchMode>();
        case nx3d::GameMode::bomb:
            return std::make_unique<BombMode>();

        // Not implemented yet. Fall back to deathmatch so a room requesting
        // one of these still runs (with automatic respawns) until the mode is
        // actually built.
        case nx3d::GameMode::nospawn:
        case nx3d::GameMode::hardpoint:
        case nx3d::GameMode::custom:
            nexilis::Log::warning("Gamemode not implemented, falling back to deathmatch");
            return std::make_unique<DeathmatchMode>();
    }
    return std::make_unique<DeathmatchMode>();
}

} // namespace

void GameModeRegistry::assignRoom(nexilis::server::Room& room, nx3d::GameMode gameMode)
{
    m_fallenPlayers.erase(room.getId());
    auto mode = createGameMode(gameMode);
    auto* modePtr = mode.get();
    auto authority = std::make_unique<Authority>();
    auto* authorityPtr = authority.get();
    const auto roomId = room.getId();

    room.setClientPositionAllowed(false);
    room.setClientGameItemCreationAllowed(false);
    auto pickupMutex = std::make_shared<std::mutex>();
    room.setGameItemPickupHandler([authorityPtr, modePtr, pickupMutex](nexilis::server::Room& r, uint64_t id, uint64_t itemId)
                                  {
        std::lock_guard lock(*pickupMutex);
        auto* user = nexilis::server::ClientStorage::getClientById(id);
        auto* item = r.getGameItemById(itemId);
        if (!user || user->getRoomId() != r.getId() || !item ||
            item->getStatus() != "on_ground")
            return false;
        const auto pos = user->getObject3D().getPosition();
        const auto itemPos = item->getPosition();
        const float dx = pos.x - itemPos.x, dy = pos.y - itemPos.y, dz = pos.z - itemPos.z;
        if (dx * dx + dy * dy + dz * dz >= 10000.0f)
            return false;
        const auto type = std::string(nx3d::ammunition::pickupWeaponType(item->getType()));
        const auto* object = nx3d::findNXObject(type);
        if (!object || (object->kind != nx3d::NXObjectKind::firearm &&
                        object->kind != nx3d::NXObjectKind::bomb))
            return false;
        const auto slot = object->slot;
        int magazine = 0, reserve = 0;
        if ((object->kind == nx3d::NXObjectKind::firearm &&
             (!nx3d::ammunition::decodePickup(item->getType(), magazine, reserve) ||
              magazine > object->combat.magazineSize ||
              reserve > object->combat.magazineSize * object->combat.reserveMagazines)) ||
            !modePtr->pickupWeapon(r, id, slot, type))
            return false;
        if (object->kind == nx3d::NXObjectKind::bomb)
            authorityPtr->equipWeapon(r, id, type);
        else
            authorityPtr->pickupWeapon(id, slot, {type, magazine, reserve});
        r.deleteGameItem(itemId);
        return true; });
    room.setSpawnHandler([authorityPtr, modePtr](nexilis::server::Room& r, uint64_t id)
                         { authorityPtr->spawn(r, id, modePtr->preserveEquipmentOnSpawn(id)); });
    room.setMovementInputHandler([authorityPtr, roomId](uint64_t id,
                                                        const nexilis::Vector3f& input,
                                                        float yaw)
                                 {
        auto* r = nexilis::server::RoomStorage::getRoomById(roomId);
        return r && authorityPtr->move(*r, id, input, yaw); });
    room.setShotHandler([authorityPtr, modePtr, roomId](uint64_t id,
                                                        const nexilis::Vector3f& direction,
                                                        uint8_t slot, uint8_t action)
                        {
        auto* r = nexilis::server::RoomStorage::getRoomById(roomId);
        return r ? authorityPtr->fire(*r, id, direction, slot, action,
                                      modePtr->weaponForSlot(id, slot))
                 : std::optional<nexilis::server::Room::ShotHit>{}; });

    room.setDeathHandler([modePtr](nexilis::server::Room& r, uint64_t killerId, uint64_t victimId)
                         { modePtr->onPlayerDeath(r, killerId, victimId); });
    room.setTeamHandler([modePtr](nexilis::server::Room& r, uint64_t playerId)
                        { modePtr->onTeamSelected(r, playerId); });
    room.setActionHandler([modePtr, authorityPtr, pickupMutex](nexilis::server::Room& r,
                                                               uint64_t playerId, uint8_t action)
                          {
        if (action >= nx3d::economy::kEquipPrimaryAction &&
            action <= nx3d::economy::kEquipDefuseKitAction)
        {
            const uint8_t slot = action - nx3d::economy::kEquipPrimaryAction + 1;
            const std::string weapon = modePtr->weaponForSlot(playerId, slot);
            return !weapon.empty() && authorityPtr->equipWeapon(r, playerId, weapon);
        }
        const uint8_t slot = action == nx3d::economy::kDropPrimaryAction ? 1 :
                             action == nx3d::economy::kDropSecondaryAction ? 2 : 0;
        const auto weapon = slot ? modePtr->weaponForSlot(playerId, slot) : std::string{};
        if (!modePtr->onAction(r, playerId, action))
            return false;
        if (const auto* item = nx3d::economy::itemForAction(action);
            item && item->kind == nx3d::economy::ItemKind::ammo)
            authorityPtr->refill(playerId);
        if (action == nx3d::economy::kDropBombAction)
        {
            auto selected = modePtr->weaponForSlot(playerId, 1);
            if (selected.empty()) selected = modePtr->weaponForSlot(playerId, 2);
            if (!selected.empty()) authorityPtr->equipWeapon(r, playerId, selected);
        }
        if (!slot)
            return true;
        auto* user = nexilis::server::ClientStorage::getClientById(playerId);
        if (!user || weapon.empty())
            return false;
        const auto dropped = authorityPtr->dropWeapon(playerId, slot, weapon);
        if (dropped.type.empty())
            return false;
        const auto playerPos = user->getObject3D().getPosition();
        const float yaw = user->getObject3D().getYaw();
        const nexilis::Vector3f pos{playerPos.x - std::sin(yaw) * 25.0f,
                                    playerPos.y - 10.0f,
                                    playerPos.z - std::cos(yaw) * 25.0f};
        const nexilis::Vector3f dimensions{1.0f, 1.0f, 1.0f};
        const auto type = nx3d::ammunition::encodePickup(
                dropped.type, dropped.magazine, dropped.reserve);
        const auto itemId = nexilis::Util::getRandomUint64();
        {
            std::lock_guard lock(*pickupMutex);
            r.addGameItem(nexilis::GameItem(itemId, type, pos, dimensions, "on_ground"));
        }
        const nexilis::nx_data data{
                static_cast<uint8_t>(nexilis::CommandType::room),
                static_cast<uint8_t>(nexilis::RoomCommandType::Root::game_item),
                static_cast<uint8_t>(nexilis::RoomCommandType::GameItemAction::create)};
        r.broadcastToAll(nexilis::server::Command::createRoomCommand(
                r.getId(), *user, data,
                {{"id", itemId}, {"x", pos.x}, {"y", pos.y}, {"z", pos.z},
                 {"w", dimensions.x}, {"h", dimensions.y}, {"d", dimensions.z},
                 {"item_type", boost::json::value(type)},
                 {"status", boost::json::value("on_ground")},
                 {"filepath", boost::json::value("")}}, 0));
        return true; });
    room.setCombatHandler([modePtr](uint64_t shooterId, uint64_t targetId)
                          { return modePtr->canDamage(shooterId, targetId); });
    room.setTeamSelectionHandler([modePtr](uint64_t playerId)
                                 { return modePtr->canSelectTeam(playerId); });

    m_modes[roomId] = std::move(mode);
    m_authorities[roomId] = std::move(authority);
}

void GameModeRegistry::update(float dt)
{
    for (auto& [roomId, mode] : m_modes)
    {
        if (auto* room = nexilis::server::RoomStorage::getRoomById(roomId))
        {
            m_authorities[roomId]->update(*room, dt);
            auto& fallen = m_fallenPlayers[roomId];
            for (auto playerId : room->getClients())
            {
                auto* player = nexilis::server::ClientStorage::getClientById(playerId);
                if (!player)
                    continue;
                if (player->getObject3D().getPosition().y >= nx3d::kFallDeathY)
                {
                    fallen.erase(playerId);
                    continue;
                }
                if (room->getPlayerTeam(playerId).empty() ||
                    room->getPlayerHealth(playerId) <= 0.0f || !fallen.insert(playerId).second)
                    continue;

                const float health = room->getPlayerHealth(playerId);
                if (!room->damagePlayer(playerId, health))
                    continue;

                // Use the usual damage event so the victim's HUD updates in
                // modes that leave dead players waiting for a new round.
                nexilis::nx_data damageData{
                        static_cast<uint8_t>(nexilis::CommandType::room),
                        static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
                        static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::shoot)};
                std::map<std::string, boost::json::value> params{
                        {"target_id", boost::json::value(playerId)},
                        {"damage", boost::json::value(static_cast<double>(health))},
                        {"new_health", boost::json::value(0.0)}};
                room->broadcastToAll(nexilis::server::Command::createRoomCommand(
                        roomId, *player, damageData, params, 0));
                room->onPlayerDied(0, playerId);
            }
            mode->update(*room, dt);
            m_authorities[roomId]->reconcile(*room);
        }
    }
}

void GameModeRegistry::clear()
{
    m_modes.clear();
    m_authorities.clear();
    m_fallenPlayers.clear();
}

} // namespace nx3d::server::gamemode
