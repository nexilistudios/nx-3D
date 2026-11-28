#include <server/room_admission.hh>

#include <nexilis/room_data.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>

#include <memory>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    using namespace nexilis;
    using namespace nexilis::server;

    require(RoomStorage::add(Room(RoomData(0, "server room", 100))),
            "Server room could not be created");
    auto first = std::make_shared<User>(10, "127.0.0.1");
    auto second = std::make_shared<User>(20, "127.0.0.2");
    require(ClientStorage::add(first) && ClientStorage::add(second),
            "Test clients could not be registered");
    auto serverRoom = RoomStorage::getRoomById(100);
    require(!serverRoom->allowsUser(*first) && !serverRoom->allowsUser(*second),
            "Nexilis room unexpectedly admitted clients by default");

    nx3d::server::admitConnectedClientsToRooms();
    require(serverRoom->allowsUser(*first) && serverRoom->allowsUser(*second),
            "Existing server room did not admit connected clients");

    RoomStorage::setCreateHandler([](Room& room)
                                  { nx3d::server::admitConnectedClientsToRoom(room); });
    require(RoomStorage::add(Room(RoomData(10, "client room", 200))),
            "Client room could not be created");
    auto clientRoom = RoomStorage::getRoomById(200);
    require(clientRoom->allowsUser(*second),
            "New client room did not admit another connected client");

    auto third = std::make_shared<User>(30, "127.0.0.3");
    require(ClientStorage::add(third), "Late client could not be registered");
    require(!serverRoom->allowsUser(*third) && !clientRoom->allowsUser(*third),
            "Late client was admitted before synchronization");
    nx3d::server::admitConnectedClientsToRooms();
    require(serverRoom->allowsUser(*third) && clientRoom->allowsUser(*third),
            "Late client was not admitted to both rooms");

    RoomStorage::setCreateHandler({});
    RoomStorage::clear();
    ClientStorage::clear();
}
