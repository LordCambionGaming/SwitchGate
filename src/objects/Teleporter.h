#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <vector>

// Un teletrasporto invisibile: appena il giocatore entra nel raggio di "from"
// riappare esattamente a "to" (senza alcun effetto visivo, ne' in-game ne'
// nell'editor durante il gioco - solo l'editor lo mostra, per poterlo
// piazzare). Usato per illusioni come la scala infinita di Penrose: si sale
// una rampa normale, e in cima si viene riportati silenziosamente in fondo,
// dando la sensazione di salire all'infinito senza mai arrivare in cima.
struct LevelTeleporter {
    Vector3 from{ 0, 1, 0 };
    float radius = 1.0f;
    Vector3 to{ 0, 1, 0 };
    int subworld = -1;   // -1 = attivo in ogni sub-mondo
};

std::vector<LevelTeleporter> ParseTeleportersField(const nlohmann::json& root);

nlohmann::json TeleportersToJson(const std::vector<LevelTeleporter>& teleporters);