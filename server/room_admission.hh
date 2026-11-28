#ifndef NX3D_SERVER_ROOM_ADMISSION_HH
#define NX3D_SERVER_ROOM_ADMISSION_HH

#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>

namespace nx3d::server
{

// Nexilis stores authenticated sessions in ClientStorage. Explicitly admit
// each such session to every game room, including rooms created by clients.
inline void admitConnectedClientsToRoom(nexilis::server::Room& room)
{
    for (const auto& client : nexilis::server::ClientStorage::getAllClients())
        room.addAdmittedUser(client->getId());
}

inline void admitConnectedClientsToRooms()
{
    const auto clients = nexilis::server::ClientStorage::getAllClients();
    for (const auto& room : nexilis::server::RoomStorage::getAllRooms())
        for (const auto& client : clients)
            room->addAdmittedUser(client->getId());
}

} // namespace nx3d::server

#endif
