#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <vector>

// Uno specchio: pannello verticale piatto che riflette i raggi di luce.
// "angleDeg" e' la rotazione attorno all'asse Y: 0 = pannello disposto
// lungo l'asse X (superficie riflettente rivolta verso +Z/-Z), 90 = pannello
// lungo l'asse Z, e cosi' via a qualunque angolo intermedio (es. 45 per un
// classico specchio "diagonale"). "length" e' la larghezza del pannello.
struct LevelMirror {
    Vector3 position{ 0, 1.0f, 0 };
    float angleDeg = 45.0f;
    float length = 2.0f;
    float height = 2.0f;
    Color color = SKYBLUE;
};

std::vector<LevelMirror> ParseMirrorsField(const nlohmann::json& root);

nlohmann::json MirrorsToJson(const std::vector<LevelMirror>& mirrors);
