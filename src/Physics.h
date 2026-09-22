#pragma once

#include "raylib.h"
#include "Level.h"
#include <vector>

// Stato fisico del giocatore (posizione, velocita', se sta toccando terra).
struct Player {
    Vector3 position{ 0, 1.0f, 8 };
    Vector3 velocity{ 0, 0, 0 };
    float radius = 0.5f;
    bool onGround = false;
};

extern const float GRAVITY;
extern const float JUMP_SPEED;
extern const float MOVE_SPEED;

// Trova la quota della superficie di appoggio piu' alta sotto un punto (x, z).
// Se ritorna false, non c'e' nessuna piattaforma li' sotto: si sta cadendo.
bool FindGroundY(const LevelData& level, float x, float z, float& outY);

// Spinge un punto (giocatore o cassa) fuori da un parallelepipedo solido:
// semplice risoluzione per assi separati (x poi z).
void ResolveBoxCollision(Vector3& pos, float radius, Vector3 boxPos, Vector3 boxSize);

// Risolve la collisione tra due casse: la cassa A si ferma (viene respinta
// fuori da B) senza muovere B.
void ResolveBoxToBoxCollision(Vector3& posA, Vector3 sizeA, Vector3& posB, Vector3 sizeB);

void ResolveObstacles(Vector3& pos, float radius, const std::vector<LevelBox>& obstacles);

// Una porta blocca il passaggio solo mentre non e' (quasi) del tutto aperta;
// l'altezza attuale viene dall'animazione della partita in corso, non dal
// dato statico del livello.
void ResolveDoors(Vector3& pos, float radius, const std::vector<LevelDoor>& doors, const std::vector<float>& doorHeights);

// Aggiorna la fisica del giocatore per un frame: movimento orizzontale,
// collisioni con ostacoli/porte, gravita', salto, e il "respawn" quando si
// cade in un pozzo sotto fallResetY.
void UpdatePlayerPhysics(Player& player, const LevelData& level, const std::vector<float>& doorHeights, Vector3 moveInput, float dt, bool jumpPressed);
