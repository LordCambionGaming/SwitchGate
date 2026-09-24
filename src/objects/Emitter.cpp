#include "Emitter.h"

using json = nlohmann::json;

std::vector<LevelEmitter> ParseEmittersField(const json& root) {
    std::vector<LevelEmitter> out;
    if (!root.contains("emitters") || !root["emitters"].is_array()) return out;
    for (const auto& e : root["emitters"]) {
        LevelEmitter em;
        if (e.contains("position")) em.position = ParseVec3(e["position"], em.position);
        if (e.contains("angle")) em.angleDeg = e["angle"].get<float>();
        if (e.contains("color")) em.color = ParseColor(e["color"], em.color);
        if (e.contains("subworld")) em.subworld = e["subworld"].get<int>();
        out.push_back(em);
    }
    return out;
}

json EmittersToJson(const std::vector<LevelEmitter>& emitters) {
    json arr = json::array();
    for (const auto& e : emitters) {
        arr.push_back({
            { "position", Vec3ToJson(e.position) },
            { "angle", e.angleDeg },
            { "color", ColorToJson(e.color) },
            { "subworld", e.subworld }
        });
    }
    return arr;
}