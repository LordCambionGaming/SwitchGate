#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <vector>

// Un emettitore di raggio di luce: spara un fascio continuo dalla propria
// posizione nella direzione indicata da "angleDeg" (0 = verso +Z, 90 = verso
// +X, come per gli specchi), finche' non colpisce uno specchio (si
// riflette), un ostacolo/porta chiusa (si ferma) o esce dai limiti del
// livello.
struct LevelEmitter {
    Vector3 position{ 0, 1.0f, 0 };
    float angleDeg = 0.0f;
    Color color = RED;
    int subworld = -1;                     // -1 = condiviso tra tutti i sub-mondi
};

std::vector<LevelEmitter> ParseEmittersField(const nlohmann::json& root);

nlohmann::json EmittersToJson(const std::vector<LevelEmitter>& emitters);