#include "Physics.h"
#include "raymath.h"
#include <algorithm>

const float GRAVITY = -24.0f;
const float JUMP_SPEED = 9.0f;
const float MOVE_SPEED = 6.0f;

bool FindGroundY(const LevelData& level, float x, float z, float& outY) {
    bool found = false;
    float best = -1e9f;
    for (const auto& p : level.platforms) {
        float minX = p.position.x - p.size.x / 2.0f, maxX = p.position.x + p.size.x / 2.0f;
        float minZ = p.position.z - p.size.z / 2.0f, maxZ = p.position.z + p.size.z / 2.0f;
        if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
            float top = p.position.y + p.size.y / 2.0f;
            if (!found || top > best) { best = top; found = true; }
        }
    }
    outY = best;
    return found;
}

void ResolveBoxCollision(Vector3& pos, float radius, Vector3 boxPos, Vector3 boxSize) {
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

void ResolveObstacles(Vector3& pos, float radius, const std::vector<LevelBox>& obstacles) {
    for (const auto& o : obstacles) ResolveBoxCollision(pos, radius, o.position, o.size);
}

void ResolveDoors(Vector3& pos, float radius, const std::vector<LevelDoor>& doors, const std::vector<float>& doorHeights) {
    for (size_t i = 0; i < doors.size(); i++) {
        if (i < doorHeights.size() && doorHeights[i] <= 0.1f) continue;
        ResolveBoxCollision(pos, radius, doors[i].position, GetDoorEffectiveSize(doors[i]));
    }
}

void UpdatePlayerPhysics(Player& player, const LevelData& level, const std::vector<float>& doorHeights, Vector3 moveInput, float dt, bool jumpPressed) {
    // Movimento orizzontale
    if (moveInput.x != 0.0f || moveInput.z != 0.0f) {
        moveInput = Vector3Normalize(moveInput);
        player.position.x += moveInput.x * MOVE_SPEED * dt;
        player.position.z += moveInput.z * MOVE_SPEED * dt;
    }
    ResolveObstacles(player.position, player.radius, level.obstacles);
    ResolveDoors(player.position, player.radius, level.doors, doorHeights);
    player.position.x = Clamp(player.position.x, -19.5f, 19.5f);
    player.position.z = Clamp(player.position.z, -19.5f, 19.5f);

    if (level.gravityEnabled) {
        player.velocity.y += GRAVITY * dt;
    } else {
        player.velocity.y = 0.0f;
    }
    player.position.y += player.velocity.y * dt;

    float groundY;
    bool hasGround = FindGroundY(level, player.position.x, player.position.z, groundY);
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
