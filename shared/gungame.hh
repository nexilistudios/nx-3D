#ifndef NX3D_SHARED_GUNGAME_HH
#define NX3D_SHARED_GUNGAME_HH

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <boost/json.hpp>

namespace nx3d::gungame
{
// A kill advances to the next stage. The kill made with the final knife wins.
inline constexpr std::array<std::string_view, 6> kWeapons{
        "glock", "mp5", "ak47", "m16", "launcher", "knife"};
inline constexpr int kKillsToWin = static_cast<int>(kWeapons.size());
inline constexpr int kRestartSeconds = 15;
inline constexpr std::string_view weaponForKills(int kills)
{
    return kWeapons[static_cast<std::size_t>(std::clamp(kills, 0, kKillsToWin - 1))];
}

struct Standing
{
    uint64_t id = 0;
    int kills = 0;
    std::string username;
};

struct Result
{
    uint64_t winner = 0;
    std::vector<Standing> standings;
};

inline void rankStandings(std::vector<Standing>& standings)
{
    std::sort(standings.begin(), standings.end(),
              [](const Standing& a, const Standing& b)
              {
                  if (a.kills != b.kills) return a.kills > b.kills;
                  return a.id < b.id;
              });
}

inline constexpr std::string_view kResultPrefix = "\x1eNX3D_GUNGAME_RESULT ";
inline constexpr std::string_view kCountdownPrefix = "\x1eNX3D_GUNGAME_COUNTDOWN ";

inline std::string encodeCountdown(int seconds)
{
    return std::string(kCountdownPrefix) + std::to_string(seconds);
}

inline bool decodeCountdown(std::string_view message, int& seconds)
{
    if (!message.starts_with(kCountdownPrefix))
        return false;
    const auto value = message.substr(kCountdownPrefix.size());
    int parsed = -1;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() ||
        parsed < 0 || parsed > kRestartSeconds)
        return false;
    seconds = parsed;
    return true;
}

inline std::string encodeResult(const Result& result)
{
    boost::json::array standings;
    for (const auto& entry : result.standings)
        standings.emplace_back(boost::json::object{{"id", entry.id},
                                                   {"kills", entry.kills},
                                                   {"username", entry.username}});
    return std::string(kResultPrefix) + boost::json::serialize(boost::json::object{
            {"winner", result.winner}, {"standings", std::move(standings)}});
}

inline bool decodeResult(std::string_view message, Result& result)
{
    if (!message.starts_with(kResultPrefix))
        return false;
    boost::system::error_code error;
    const auto value = boost::json::parse(message.substr(kResultPrefix.size()), error);
    if (error || !value.is_object())
        return false;
    try
    {
        auto unsignedNumber = [](const boost::json::value& number) -> std::optional<uint64_t>
        {
            if (number.is_uint64())
                return number.as_uint64();
            if (number.is_int64() && number.as_int64() >= 0)
                return static_cast<uint64_t>(number.as_int64());
            return std::nullopt;
        };
        const auto& object = value.as_object();
        const auto& rows = object.at("standings").as_array();
        if (rows.empty() || rows.size() > 64)
            return false;
        Result next;
        const auto winner = unsignedNumber(object.at("winner"));
        if (!winner || *winner == 0)
            return false;
        next.winner = *winner;
        for (const auto& row : rows)
        {
            const auto& entry = row.as_object();
            const auto id = unsignedNumber(entry.at("id"));
            const auto kills = unsignedNumber(entry.at("kills"));
            if (!id || *id == 0 || !kills || *kills > kKillsToWin)
                return false;
            next.standings.push_back({*id,
                                      static_cast<int>(*kills),
                                      std::string(entry.at("username").as_string())});
        }
        result = std::move(next);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
} // namespace nx3d::gungame

#endif
