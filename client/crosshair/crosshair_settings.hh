#ifndef NX3D_CLIENT_CROSSHAIR_SETTINGS_HH
#define NX3D_CLIENT_CROSSHAIR_SETTINGS_HH

#include <algorithm>
#include <array>
#include <cmath>
#include <istream>
#include <ostream>
#include <string>

namespace nx3d::client
{
struct CrosshairSettings
{
    bool dynamic = false;
    float length = 3.0f, thickness = 0.8f, gap = 2.0f;
    bool dot = false, tStyle = false, outline = true;
    float outlineThickness = 1.0f;
    int red = 50, green = 250, blue = 50, alpha = 255;

    bool operator==(const CrosshairSettings&) const = default;
    void sanitize()
    {
        auto limit = [](float value, float low, float high, float fallback)
        { return std::isfinite(value) ? std::clamp(value, low, high) : fallback; };
        length = limit(length, 0.0f, 10.0f, 3.0f);
        thickness = limit(thickness, 0.1f, 5.0f, 0.8f);
        gap = limit(gap, -5.0f, 20.0f, 2.0f);
        outlineThickness = limit(outlineThickness, 0.1f, 3.0f, 1.0f);
        red = std::clamp(red, 0, 255);
        green = std::clamp(green, 0, 255);
        blue = std::clamp(blue, 0, 255);
        alpha = std::clamp(alpha, 0, 255);
    }
};

inline constexpr std::array<const char*, 5> kCrosshairPresets{
        "Classic", "Compact", "Dot", "T-style", "Dynamic"};
inline CrosshairSettings crosshairPreset(int index)
{
    CrosshairSettings result;
    switch (index)
    {
    case 1: result.length = 2; result.gap = 0; result.red = 0; result.green = 255; result.blue = 255; break;
    case 2: result.length = 0; result.dot = true; result.thickness = 2; break;
    case 3: result.tStyle = true; result.red = 255; result.green = 220; result.blue = 0; break;
    case 4: result.dynamic = true; break;
    default: break;
    }
    return result;
}

// Optional versioned tail of settings.txt. Old/truncated files retain defaults.
inline bool readCrosshairSettings(std::istream& file, CrosshairSettings& settings)
{
    std::string version;
    CrosshairSettings candidate;
    int dynamic, dot, tStyle, outline;
    if (!(file >> version) || version != "crosshair_v1" ||
        !(file >> dynamic >> candidate.length >> candidate.thickness >> candidate.gap
               >> dot >> tStyle >> outline >> candidate.outlineThickness
               >> candidate.red >> candidate.green >> candidate.blue >> candidate.alpha))
        return false;
    candidate.dynamic = dynamic != 0;
    candidate.dot = dot != 0;
    candidate.tStyle = tStyle != 0;
    candidate.outline = outline != 0;
    candidate.sanitize();
    settings = candidate;
    return true;
}
inline void writeCrosshairSettings(std::ostream& file, const CrosshairSettings& s)
{
    file << "crosshair_v1 " << s.dynamic << ' ' << s.length << ' ' << s.thickness << ' '
         << s.gap << ' ' << s.dot << ' ' << s.tStyle << ' ' << s.outline << ' '
         << s.outlineThickness << ' ' << s.red << ' ' << s.green << ' ' << s.blue
         << ' ' << s.alpha << '\n';
}

struct CrosshairRect { float x, y, width, height; };
struct CrosshairGeometry
{
    std::array<CrosshairRect, 5> rects{};
    int count = 0;
    float outline = 0;
};
// Both axes use the vertical scale to preserve shape on ultrawide displays.
// Dynamic spread is cosmetic feedback, not weapon accuracy.
inline CrosshairGeometry crosshairGeometry(CrosshairSettings settings, int height, float spread = 0)
{
    settings.sanitize();
    CrosshairGeometry result;
    const float scale = std::max(height, 1) / 480.0f;
    const float thickness = std::max(1.0f, std::round(settings.thickness * scale));
    const float half = thickness * 0.5f;
    const float length = std::round(settings.length * scale);
    const float gap = std::round((settings.gap + (settings.dynamic ? std::clamp(spread, 0.0f, 12.0f) : 0.0f)) * scale) + half;
    if (settings.outline)
        result.outline = std::max(1.0f, std::round(settings.outlineThickness * scale));
    if (length > 0)
    {
        result.rects[result.count++] = {-gap - length, -half, length, thickness};
        result.rects[result.count++] = {gap, -half, length, thickness};
        result.rects[result.count++] = {-half, gap, thickness, length};
        if (!settings.tStyle)
            result.rects[result.count++] = {-half, -gap - length, thickness, length};
    }
    if (settings.dot) result.rects[result.count++] = {-half, -half, thickness, thickness};
    return result;
}
} // namespace nx3d::client
#endif
