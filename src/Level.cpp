#include "Level.h"
#include "objects/JsonUtil.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

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

        // Ogni tipo di oggetto sa leggere la propria chiave dal JSON radice.
        lvl.switches = ParseSwitchesField(j);
        lvl.pads = ParsePadsField(j);
        lvl.mirrors = ParseMirrorsField(j);
        lvl.emitters = ParseEmittersField(j);
        lvl.receivers = ParseReceiversField(j);
        lvl.doors = ParseDoorsField(j);

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

        // Le tre "famiglie" di box condividono lo stesso formato ma vivono
        // sotto chiavi diverse, quindi qui passiamo direttamente il sotto-array.
        if (j.contains("platforms")) lvl.platforms = ParseBoxArray(j["platforms"]);
        if (lvl.platforms.empty()) {
            // Pavimento di default se l'autore del livello non ne specifica uno.
            lvl.platforms.push_back(LevelBox{ Vector3{0, -0.25f, 0}, Vector3{40, 0.5f, 40}, LIGHTGRAY });
        }
        if (j.contains("obstacles")) lvl.obstacles = ParseBoxArray(j["obstacles"]);
        if (j.contains("draggables")) lvl.draggables = ParseBoxArray(j["draggables"]);

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

bool LevelManager::SaveToFile(const std::string& path, const LevelData& level, std::string& errorMessage) {
    json j;
    j["name"] = level.name;
    j["description"] = level.description;
    j["player_start"] = Vec3ToJson(level.playerStart);
    j["gravity"] = level.gravityEnabled;
    j["fall_reset_y"] = level.fallResetY;

    j["switches"] = SwitchesToJson(level.switches);

    j["random_sequence"] = level.randomSequence;
    if (!level.randomSequence) j["sequence"] = level.fixedSequence;

    j["platforms"] = BoxArrayToJson(level.platforms);
    j["obstacles"] = BoxArrayToJson(level.obstacles);
    j["draggables"] = BoxArrayToJson(level.draggables);
    j["pads"] = PadsToJson(level.pads);

    j["mirrors"] = MirrorsToJson(level.mirrors);
    j["emitters"] = EmittersToJson(level.emitters);
    j["receivers"] = ReceiversToJson(level.receivers);

    j["doors"] = DoorsToJson(level.doors);
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
        if (ToLowerStr(ext) != ".json") continue;

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
