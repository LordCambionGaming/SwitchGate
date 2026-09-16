#include "Scores.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

bool LoadHighscores(const std::string& path, HighscoreMap& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    json j;
    try {
        file >> j;
    } catch (...) {
        return false;
    }
    if (!j.is_object()) return false;

    HighscoreMap map;
    for (auto it = j.begin(); it != j.end(); ++it) {
        HighscoreEntry entry;
        entry.hasScore = true;
        if (it.value().contains("best_time")) entry.bestTime = it.value()["best_time"].get<float>();
        if (it.value().contains("best_score")) entry.bestScore = it.value()["best_score"].get<int>();
        map[it.key()] = entry;
    }
    out = map;
    return true;
}

bool SaveHighscores(const std::string& path, const HighscoreMap& scores) {
    json j = json::object();
    for (const auto& kv : scores) {
        if (!kv.second.hasScore) continue;
        j[kv.first] = {
            { "best_time", kv.second.bestTime },
            { "best_score", kv.second.bestScore }
        };
    }
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

std::string FormatTime(float seconds) {
    if (seconds < 0.0f) seconds = 0.0f;
    int totalTenths = (int)(seconds * 10.0f + 0.5f);
    int minutes = totalTenths / 600;
    int secs = (totalTenths / 10) % 60;
    int tenths = totalTenths % 10;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d.%d", minutes, secs, tenths);
    return std::string(buf);
}
