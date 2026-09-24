#pragma once

#include "raylib.h"
#include "JsonUtil.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Una porta che si apre quando un interruttore specifico viene attivato
// (linkedSwitch = indice in LevelData::switches), oppure quando l'intero
// puzzle e' risolto se linkedSwitch e' -1. "rotated" scambia larghezza (X) e
// profondita' (Z), per poterla orientare anche su un muro laterale.
struct LevelDoor {
    Vector3 position{ 0, 0, -9 };
    Vector3 size{ 4, 3, 0.5f };
    Color color = DARKBROWN;
    int linkedSwitch = -1;                 // Compatibilita' con il vecchio formato
    std::vector<int> linkedSwitches;       // Lista di interruttori collegati
    std::string logicOp = "OR";            // Operatore logico: "OR" oppure "AND"
    bool rotated = false;
    int subworld = -1;                     // -1 = condivisa tra tutti i sub-mondi
};

// Dimensioni della porta come vanno effettivamente usate per disegno e
// collisioni: se "rotated" e' true, X e Z sono scambiate.
inline Vector3 GetDoorEffectiveSize(const LevelDoor& door) {
    if (!door.rotated) return door.size;
    return Vector3{ door.size.z, door.size.y, door.size.x };
}

// Legge l'array "doors" dal JSON radice del livello. Se assente, ripiega sul
// vecchio schema con una singola porta sotto la chiave "door".
std::vector<LevelDoor> ParseDoorsField(const nlohmann::json& root);

nlohmann::json DoorsToJson(const std::vector<LevelDoor>& doors);