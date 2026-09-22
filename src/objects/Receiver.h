#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <vector>

// Un ricevitore: si "accende" quando colpito da un raggio di luce dello
// stesso colore (WHITE = accetta qualunque colore). Come le pedane, puo'
// aprire una porta collegata (linkedDoor, -1 = nessun effetto) e concorre
// alla logica AND/OR della porta insieme a interruttori e pedane.
struct LevelReceiver {
    Vector3 position{ 0, 1.0f, 0 };
    float radius = 0.4f;
    Color color = RED;
    int linkedDoor = -1;
};

std::vector<LevelReceiver> ParseReceiversField(const nlohmann::json& root);

nlohmann::json ReceiversToJson(const std::vector<LevelReceiver>& receivers);
