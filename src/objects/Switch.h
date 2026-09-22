#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Un interruttore da cliccare in un certo ordine.
struct LevelSwitch {
    Vector3 position{ 0, 0.5f, 0 };
    Color color = RED;
    std::string name = "?";
};

// Legge l'array "switches" dal JSON radice del livello (se presente).
std::vector<LevelSwitch> ParseSwitchesField(const nlohmann::json& root);

nlohmann::json SwitchesToJson(const std::vector<LevelSwitch>& switches);
