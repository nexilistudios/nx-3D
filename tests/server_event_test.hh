#ifndef NX3D_TESTS_SERVER_EVENT_TEST_HH
#define NX3D_TESTS_SERVER_EVENT_TEST_HH

#include <nexilis/client/client_api.hh>

#include <stdexcept>
#include <string>
#include <unordered_map>

inline std::string latestServerEvent(nexilis::client::ClientAPI& api, const std::string& name)
{
    static std::unordered_map<nexilis::client::ClientAPI*,
                              std::unordered_map<std::string, std::string>>
            latest;
    auto& byName = latest[&api];
    for (const auto& event : api.consumeServerRoomEvents())
        if (event.payload.is_string())
            byName[event.event] = std::string(event.payload.as_string());
    if (const auto it = byName.find(name); it != byName.end())
        return it->second;
    throw std::runtime_error("Missing server room event: " + name);
}

#endif
