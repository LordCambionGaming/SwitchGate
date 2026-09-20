#include "Level.h"
#include "raymath.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>

using json = nlohmann::json;

 
// Helper di conversione JSON -> tipi raylib
 

static Vector3 ParseVec3(const json& j, Vector3 fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    try {
        return Vector3{ j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
    } catch (...) {
        return fallback;
    }
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static Color ParseColor(const json& j, Color fallback) {
    if (j.is_string()) {
        static const std::unordered_map<std::string, Color> table = {
            { "red", RED }, { "rosso", RED },
            { "blue", BLUE }, { "blu", BLUE },
            { "green", GREEN }, { "verde", GREEN },
            { "yellow", YELLOW }, { "giallo", YELLOW },
            { "orange", ORANGE }, { "arancione", ORANGE },
            { "purple", PURPLE }, { "viola", PURPLE }, { "violet", VIOLET },
            { "pink", PINK }, { "rosa", PINK },
            { "gold", GOLD }, { "oro", GOLD },
            { "lime", LIME },
            { "skyblue", SKYBLUE }, { "azzurro", SKYBLUE },
            { "white", WHITE }, { "bianco", WHITE },
            { "black", BLACK }, { "nero", BLACK },
            { "gray", GRAY }, { "grey", GRAY }, { "grigio", GRAY },
            { "lightgray", LIGHTGRAY }, { "grigiochiaro", LIGHTGRAY },
            { "darkgray", DARKGRAY }, { "grigioscuro", DARKGRAY },
            { "brown", BROWN }, { "marrone", BROWN },
            { "darkbrown", DARKBROWN },
            { "maroon", MAROON }, { "bordeaux", MAROON },
            { "beige", BEIGE },
        };
        auto it = table.find(ToLower(j.get<std::string>()));
        if (it != table.end()) return it->second;
        return fallback;
    }
    if (j.is_array() && j.size() >= 3) {
        // Consente anche [r,g,b] o [r,g,b,a] con valori 0-255
        unsigned char r = (unsigned char)Clamp(j[0].get<float>(), 0, 255);
        unsigned char g = (unsigned char)Clamp(j[1].get<float>(), 0, 255);
        unsigned char b = (unsigned char)Clamp(j[2].get<float>(), 0, 255);
        unsigned char a = j.size() >= 4 ? (unsigned char)Clamp(j[3].get<float>(), 0, 255) : 255;
        return Color{ r, g, b, a };
    }
    return fallback;
}

static std::vector<LevelBox> ParseBoxArray(const json& arr) {
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

 
// Caricamento di un singolo file
 

bool LevelManager::LoadFromFile(const std::string& path, LevelData& out, std::string& errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorMessage = "Impossibile aprire il file: " + path;
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        errorMessage = "JSON non valido in \"" + path + "\": " + e.what();
        return false;
    }

    LevelData lvl;
    lvl.filePath = path;

    try {
        if (j.contains("name")) lvl.name = j["name"].get<std::string>();
        if (j.contains("description")) lvl.description = j["description"].get<std::string>();
        if (j.contains("player_start")) lvl.playerStart = ParseVec3(j["player_start"], lvl.playerStart);
        if (j.contains("gravity")) lvl.gravityEnabled = j["gravity"].get<bool>();
        if (j.contains("fall_reset_y")) lvl.fallResetY = j["fall_reset_y"].get<float>();

        if (j.contains("switches") && j["switches"].is_array()) {
            for (const auto& s : j["switches"]) {
                LevelSwitch sw;
                if (s.contains("position")) sw.position = ParseVec3(s["position"], sw.position);
                if (s.contains("color")) sw.color = ParseColor(s["color"], sw.color);
                if (s.contains("name")) sw.name = s["name"].get<std::string>();
                lvl.switches.push_back(sw);
            }
        }

        if (j.contains("random_sequence")) lvl.randomSequence = j["random_sequence"].get<bool>();
        if (j.contains("sequence") && j["sequence"].is_array()) {
            for (const auto& v : j["sequence"]) lvl.fixedSequence.push_back(v.get<int>());
        }
        // Se la sequenza fissa e' incoerente con il numero di interruttori, ripiega sull'ordine naturale.
        if (!lvl.randomSequence && !lvl.switches.empty()) {
            bool valid = lvl.fixedSequence.size() == lvl.switches.size();
            if (valid) {
                std::vector<int> sorted = lvl.fixedSequence;
                std::sort(sorted.begin(), sorted.end());
                for (size_t k = 0; k < sorted.size(); k++) if (sorted[k] != (int)k) { valid = false; break; }
            }
            if (!valid) {
                lvl.fixedSequence.resize(lvl.switches.size());
                for (size_t k = 0; k < lvl.switches.size(); k++) lvl.fixedSequence[k] = (int)k;
            }
        }

        if (j.contains("platforms")) lvl.platforms = ParseBoxArray(j["platforms"]);
        if (lvl.platforms.empty()) {
            // Pavimento di default se l'autore del livello non ne specifica uno.
            lvl.platforms.push_back(LevelBox{ Vector3{0, -0.25f, 0}, Vector3{40, 0.5f, 40}, LIGHTGRAY });
        }
        if (j.contains("obstacles")) lvl.obstacles = ParseBoxArray(j["obstacles"]);
        if (j.contains("draggables")) lvl.draggables = ParseBoxArray(j["draggables"]);
        if (j.contains("pads") && j["pads"].is_array()) {
            for (const auto& p : j["pads"]) {
                LevelPad pad;
                if (p.contains("position")) pad.position = ParseVec3(p["position"], pad.position);
                if (p.contains("size")) pad.size = ParseVec3(p["size"], pad.size);
                if (p.contains("color")) pad.color = ParseColor(p["color"], pad.color);
                if (p.contains("linked_door")) pad.linkedDoor = p["linked_door"].get<int>();
                lvl.pads.push_back(pad);
            }
        }

        if (j.contains("doors") && j["doors"].is_array()) {
            for (const auto& d : j["doors"]) {
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
                lvl.doors.push_back(door);
            }
        } else if (j.contains("door")) {
            // Schema precedente: una singola porta, sempre legata alla
            // risoluzione dell'intero puzzle.
            const auto& d = j["door"];
            LevelDoor door;
            if (d.contains("position")) door.position = ParseVec3(d["position"], door.position);
            if (d.contains("size")) door.size = ParseVec3(d["size"], door.size);
            lvl.doors.push_back(door);
        }
        if (j.contains("exit")) {
            const auto& e = j["exit"];
            if (e.contains("position")) lvl.exitPosition = ParseVec3(e["position"], lvl.exitPosition);
            if (e.contains("radius")) lvl.exitRadius = e["radius"].get<float>();
        }
    } catch (const std::exception& e) {
        errorMessage = "Errore nello schema del livello \"" + path + "\": " + e.what();
        return false;
    }

    out = lvl;
    return true;
}

 
// Salvataggio (usato dall'editor di livelli)
 

static bool ColorsEqual(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static json ColorToJson(Color c) {
    struct Named { const char* name; Color color; };
    static const Named table[] = {
        { "red", RED }, { "blue", BLUE }, { "green", GREEN }, { "yellow", YELLOW },
        { "orange", ORANGE }, { "purple", PURPLE }, { "violet", VIOLET }, { "pink", PINK },
        { "gold", GOLD }, { "lime", LIME }, { "skyblue", SKYBLUE }, { "white", WHITE },
        { "black", BLACK }, { "gray", GRAY }, { "lightgray", LIGHTGRAY }, { "darkgray", DARKGRAY },
        { "brown", BROWN }, { "darkbrown", DARKBROWN }, { "maroon", MAROON }, { "beige", BEIGE },
    };
    for (const auto& n : table) {
        if (ColorsEqual(c, n.color)) return json(n.name);
    }
    if (c.a == 255) return json::array({ c.r, c.g, c.b });
    return json::array({ c.r, c.g, c.b, c.a });
}

static json Vec3ToJson(Vector3 v) {
    return json::array({ v.x, v.y, v.z });
}

static json BoxArrayToJson(const std::vector<LevelBox>& boxes) {
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

bool LevelManager::SaveToFile(const std::string& path, const LevelData& level, std::string& errorMessage) {
    json j;
    j["name"] = level.name;
    j["description"] = level.description;
    j["player_start"] = Vec3ToJson(level.playerStart);
    j["gravity"] = level.gravityEnabled;
    j["fall_reset_y"] = level.fallResetY;

    json switchesArr = json::array();
    for (const auto& s : level.switches) {
        switchesArr.push_back({
            { "position", Vec3ToJson(s.position) },
            { "color", ColorToJson(s.color) },
            { "name", s.name }
        });
    }
    j["switches"] = switchesArr;

    j["random_sequence"] = level.randomSequence;
    if (!level.randomSequence) j["sequence"] = level.fixedSequence;

    j["platforms"] = BoxArrayToJson(level.platforms);
    j["obstacles"] = BoxArrayToJson(level.obstacles);
    j["draggables"] = BoxArrayToJson(level.draggables);
    json padsArr = json::array();
    for (const auto& p : level.pads) {
        padsArr.push_back({
            { "position", Vec3ToJson(p.position) },
            { "size", Vec3ToJson(p.size) },
            { "color", ColorToJson(p.color) },
            { "linked_door", p.linkedDoor }
        });
    }
    j["pads"] = padsArr;

    json doorsArr = json::array();
    for (const auto& d : level.doors) {
        doorsArr.push_back({
            { "position", Vec3ToJson(d.position) },
            { "size", Vec3ToJson(d.size) },
            { "color", ColorToJson(d.color) },
            { "linked_switch", d.linkedSwitch },
            { "linked_switches", d.linkedSwitches },
            { "logic_op", d.logicOp },
            { "rotated", d.rotated }
        });
    }
    j["doors"] = doorsArr;
    j["exit"] = { { "position", Vec3ToJson(level.exitPosition) }, { "radius", level.exitRadius } };

    std::ofstream file(path);
    if (!file.is_open()) {
        errorMessage = "Impossibile scrivere il file: " + path;
        return false;
    }
    file << j.dump(2);
    return true;
}

 
// Scansione della cartella dei livelli
 

void LevelManager::ScanDirectory(const std::string& directory) {
    levels.clear();
    errors.clear();

    if (!DirectoryExists(directory.c_str())) {
        errors.push_back("Cartella livelli non trovata: " + directory);
        return;
    }

    FilePathList files = LoadDirectoryFiles(directory.c_str());
    for (unsigned int i = 0; i < files.count; i++) {
        std::string path = files.paths[i];
        std::string ext = GetFileExtension(path.c_str()) ? GetFileExtension(path.c_str()) : "";
        if (ToLower(ext) != ".json") continue;

        LevelData lvl;
        std::string err;
        if (LoadFromFile(path, lvl, err)) {
            levels.push_back(lvl);
        } else {
            errors.push_back(err);
        }
    }
    UnloadDirectoryFiles(files);

    // Ordine stabile e prevedibile nel menu di selezione.
    std::sort(levels.begin(), levels.end(), [](const LevelData& a, const LevelData& b) {
        return a.filePath < b.filePath;
    });
}