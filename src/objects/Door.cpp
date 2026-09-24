#include "Door.h"

using json = nlohmann::json;

std::vector<LevelDoor> ParseDoorsField(const json& root) {
    std::vector<LevelDoor> out;

    if (root.contains("doors") && root["doors"].is_array()) {
        for (const auto& d : root["doors"]) {
            LevelDoor door;
            if (d.contains("position")) door.position = ParseVec3(d["position"], door.position);
            if (d.contains("size")) door.size = ParseVec3(d["size"], door.size);
            if (d.contains("color")) door.color = ParseColor(d["color"], door.color);
            if (d.contains("linked_switch")) door.linkedSwitch = d["linked_switch"].get<int>();
            if (d.contains("linked_switches") && d["linked_switches"].is_array()) {
                for (const auto& swIdx : d["linked_switches"]) {
                    door.linkedSwitches.push_back(swIdx.get<int>());
                }
            }
            if (d.contains("logic_op")) door.logicOp = d["logic_op"].get<std::string>();
            if (d.contains("rotated")) door.rotated = d["rotated"].get<bool>();
            if (d.contains("subworld")) door.subworld = d["subworld"].get<int>();
            out.push_back(door);
        }
    } else if (root.contains("door")) {
        // Schema precedente: una singola porta, sempre legata alla
        // risoluzione dell'intero puzzle.
        const auto& d = root["door"];
        LevelDoor door;
        if (d.contains("position")) door.position = ParseVec3(d["position"], door.position);
        if (d.contains("size")) door.size = ParseVec3(d["size"], door.size);
        out.push_back(door);
    }

    return out;
}

json DoorsToJson(const std::vector<LevelDoor>& doors) {
    json arr = json::array();
    for (const auto& d : doors) {
        arr.push_back({
            { "position", Vec3ToJson(d.position) },
            { "size", Vec3ToJson(d.size) },
            { "color", ColorToJson(d.color) },
            { "linked_switch", d.linkedSwitch },
            { "linked_switches", d.linkedSwitches },
            { "logic_op", d.logicOp },
            { "rotated", d.rotated },
            { "subworld", d.subworld }
        });
    }
    return arr;
}