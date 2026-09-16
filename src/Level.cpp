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
        if (lvl.switches.empty()) {
            errorMessage = "Il livello \"" + path + "\" non contiene nessun interruttore (\"switches\").";
            return false;
        }

        if (j.contains("random_sequence")) lvl.randomSequence = j["random_sequence"].get<bool>();
        if (j.contains("sequence") && j["sequence"].is_array()) {
            for (const auto& v : j["sequence"]) lvl.fixedSequence.push_back(v.get<int>());
        }
        // Se la sequenza fissa e' incoerente con il numero di interruttori, ripiega sull'ordine naturale.
        if (!lvl.randomSequence) {
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

        if (j.contains("door")) {
            const auto& d = j["door"];
            if (d.contains("position")) lvl.doorPosition = ParseVec3(d["position"], lvl.doorPosition);
            if (d.contains("size")) lvl.doorSize = ParseVec3(d["size"], lvl.doorSize);
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

    j["door"] = { { "position", Vec3ToJson(level.doorPosition) }, { "size", Vec3ToJson(level.doorSize) } };
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
