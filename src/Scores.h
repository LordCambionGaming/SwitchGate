
#pragma once
#include <string>
#include <unordered_map>

struct HighscoreEntry {
    bool hasScore = false;
    float bestTime = 0.0f;
    int bestScore = 0;
};

using HighscoreMap = std::unordered_map<std::string, HighscoreEntry>;

bool LoadHighscores(const std::string& path, HighscoreMap& out);
bool SaveHighscores(const std::string& path, const HighscoreMap& scores);

// Formatta un tempo in secondi come "MM:SS.d" (es. 72.34 -> "01:12.3").
std::string FormatTime(float seconds);
