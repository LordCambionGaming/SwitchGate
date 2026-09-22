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

// Trova la quota della superficie di appoggio piu' alta sotto un punto (x, z),
// considerando solo le piattaforme il cui bordo superiore non supera maxY.
// Questo evita che una piattaforma piu' alta nella stessa colonna x/z (es. un
// ponte/soffitto sopra un pozzo) "catturi" chi ci sta cadendo sotto: maxY va
// passato come la quota dei piedi PRIMA del passo di fisica corrente (piu' un
// piccolo margine), cosi' si considerano solo le superfici su cui si poteva
// gia' essere appoggiati. Se ritorna false, non c'e' nessuna piattaforma
// valida li' sotto: si sta cadendo.
bool FindGroundY(const LevelData& level, float x, float z, float maxY, float& outY);

// Speculare a FindGroundY: trova il bordo inferiore piu' basso tra le
// piattaforme sopra un punto (x, z), considerando solo quelle il cui fondo
// non e' piu' basso di minY (la quota della testa/parte superiore PRIMA del
// passo di fisica corrente). Usata per bloccare chi salta contro il fondo
// di una piattaforma invece di lasciarlo attraversarla.
bool FindCeilingY(const LevelData& level, float x, float z, float minY, float& outY);

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