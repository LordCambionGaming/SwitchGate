#include "Lighting.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

Vector3 DirFromAngleDeg(float angleDeg) {
    float rad = angleDeg * (PI / 180.0f);
    return Vector3{ sinf(rad), 0.0f, cosf(rad) };
}

// Intersezione raggio (2D, XZ) / segmento finito. Ritorna la distanza lungo
// il raggio se c'e' un colpo valido (t > eps e il punto cade dentro al
// segmento), altrimenti -1.
static float RaySegmentHitXZ(Vector3 origin, Vector3 dir, Vector3 segA, Vector3 segB) {
    float rX = dir.x, rZ = dir.z;
    float sX = segB.x - segA.x, sZ = segB.z - segA.z;
    float denom = rX * sZ - rZ * sX;
    if (fabsf(denom) < 1e-6f) return -1.0f; // paralleli
    float dx = segA.x - origin.x, dz = segA.z - origin.z;
    float t = (dx * sZ - dz * sX) / denom;
    float u = (dx * rZ - dz * rX) / denom;
    if (t > 0.01f && u >= 0.0f && u <= 1.0f) return t;
    return -1.0f;
}

// Intersezione raggio (2D, XZ) / rettangolo assiale (l'impronta a terra di
// un ostacolo/porta). Ritorna la distanza di ingresso, o -1 se non c'e' colpo.
static float RayBoxHitXZ(Vector3 origin, Vector3 dir, float minX, float maxX, float minZ, float maxZ) {
    float tmin = -1e9f, tmax = 1e9f;
    if (fabsf(dir.x) < 1e-6f) {
        if (origin.x < minX || origin.x > maxX) return -1.0f;
    } else {
        float t1 = (minX - origin.x) / dir.x, t2 = (maxX - origin.x) / dir.x;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    }
    if (fabsf(dir.z) < 1e-6f) {
        if (origin.z < minZ || origin.z > maxZ) return -1.0f;
    } else {
        float t1 = (minZ - origin.z) / dir.z, t2 = (maxZ - origin.z) / dir.z;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    }
    if (tmax < 0.01f || tmin > tmax) return -1.0f;
    return (tmin > 0.01f) ? tmin : 0.01f;
}

// Intersezione raggio (2D, XZ) / cerchio. Ritorna la distanza al primo punto
// di ingresso, o -1 se non c'e' colpo.
static float RayCircleHitXZ(Vector3 origin, Vector3 dir, Vector3 center, float radius) {
    float ocx = origin.x - center.x, ocz = origin.z - center.z;
    float b = 2.0f * (ocx * dir.x + ocz * dir.z);
    float c = ocx * ocx + ocz * ocz - radius * radius;
    float disc = b * b - 4.0f * c;
    if (disc < 0.0f) return -1.0f;
    float sq = sqrtf(disc);
    float t = (-b - sq) / 2.0f;
    if (t < 0.01f) t = (-b + sq) / 2.0f;
    if (t < 0.01f) return -1.0f;
    return t;
}

std::vector<LightBeamSegment> ComputeLightBeams(const LevelData& level,
                                                 const std::vector<float>& doorHeights,
                                                 const std::vector<Vector3>& draggablePositions,
                                                 int subworld,
                                                 std::vector<bool>& receiverLit) {
    std::vector<LightBeamSegment> segments;
    receiverLit.assign(level.receivers.size(), false);
    if (level.emitters.empty()) return segments;

    const int maxBounces = 12;
    const float maxRange = 80.0f;
    const float receiverHeightTolerance = 0.6f;

    for (const auto& emitter : level.emitters) {
        if (emitter.subworld != -1 && emitter.subworld != subworld) continue; // emettitore di un altro sub-mondo: spento
        Vector3 pos = emitter.position;
        Vector3 dir = DirFromAngleDeg(emitter.angleDeg);

        for (int bounce = 0; bounce <= maxBounces; bounce++) {
            float bestT = maxRange;
            enum class Hit { None, Mirror, Block, Receiver } hitType = Hit::None;
            int hitIndex = -1;
            Vector3 mirrorNormal{ 0, 0, 0 };

            for (size_t m = 0; m < level.mirrors.size(); m++) {
                const auto& mir = level.mirrors[m];
                if (mir.subworld != -1 && mir.subworld != subworld) continue; // specchio di un altro sub-mondo: il raggio lo attraversa
                if (fabsf(pos.y - mir.position.y) > mir.height / 2.0f) continue;
                Vector3 mDir = DirFromAngleDeg(mir.angleDeg);
                Vector3 half = Vector3Scale(mDir, mir.length / 2.0f);
                Vector3 segA = Vector3Subtract(mir.position, half);
                Vector3 segB = Vector3Add(mir.position, half);
                float t = RaySegmentHitXZ(pos, dir, segA, segB);
                if (t > 0.01f && t < bestT) {
                    bestT = t;
                    hitType = Hit::Mirror;
                    hitIndex = (int)m;
                    mirrorNormal = Vector3{ mDir.z, 0, -mDir.x };
                }
            }

            for (const auto& o : level.obstacles) {
                if (o.subworld != -1 && o.subworld != subworld) continue;
                if (pos.y < o.position.y - o.size.y / 2.0f || pos.y > o.position.y + o.size.y / 2.0f) continue;
                float t = RayBoxHitXZ(pos, dir,
                                       o.position.x - o.size.x / 2.0f, o.position.x + o.size.x / 2.0f,
                                       o.position.z - o.size.z / 2.0f, o.position.z + o.size.z / 2.0f);
                if (t > 0.01f && t < bestT) { bestT = t; hitType = Hit::Block; hitIndex = -1; }
            }

            // Le casse trascinabili bloccano il raggio nella loro posizione
            // ATTUALE (il giocatore le puo' spingere), non in quella di
            // partenza salvata nel livello.
            for (size_t c = 0; c < level.draggables.size() && c < draggablePositions.size(); c++) {
                if (level.draggables[c].subworld != -1 && level.draggables[c].subworld != subworld) continue;
                Vector3 cp = draggablePositions[c];
                Vector3 cs = level.draggables[c].size;
                if (pos.y < cp.y - cs.y / 2.0f || pos.y > cp.y + cs.y / 2.0f) continue;
                float t = RayBoxHitXZ(pos, dir,
                                       cp.x - cs.x / 2.0f, cp.x + cs.x / 2.0f,
                                       cp.z - cs.z / 2.0f, cp.z + cs.z / 2.0f);
                if (t > 0.01f && t < bestT) { bestT = t; hitType = Hit::Block; hitIndex = -1; }
            }

            for (size_t d = 0; d < level.doors.size(); d++) {
                if (level.doors[d].subworld != -1 && level.doors[d].subworld != subworld) continue;
                // Altezza ANIMATA attuale della porta (quella che si vede rimpicciolire
                // mentre sprofonda nel pavimento), non quella statica configurata: cosi'
                // il raggio passa sopra la porta non appena questa e' scesa sotto la sua
                // quota, invece di restare bloccato finche' non e' quasi del tutto aperta.
                float h = (d < doorHeights.size()) ? doorHeights[d] : level.doors[d].size.y;
                if (h <= 0.1f) continue; // porta aperta: non blocca
                const auto& door = level.doors[d];
                Vector3 sz = GetDoorEffectiveSize(door);
                if (pos.y > door.position.y + h) continue;
                float t = RayBoxHitXZ(pos, dir,
                                       door.position.x - sz.x / 2.0f, door.position.x + sz.x / 2.0f,
                                       door.position.z - sz.z / 2.0f, door.position.z + sz.z / 2.0f);
                if (t > 0.01f && t < bestT) { bestT = t; hitType = Hit::Block; hitIndex = -1; }
            }

            for (size_t r = 0; r < level.receivers.size(); r++) {
                const auto& rec = level.receivers[r];
                if (rec.subworld != -1 && rec.subworld != subworld) continue; // ricevitore di un altro sub-mondo: il raggio lo attraversa
                if (fabsf(pos.y - rec.position.y) > receiverHeightTolerance) continue;
                float t = RayCircleHitXZ(pos, dir, rec.position, rec.radius);
                if (t > 0.01f && t < bestT) { bestT = t; hitType = Hit::Receiver; hitIndex = (int)r; }
            }

            Vector3 endPoint = Vector3Add(pos, Vector3Scale(dir, bestT));
            segments.push_back(LightBeamSegment{ pos, endPoint, emitter.color });

            if (hitType == Hit::Receiver) {
                const auto& rec = level.receivers[hitIndex];
                bool colorOk = (rec.color.r == WHITE.r && rec.color.g == WHITE.g && rec.color.b == WHITE.b) ||
                               (rec.color.r == emitter.color.r && rec.color.g == emitter.color.g && rec.color.b == emitter.color.b);
                if (colorOk) receiverLit[hitIndex] = true;
                break;
            }
            if (hitType == Hit::Mirror) {
                float d = Vector3DotProduct(dir, mirrorNormal);
                dir = Vector3Subtract(dir, Vector3Scale(mirrorNormal, 2.0f * d));
                pos = endPoint;
                continue;
            }
            break; // Hit::Block oppure Hit::None (fine del raggio massimo)
        }
    }

    return segments;
}