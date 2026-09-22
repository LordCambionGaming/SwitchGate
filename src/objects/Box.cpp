#include "Box.h"

using json = nlohmann::json;

std::vector<LevelBox> ParseBoxArray(const json& arr) {
    std::vector<LevelBox> out;
    if (!arr.is_array()) return out;
    for (const auto& item : arr) {
        LevelBox box;
        if (item.contains("position")) box.position = ParseVec3(item["position"], box.position);
        if (item.contains("size")) box.size = ParseVec3(item["size"], box.size);
        if (item.contains("color")) box.color = ParseColor(item["color"], box.color);
        out.push_back(box);
    }
    return out;
}

json BoxArrayToJson(const std::vector<LevelBox>& boxes) {
    json arr = json::array();
    for (const auto& b : boxes) {
        arr.push_back({
            { "position", Vec3ToJson(b.position) },
            { "size", Vec3ToJson(b.size) },
            { "color", ColorToJson(b.color) }
        });
    }
    return arr;
}
