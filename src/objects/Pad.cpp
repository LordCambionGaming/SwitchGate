#include "Pad.h"

using json = nlohmann::json;

std::vector<LevelPad> ParsePadsField(const json& root) {
    std::vector<LevelPad> out;
    if (!root.contains("pads") || !root["pads"].is_array()) return out;
    for (const auto& p : root["pads"]) {
        LevelPad pad;
        if (p.contains("position")) pad.position = ParseVec3(p["position"], pad.position);
        if (p.contains("size")) pad.size = ParseVec3(p["size"], pad.size);
        if (p.contains("color")) pad.color = ParseColor(p["color"], pad.color);
        if (p.contains("linked_door")) pad.linkedDoor = p["linked_door"].get<int>();
        out.push_back(pad);
    }
    return out;
}

json PadsToJson(const std::vector<LevelPad>& pads) {
    json arr = json::array();
    for (const auto& p : pads) {
        arr.push_back({
            { "position", Vec3ToJson(p.position) },
            { "size", Vec3ToJson(p.size) },
            { "color", ColorToJson(p.color) },
            { "linked_door", p.linkedDoor }
        });
    }
    return arr;
}
