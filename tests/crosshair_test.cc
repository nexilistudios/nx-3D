#include <client/crosshair/crosshair_settings.hh>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main()
{
    using namespace nx3d::client;
    try
    {
        for (int i = 0; i < static_cast<int>(kCrosshairPresets.size()); ++i)
        {
            auto expected = crosshairPreset(i);
            expected.alpha = 173;
            std::stringstream file;
            writeCrosshairSettings(file, expected);
            CrosshairSettings loaded;
            require(readCrosshairSettings(file, loaded), "Saved crosshair cannot be loaded");
            require(loaded == expected, "Saving loses crosshair fields");
        }
        CrosshairSettings unchanged = crosshairPreset(3);
        const auto original = unchanged;
        for (const char* content : {"", "crosshair_v2 1", "crosshair_v1 1 2 3", "crosshair_v1 invalid"})
        {
            std::istringstream oldOrTruncated(content);
            require(!readCrosshairSettings(oldOrTruncated, unchanged), "Invalid settings were accepted");
            require(unchanged == original, "Partial settings modified the crosshair");
        }
        std::istringstream invalid("crosshair_v1 1 -100 500 -50 1 1 0 50 -5 999 50 999");
        require(readCrosshairSettings(invalid, unchanged), "Out-of-range settings could not be normalized");
        require(unchanged.length == 0 && unchanged.thickness == 5 && unchanged.gap == -5 &&
                unchanged.red == 0 && unchanged.green == 255 && unchanged.alpha == 255 &&
                unchanged.outlineThickness == 3, "Saved values exceed supported ranges");
        unchanged.length = std::numeric_limits<float>::quiet_NaN();
        unchanged.thickness = std::numeric_limits<float>::infinity();
        unchanged.sanitize();
        require(std::isfinite(unchanged.length) && std::isfinite(unchanged.thickness), "Non-finite geometry survived validation");
        CrosshairSettings settings;
        for (int height : {480, 640, 720, 1080, 1440, 2160})
        {
            const auto geometry = crosshairGeometry(settings, height);
            require(geometry.count == 4, "Classic crosshair must have four arms");
            const auto& left = geometry.rects[0];
            const auto& right = geometry.rects[1];
            const auto& bottom = geometry.rects[2];
            const auto& top = geometry.rects[3];
            require(left.x + left.width == -right.x, "Horizontal arms are not centered");
            require(top.y + top.height == -bottom.y, "Vertical arms are not centered");
            require(left.width == top.height && left.height == top.width,
                    "Horizontal and vertical arms have different pixel dimensions");
            require(crosshairGeometry(settings, height, 9).rects[1].x == right.x,
                    "Static style responds to movement or firing");
            auto dynamic = settings;
            dynamic.dynamic = true;
            require(crosshairGeometry(dynamic, height, 9).rects[1].x > right.x,
                    "Dynamic style does not expand");
            require(crosshairGeometry(dynamic, height, -100).rects[1].x == right.x,
                    "Dynamic spread can shrink below the configured gap");
        }
        settings.tStyle = true;
        require(crosshairGeometry(settings, 1080).count == 3, "T-style has a top arm");
        settings.dot = true;
        require(crosshairGeometry(settings, 1080).count == 4, "T-style center dot is missing");
        settings.length = 0;
        const auto dot = crosshairGeometry(settings, 1080);
        require(dot.count == 1 && dot.rects[0].width == dot.rects[0].height &&
                dot.rects[0].x == -dot.rects[0].width * 0.5f, "Dot preset is not a centered square");
        settings.outline = false;
        require(crosshairGeometry(settings, 1080).outline == 0, "Disabled outlines remain visible");
        settings.dot = false;
        require(crosshairGeometry(settings, 1080).count == 0, "Zero length without dot should hide the crosshair");
        std::cout << "Crosshair settings and geometry passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
