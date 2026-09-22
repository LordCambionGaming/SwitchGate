#pragma once

#include "raylib.h"
#include <nlohmann/json.hpp>
#include <string>

// Helper di conversione condivisi da tutti i moduli in objects/: ogni
// oggetto di gioco (interruttore, porta, specchio...) si appoggia a queste
// funzioni per leggere/scrivere posizioni e colori nel JSON del livello,
// invece di reimplementare ogni volta lo stesso parsing.

Vector3 ParseVec3(const nlohmann::json& j, Vector3 fallback);
Color ParseColor(const nlohmann::json& j, Color fallback);

nlohmann::json Vec3ToJson(Vector3 v);
nlohmann::json ColorToJson(Color c);

std::string ToLowerStr(const std::string& s);
