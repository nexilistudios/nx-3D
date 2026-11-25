#ifndef NX3D_SERVER_ROOM_EVENT_HH
#define NX3D_SERVER_ROOM_EVENT_HH

#include <nexilis/command_type.hh>
#include <nexilis/server/command/command.hh>

#include <boost/json/value.hpp>

#include <cstdint>
#include <string_view>

namespace nx3d::server
{

inline nexilis::nx_data roomEvent(uint64_t roomId, std::string_view event,
                                  std::string_view payload)
{
    return nexilis::server::Command::clientMessageData(
            nexilis::CommandType::room, "server_event", 0,
            {{"action", "emit"}, {"room_id", roomId}, {"event", boost::json::value(event)}, {"payload", boost::json::value(payload)}});
}

} // namespace nx3d::server

#endif
