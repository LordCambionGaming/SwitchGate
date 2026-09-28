#include "Teleporter.h"

using json = nlohmann::json;

std::vector<LevelTeleporter> ParseTeleportersField(const json& root) {
    std::vector<LevelTeleporter> out;
    if (!root.contains("teleporters") || !root["teleporters"].is_array()) return out;
    for (const auto& t : root["teleporters"]) {
        LevelTeleporter tp;
        if (t.contains("from")) tp.from = ParseVec3(t["from"], tp.from);
        if (t.contains("radius")) tp.radius = t["radius"].get<float>();
        if (t.contains("to")) tp.to = ParseVec3(t["to"], tp.to);
        if (t.contains("subworld")) tp.subworld = t["subworld"].get<int>();
        out.push_back(tp);
    }
    return out;
}

json TeleportersToJson(const std::vector<LevelTeleporter>& teleporters) {
    json arr = json::array();
    for (const auto& t : teleporters) {
        arr.push_back({
            { "from", Vec3ToJson(t.from) },
            { "radius", t.radius },
            { "to", Vec3ToJson(t.to) },
            { "subworld", t.subworld }
        });
    }
    return arr;
}