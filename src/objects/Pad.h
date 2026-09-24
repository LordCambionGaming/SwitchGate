#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <vector>

// Una pedana a pressione: si attiva quando una cassa trascinabile dello
// stesso colore ci si ferma sopra, e in quel caso puo' aprire una porta
// collegata (linkedDoor = indice in LevelData::doors, -1 = nessun effetto).
struct LevelPad {
    Vector3 position{ 0, 0.05f, 0 };
    Vector3 size{ 1.5f, 0.1f, 1.5f };
    Color color = RED;
    int linkedDoor = -1;
    int subworld = -1;                     // -1 = condivisa tra tutti i sub-mondi
};

std::vector<LevelPad> ParsePadsField(const nlohmann::json& root);

nlohmann::json PadsToJson(const std::vector<LevelPad>& pads);