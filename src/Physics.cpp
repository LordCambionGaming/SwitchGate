#include "Physics.h"
#include "raymath.h"
#include <algorithm>

const float GRAVITY = -24.0f;
const float JUMP_SPEED = 9.0f;
const float MOVE_SPEED = 6.0f;

bool FindGroundY(const LevelData& level, float x, float z, float maxY, int subworld, float& outY) {
    bool found = false;
    float best = -1e9f;
    for (const auto& p : level.platforms) {
        // Una piattaforma specifica di un altro sub-mondo (subworld != -1 e
        // diverso da quello corrente) e' come se non esistesse: non regge
        // chi ci cammina sopra finche' non si e' in quel mondo.
        if (p.subworld != -1 && p.subworld != subworld) continue;
        float minX = p.position.x - p.size.x / 2.0f, maxX = p.position.x + p.size.x / 2.0f;
        float minZ = p.position.z - p.size.z / 2.0f, maxZ = p.position.z + p.size.z / 2.0f;
        if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
            float top = p.position.y + p.size.y / 2.0f;
            if (top > maxY) continue; // sopra la quota su cui si poteva gia' essere appoggiati: ignorala
            if (!found || top > best) { best = top; found = true; }
        }
    }
    outY = best;
    return found;
}

// Speculare a FindGroundY: trova il bordo INFERIORE piu' basso tra le
// piattaforme sopra un punto (x, z), considerando solo quelle il cui fondo
// non e' piu' basso di minY (la quota della testa prima del passo di fisica
// corrente). Senza questo controllo, saltando da sotto una piattaforma la si
// attraverserebbe: il fondo verrebbe ignorato finche' non si e' gia' sopra,
// e poi FindGroundY vi si "appoggerebbe" da sopra come se nulla fosse.
bool FindCeilingY(const LevelData& level, float x, float z, float minY, int subworld, float& outY) {
    bool found = false;
    float best = 1e9f;
    for (const auto& p : level.platforms) {
        if (p.subworld != -1 && p.subworld != subworld) continue;
        float minX = p.position.x - p.size.x / 2.0f, maxX = p.position.x + p.size.x / 2.0f;
        float minZ = p.position.z - p.size.z / 2.0f, maxZ = p.position.z + p.size.z / 2.0f;
        if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
            float bottom = p.position.y - p.size.y / 2.0f;
            if (bottom < minY) continue; // sotto la quota della testa prima di questo passo: non e' un soffitto valido
            if (!found || bottom < best) { best = bottom; found = true; }
        }
    }
    outY = best;
    return found;
}

void ResolveBoxCollision(Vector3& pos, float radius, Vector3 boxPos, Vector3 boxSize) {
    // Se la sfera del giocatore (o della cassa) non si sovrappone verticalmente
    // col box, non lo blocca: prima questo controllo mancava del tutto, quindi
    // ogni "muro" si comportava come se fosse alto all'infinito, bloccando
    // anche chi ci saltava sopra.
    float boxMinY = boxPos.y - boxSize.y / 2.0f;
    float boxMaxY = boxPos.y + boxSize.y / 2.0f;
    if (pos.y + radius <= boxMinY || pos.y - radius >= boxMaxY) return;

    float minX = boxPos.x - boxSize.x / 2.0f - radius;
    float maxX = boxPos.x + boxSize.x / 2.0f + radius;
    float minZ = boxPos.z - boxSize.z / 2.0f - radius;
    float maxZ = boxPos.z + boxSize.z / 2.0f + radius;

    if (pos.x > minX && pos.x < maxX && pos.z > minZ && pos.z < maxZ) {
        float overlapLeft = pos.x - minX;
        float overlapRight = maxX - pos.x;
        float overlapBack = pos.z - minZ;
        float overlapFront = maxZ - pos.z;

        float minOverlapX = std::min(overlapLeft, overlapRight);
        float minOverlapZ = std::min(overlapBack, overlapFront);

        if (minOverlapX < minOverlapZ) {
            pos.x += (overlapLeft < overlapRight) ? -minOverlapX : minOverlapX;
        } else {
            pos.z += (overlapBack < overlapFront) ? -minOverlapZ : minOverlapZ;
        }
    }
}

void ResolveBoxToBoxCollision(Vector3& posA, Vector3 sizeA, Vector3& posB, Vector3 sizeB) {
    float minXA = posA.x - sizeA.x / 2.0f;
    float maxXA = posA.x + sizeA.x / 2.0f;
    float minZA = posA.z - sizeA.z / 2.0f;
    float maxZA = posA.z + sizeA.z / 2.0f;

    float minXB = posB.x - sizeB.x / 2.0f;
    float maxXB = posB.x + sizeB.x / 2.0f;
    float minZB = posB.z - sizeB.z / 2.0f;
    float maxZB = posB.z + sizeB.z / 2.0f;

    float minYA = posA.y - sizeA.y / 2.0f;
    float maxYA = posA.y + sizeA.y / 2.0f;
    float minYB = posB.y - sizeB.y / 2.0f;
    float maxYB = posB.y + sizeB.y / 2.0f;

    if (maxXA > minXB && minXA < maxXB && maxYA > minYB && minYA < maxYB && maxZA > minZB && minZA < maxZB) {
        float overlapLeft = maxXA - minXB;
        float overlapRight = maxXB - minXA;
        float overlapBack = maxZA - minZB;
        float overlapFront = maxZB - minZA;

        float minOverlapX = std::min(overlapLeft, overlapRight);
        float minOverlapZ = std::min(overlapBack, overlapFront);

        if (minOverlapX < minOverlapZ) {
            posA.x += (overlapLeft < overlapRight) ? -overlapLeft : overlapRight;
        } else {
            posA.z += (overlapBack < overlapFront) ? -overlapBack : overlapFront;
        }
    }
}

void ResolveObstacles(Vector3& pos, float radius, const std::vector<LevelBox>& obstacles, int subworld) {
    for (const auto& o : obstacles) {
        if (o.subworld != -1 && o.subworld != subworld) continue; // muro di un altro sub-mondo: intangibile
        ResolveBoxCollision(pos, radius, o.position, o.size);
    }
}

void ResolveDoors(Vector3& pos, float radius, const std::vector<LevelDoor>& doors, const std::vector<float>& doorHeights, int subworld) {
    for (size_t i = 0; i < doors.size(); i++) {
        if (doors[i].subworld != -1 && doors[i].subworld != subworld) continue;
        if (i < doorHeights.size() && doorHeights[i] <= 0.1f) continue;
        // door.position.y e' la base della porta (vedi Rendering.cpp): il box
        // di collisione va centrato a base + meta' altezza, non su door.position.y
        // direttamente, altrimenti non corrisponde a dove la porta e' disegnata.
        Vector3 sz = GetDoorEffectiveSize(doors[i]);
        Vector3 center = { doors[i].position.x, doors[i].position.y + sz.y / 2.0f, doors[i].position.z };
        ResolveBoxCollision(pos, radius, center, sz);
    }
}

void UpdatePlayerPhysics(Player& player, const LevelData& level, const std::vector<float>& doorHeights, Vector3 moveInput, float dt, bool jumpPressed, int subworld) {
    // Movimento orizzontale
    if (moveInput.x != 0.0f || moveInput.z != 0.0f) {
        moveInput = Vector3Normalize(moveInput);
        player.position.x += moveInput.x * MOVE_SPEED * dt;
        player.position.z += moveInput.z * MOVE_SPEED * dt;
    }
    ResolveObstacles(player.position, player.radius, level.obstacles, subworld);
    ResolveDoors(player.position, player.radius, level.doors, doorHeights, subworld);
    player.position.x = Clamp(player.position.x, -19.5f, 19.5f);
    player.position.z = Clamp(player.position.z, -19.5f, 19.5f);

    // Quota dei piedi e della testa PRIMA di applicare la gravita' di questo
    // frame: servono a limitare FindGroundY/FindCeilingY alle sole superfici
    // su cui si poteva gia' essere appoggiati/sotto (vedi commenti in Physics.h).
    float prevFeetY = player.position.y - player.radius;
    float prevHeadY = player.position.y + player.radius;

    if (level.gravityEnabled) {
        player.velocity.y += GRAVITY * dt;
    } else {
        player.velocity.y = 0.0f;
    }
    player.position.y += player.velocity.y * dt;

    // Soffitto: se si sta salendo (salto) e la testa arriva al fondo di una
    // piattaforma che prima era sopra la testa, ci si ferma li' invece di
    // attraversarla.
    float ceilY;
    bool hasCeiling = FindCeilingY(level, player.position.x, player.position.z, prevHeadY - 0.05f, subworld, ceilY);
    if (hasCeiling && player.velocity.y > 0.0f && (player.position.y + player.radius) >= ceilY) {
        player.position.y = ceilY - player.radius;
        player.velocity.y = 0.0f;
    }

    float groundY;
    bool hasGround = FindGroundY(level, player.position.x, player.position.z, prevFeetY + 0.05f, subworld, groundY);
    float feetY = player.position.y - player.radius;

    if (hasGround && feetY <= groundY && player.velocity.y <= 0.0f) {
        player.position.y = groundY + player.radius;
        player.velocity.y = 0.0f;
        player.onGround = true;
    } else {
        player.onGround = !hasGround ? false : (feetY <= groundY + 0.02f);
    }

    if (jumpPressed && player.onGround && level.gravityEnabled) {
        player.velocity.y = JUMP_SPEED;
        player.onGround = false;
    }

    // Caduto in un pozzo (vuoto tra piattaforme): torna al punto di partenza.
    if (player.position.y < level.fallResetY) {
        player.position = level.playerStart;
        player.velocity = { 0, 0, 0 };
    }
}