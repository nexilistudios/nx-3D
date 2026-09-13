#ifndef NX3D_SHARED_GAMEMODE_HH
#define NX3D_SHARED_GAMEMODE_HH

#include <algorithm>
#include <cctype>
#include <string>

namespace nx3d
{

/// The type of game a room runs.
///
/// This is a game-level (nx_3d) concept only; the networking library is
/// deliberately unaware of it. The server picks a gamemode for each of its
/// rooms, and the client derives the matching gamemode from the room it joins.
/// There is no default: event a room that does not declare a mode still has to
/// be given one explicitly, so a room can never silently run the wrong mode.
enum class GameMode
{
    /// Immediate automatic respawn on death (free-for-all / team deathmatch).
    deathmatch,

    /// No automatic respawns; players stay dead until something revives them.
    nospawn,

    /// Objective mode where kills alone are not enough to win.
    hardpoint,

    /// Everything custom that a game integrates through its own logic.
    custom
};

/// String form of a gamemode, e.g. for logging and room names.
inline const char* gameModeToString(GameMode mode)
{
    switch (mode)
    {
        case GameMode::deathmatch:
            return "deathmatch";
        case GameMode::nospawn:
            return "nospawn";
        case GameMode::hardpoint:
            return "hardpoint";
        case GameMode::custom:
            return "custom";
    }
    return "custom";
}

/// Map a room name to a gamemode.
///
/// Rooms carry no gamemode over the wire (nexilis does not know about game
/// modes), so the client recognises a room's gamemode by its name. The naming
/// convention is: a room whose name mentions a known gamemode runs that mode;
/// anything else defaults to deathmatch.
inline GameMode gameModeFromName(const std::string& roomName)
{
    std::string lower = roomName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });

    if (lower.find("nospawn") != std::string::npos)
        return GameMode::nospawn;
    if (lower.find("hardpoint") != std::string::npos)
        return GameMode::hardpoint;
    if (lower.find("custom") != std::string::npos)
        return GameMode::custom;
    return GameMode::deathmatch;
}

} // namespace nx3d

#endif