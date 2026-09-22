#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <vector>

// Un parallelepipedo generico usato sia per le piattaforme (pavimento/gradini)
// sia per gli ostacoli (muri pieni che bloccano il movimento) sia per le
// casse trascinabili: stessa geometria, ruoli diversi a seconda di quale
// vettore di LevelData lo contiene.
struct LevelBox {
    Vector3 position{ 0, 0, 0 };
    Vector3 size{ 1, 1, 1 };
    Color color = LIGHTGRAY;
};

// Legge un array di box (non e' legato a una chiave fissa: platforms,
// obstacles e draggables condividono lo stesso formato ma vivono sotto tre
// chiavi diverse, quindi chi chiama passa direttamente il sotto-array giusto).
std::vector<LevelBox> ParseBoxArray(const nlohmann::json& arr);

nlohmann::json BoxArrayToJson(const std::vector<LevelBox>& boxes);
