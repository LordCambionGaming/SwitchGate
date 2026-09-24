#include "Receiver.h"

using json = nlohmann::json;

std::vector<LevelReceiver> ParseReceiversField(const json& root) {
    std::vector<LevelReceiver> out;
    if (!root.contains("receivers") || !root["receivers"].is_array()) return out;
    for (const auto& r : root["receivers"]) {
        LevelReceiver rec;
        if (r.contains("position")) rec.position = ParseVec3(r["position"], rec.position);
        if (r.contains("radius")) rec.radius = r["radius"].get<float>();
        if (r.contains("color")) rec.color = ParseColor(r["color"], rec.color);
        if (r.contains("linked_door")) rec.linkedDoor = r["linked_door"].get<int>();
        if (r.contains("subworld")) rec.subworld = r["subworld"].get<int>();
        out.push_back(rec);
    }
    return out;
}

json ReceiversToJson(const std::vector<LevelReceiver>& receivers) {
    json arr = json::array();
    for (const auto& r : receivers) {
        arr.push_back({
            { "position", Vec3ToJson(r.position) },
            { "radius", r.radius },
            { "color", ColorToJson(r.color) },
            { "linked_door", r.linkedDoor },
            { "subworld", r.subworld }
        });
    }
    return arr;
}