#include "Mirror.h"

using json = nlohmann::json;

std::vector<LevelMirror> ParseMirrorsField(const json& root) {
    std::vector<LevelMirror> out;
    if (!root.contains("mirrors") || !root["mirrors"].is_array()) return out;
    for (const auto& m : root["mirrors"]) {
        LevelMirror mir;
        if (m.contains("position")) mir.position = ParseVec3(m["position"], mir.position);
        if (m.contains("angle")) mir.angleDeg = m["angle"].get<float>();
        if (m.contains("length")) mir.length = m["length"].get<float>();
        if (m.contains("height")) mir.height = m["height"].get<float>();
        if (m.contains("color")) mir.color = ParseColor(m["color"], mir.color);
        if (m.contains("subworld")) mir.subworld = m["subworld"].get<int>();
        out.push_back(mir);
    }
    return out;
}

json MirrorsToJson(const std::vector<LevelMirror>& mirrors) {
    json arr = json::array();
    for (const auto& m : mirrors) {
        arr.push_back({
            { "position", Vec3ToJson(m.position) },
            { "angle", m.angleDeg },
            { "length", m.length },
            { "height", m.height },
            { "color", ColorToJson(m.color) },
            { "subworld", m.subworld }
        });
    }
    return arr;
}