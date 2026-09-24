#include "Rendering.h"
#include "raymath.h"

// Vero se l'oggetto va disegnato/e' tangibile nel sub-mondo attualmente
// mostrato: objSubworld -1 = oggetto condiviso (sempre presente), stateSubworld
// -1 = si sta mostrando "tutto" (modalita' editor), altrimenti devono combaciare.
static bool InActiveSubworld(int objSubworld, int stateSubworld) {
    return stateSubworld == -1 || objSubworld == -1 || objSubworld == stateSubworld;
}

bool IsVisibleToCamera(const Camera3D& camera, Vector3 objPos, float objRadius) {
    Vector3 toObj = Vector3Subtract(objPos, camera.position);
    float dist = Vector3Length(toObj);
    if (dist > RENDER_DISTANCE + objRadius) return false;
    if (dist < 0.001f) return true;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 dirToObj = Vector3Scale(toObj, 1.0f / dist);
    float cosAngle = Vector3DotProduct(forward, dirToObj);

    // Margine generoso oltre il campo visivo verticale: copre anche
    // l'estensione orizzontale (piu' ampia, essendo lo schermo 16:9) e la
    // dimensione dell'oggetto stesso, cosi' non sparisce troppo presto ai
    // bordi dello schermo.
    float halfFovRad = (camera.fovy * 0.5f + 25.0f) * DEG2RAD;
    float angleMargin = atanf(objRadius / dist);
    return cosAngle > cosf(halfFovRad + angleMargin);
}

SceneRenderState MakeIdleSceneState(const LevelData& level) {
    SceneRenderState s;
    s.doorHeights.reserve(level.doors.size());
    for (const auto& d : level.doors) s.doorHeights.push_back(d.size.y);
    s.switchActivated.assign(level.switches.size(), false);
    s.padPressed.assign(level.pads.size(), false);
    s.receiverLit.assign(level.receivers.size(), false);
    s.draggablePositions.reserve(level.draggables.size());
    for (const auto& d : level.draggables) s.draggablePositions.push_back(d.position);
    return s;
}

void DrawLevelScene(const LevelData& level, const SceneRenderState& state, const Camera3D& camera, const Model* crateModel) {
    for (const auto& p : level.platforms) {
        if (!InActiveSubworld(p.subworld, state.subworld)) continue;
        float radius = Vector3Length(Vector3Scale(p.size, 0.5f));
        if (!IsVisibleToCamera(camera, p.position, radius)) continue;
        DrawCube(p.position, p.size.x, p.size.y, p.size.z, p.color);
        DrawCubeWires(p.position, p.size.x, p.size.y, p.size.z, Fade(BLACK, 0.25f));
    }
    for (const auto& o : level.obstacles) {
        if (!InActiveSubworld(o.subworld, state.subworld)) continue;
        float radius = Vector3Length(Vector3Scale(o.size, 0.5f));
        if (!IsVisibleToCamera(camera, o.position, radius)) continue;
        DrawCube(o.position, o.size.x, o.size.y, o.size.z, o.color);
        DrawCubeWires(o.position, o.size.x, o.size.y, o.size.z, DARKGRAY);
    }
    for (size_t i = 0; i < level.draggables.size(); i++) {
        if (!InActiveSubworld(level.draggables[i].subworld, state.subworld)) continue;
        Vector3 dp = (i < state.draggablePositions.size()) ? state.draggablePositions[i] : level.draggables[i].position;
        Vector3 ds = level.draggables[i].size;
        float radius = Vector3Length(Vector3Scale(ds, 0.5f));
        if (!IsVisibleToCamera(camera, dp, radius)) continue;
        if (crateModel) {
            DrawModelEx(*crateModel, dp, Vector3{ 0, 1, 0 }, 0.0f, ds, level.draggables[i].color);
        } else {
            DrawCube(dp, ds.x, ds.y, ds.z, level.draggables[i].color);
        }
        DrawCubeWires(dp, ds.x, ds.y, ds.z, BLACK);
    }

    for (size_t i = 0; i < level.pads.size(); i++) {
        const LevelPad& pad = level.pads[i];
        if (!InActiveSubworld(pad.subworld, state.subworld)) continue;
        if (!IsVisibleToCamera(camera, pad.position, pad.size.x)) continue;
        bool pressed = (i < state.padPressed.size()) && state.padPressed[i];
        Color c = pressed ? pad.color : Fade(pad.color, 0.45f);
        DrawCube(pad.position, pad.size.x, pad.size.y, pad.size.z, c);
        DrawCubeWires(pad.position, pad.size.x, pad.size.y, pad.size.z, DARKGRAY);
    }

    for (size_t i = 0; i < level.switches.size(); i++) {
        const auto& s = level.switches[i];
        if (!InActiveSubworld(s.subworld, state.subworld)) continue;
        if (!IsVisibleToCamera(camera, s.position, 0.87f)) continue;
        bool activated = (i < state.switchActivated.size()) && state.switchActivated[i];
        Color c = activated ? s.color : Fade(s.color, 0.4f);
        DrawCube(s.position, 1, 1, 1, c);
        DrawCubeWires(s.position, 1, 1, 1, DARKGRAY);
    }

    for (size_t i = 0; i < level.doors.size(); i++) {
        const auto& door = level.doors[i];
        if (!InActiveSubworld(door.subworld, state.subworld)) continue;
        float h = (i < state.doorHeights.size()) ? state.doorHeights[i] : 0.0f;
        if (h <= 0.01f) continue;
        Vector3 effSize = GetDoorEffectiveSize(door);
        // door.position.y e' la base (il "pavimento") della porta: la parte
        // ancora visibile (che si ritira verso il basso mentre apre) parte da
        // li' e si estende in alto di h. Prima si ignorava sempre
        // door.position.y e si partiva da 0: spostare la porta in quota con
        // il gizmo non aveva alcun effetto visibile.
        Vector3 dp = { door.position.x, door.position.y + h / 2.0f, door.position.z };
        DrawCube(dp, effSize.x, h, effSize.z, door.color);
        DrawCubeWires(dp, effSize.x, h, effSize.z, BLACK);
    }

    for (const auto& mir : level.mirrors) {
        if (!InActiveSubworld(mir.subworld, state.subworld)) continue;
        if (!IsVisibleToCamera(camera, mir.position, mir.length)) continue;
        Vector3 mDir = DirFromAngleDeg(mir.angleDeg);
        // Pannello sottile orientato lungo mDir: DrawCubeV non supporta la
        // rotazione, quindi lo disegniamo "a mano" come due triangoli.
        Vector3 half = Vector3Scale(mDir, mir.length / 2.0f);
        Vector3 up = Vector3{ 0, mir.height / 2.0f, 0 };
        Vector3 a = Vector3Subtract(Vector3Subtract(mir.position, half), up);
        Vector3 b = Vector3Add(Vector3Subtract(mir.position, half), up);
        Vector3 c = Vector3Add(Vector3Add(mir.position, half), up);
        Vector3 d = Vector3Subtract(Vector3Add(mir.position, half), up);
        DrawTriangle3D(a, b, c, mir.color);
        DrawTriangle3D(a, c, d, mir.color);
        DrawTriangle3D(a, c, b, mir.color);
        DrawTriangle3D(a, d, c, mir.color);
        DrawLine3D(a, b, DARKGRAY); DrawLine3D(b, c, DARKGRAY);
        DrawLine3D(c, d, DARKGRAY); DrawLine3D(d, a, DARKGRAY);
    }

    for (const auto& em : level.emitters) {
        if (!InActiveSubworld(em.subworld, state.subworld)) continue;
        if (!IsVisibleToCamera(camera, em.position, 0.5f)) continue;
        DrawSphere(em.position, 0.25f, em.color);
        DrawSphereWires(em.position, 0.25f, 6, 6, BLACK);
    }

    for (size_t i = 0; i < level.receivers.size(); i++) {
        const auto& rec = level.receivers[i];
        if (!InActiveSubworld(rec.subworld, state.subworld)) continue;
        if (!IsVisibleToCamera(camera, rec.position, rec.radius + 0.3f)) continue;
        bool lit = (i < state.receiverLit.size()) && state.receiverLit[i];
        Color c = lit ? rec.color : Fade(rec.color, 0.35f);
        DrawSphere(rec.position, rec.radius, c);
        DrawSphereWires(rec.position, rec.radius, 8, 8, DARKGRAY);
    }

    // Il "nucleo" del fascio e' un sottile cilindro pieno (si vede bene da
    // qualunque angolo, a differenza di una linea); il bagliore attorno e'
    // un secondo cilindro piu' largo e trasparente.
    for (const auto& seg : state.beams) {
        if (Vector3DistanceSqr(seg.a, seg.b) < 0.0001f) continue;
        DrawCylinderEx(seg.a, seg.b, 0.12f, 0.12f, 6, Fade(seg.color, 0.25f));
        DrawCylinderEx(seg.a, seg.b, 0.035f, 0.035f, 6, seg.color);
    }

    DrawCircle3D(level.exitPosition, level.exitRadius, Vector3{ 1, 0, 0 }, 90.0f,
                  state.exitOpen ? GREEN : GRAY);

    if (state.showPlayer) {
        DrawSphere(state.playerPosition, state.playerRadius, ORANGE);
        DrawSphereWires(state.playerPosition, state.playerRadius, 8, 8, MAROON);
    }
}