#include "Switch.h"

using json = nlohmann::json;

std::vector<LevelSwitch> ParseSwitchesField(const json& root) {
    std::vector<LevelSwitch> out;
    if (!root.contains("switches") || !root["switches"].is_array()) return out;
    for (const auto& s : root["switches"]) {
        LevelSwitch sw;
        if (s.contains("position")) sw.position = ParseVec3(s["position"], sw.position);
        if (s.contains("color")) sw.color = ParseColor(s["color"], sw.color);
        if (s.contains("name")) sw.name = s["name"].get<std::string>();
        out.push_back(sw);
    }
    return out;
}

json SwitchesToJson(const std::vector<LevelSwitch>& switches) {
    json arr = json::array();
    for (const auto& s : switches) {
        arr.push_back({
            { "position", Vec3ToJson(s.position) },
            { "color", ColorToJson(s.color) },
            { "name", s.name }
        });
    }
    return arr;
}
