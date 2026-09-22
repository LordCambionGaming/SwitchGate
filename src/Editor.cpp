#include "Editor.h"
#include "UI.h"
#include "Lighting.h"
#include "Rendering.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cctype>

static const std::vector<std::pair<const char*, Color>> kPalette = {
    { "Rosso", RED }, { "Blu", BLUE }, { "Verde", GREEN }, { "Giallo", YELLOW },
    { "Arancione", ORANGE }, { "Viola", PURPLE }, { "Rosa", PINK }, { "Oro", GOLD },
    { "Lime", LIME }, { "Azzurro", SKYBLUE }, { "Grigio chiaro", LIGHTGRAY },
    { "Grigio", GRAY }, { "Grigio scuro", DARKGRAY }, { "Marrone", BROWN },
};

static bool ColorsEq(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static std::string ColorDisplayName(int paletteIndex) {
    std::string name = kPalette[paletteIndex].first;
    for (auto& c : name) c = (char)toupper((unsigned char)c);
    return name;
}

static float WrapAngle(float deg) {
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return deg;
}


// Camera 3D "a orbita" e conversioni mondo <-> schermo


void LevelEditor::UpdateCameraFromOrbit() {
    float yawRad = camYaw * DEG2RAD;
    float pitchRad = camPitch * DEG2RAD;
    Vector3 offset = {
        camDistance * cosf(pitchRad) * sinf(yawRad),
        camDistance * sinf(pitchRad),
        camDistance * cosf(pitchRad) * cosf(yawRad)
    };
    camera.position = Vector3Add(camTarget, offset);
    camera.target = camTarget;
    camera.up = Vector3{ 0, 1, 0 };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void LevelEditor::UpdateCameraControls() {
    Vector2 mouse = GetMousePosition();
    bool inCanvas = MouseInCanvas();

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && inCanvas) { isOrbiting = true; lastCamMouse = mouse; }
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) && inCanvas) { isPanning = true; lastCamMouse = mouse; }

    if (isOrbiting && IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        Vector2 delta = { mouse.x - lastCamMouse.x, mouse.y - lastCamMouse.y };
        camYaw -= delta.x * 0.3f;
        camPitch = Clamp(camPitch + delta.y * 0.3f, 8.0f, 85.0f);
        lastCamMouse = mouse;
    }
    if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) isOrbiting = false;

    if (isPanning && IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 delta = { mouse.x - lastCamMouse.x, mouse.y - lastCamMouse.y };
        float yawRad = camYaw * DEG2RAD;
        // Stessa convenzione angolare di DirFromAngleDeg (Lighting.h): 0 = +Z.
        Vector3 right = { cosf(yawRad), 0, -sinf(yawRad) };
        Vector3 fwd = { sinf(yawRad), 0, cosf(yawRad) };
        float panSpeed = camDistance * 0.0015f;
        camTarget = Vector3Subtract(camTarget, Vector3Scale(right, delta.x * panSpeed));
        camTarget = Vector3Add(camTarget, Vector3Scale(fwd, delta.y * panSpeed));
        lastCamMouse = mouse;
    }
    if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) isPanning = false;

    if (inCanvas) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) camDistance = Clamp(camDistance - wheel * 2.0f, 6.0f, 80.0f);
    }

    UpdateCameraFromOrbit();
}

Vector2 LevelEditor::ScreenToWorld(Vector2 screen) const {
    Ray ray = GetMouseRay(screen, camera);
    if (fabsf(ray.direction.y) < 1e-5f) return Vector2{ camTarget.x, camTarget.z };
    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) t = 0.0f;
    return Vector2{ ray.position.x + ray.direction.x * t, ray.position.z + ray.direction.z * t };
}

Vector2 LevelEditor::WorldToScreen(Vector3 worldPos) const {
    return GetWorldToScreen(worldPos, camera);
}

bool LevelEditor::MouseInCanvas() const {
    return CheckCollisionPointRec(GetMousePosition(), canvasRect);
}


// Gestione stato


void LevelEditor::NewLevel() {
    working = LevelData{};
    working.name = "Nuovo livello";
    working.description = "";
    working.playerStart = Vector3{ 0.0f, 1.0f, 8.0f };
    working.gravityEnabled = true;
    working.randomSequence = true;
    working.switches.clear();
    working.platforms.clear();
    working.platforms.push_back(LevelBox{ Vector3{ 0, -0.25f, 0 }, Vector3{ 20, 0.5f, 20 }, LIGHTGRAY });
    working.obstacles.clear();
    working.mirrors.clear();
    working.emitters.clear();
    working.receivers.clear();
    working.doors.clear();

    LevelDoor defaultDoor;
    defaultDoor.position = Vector3{ 0, 0, -9 };
    defaultDoor.size = Vector3{ 4, 3, 0.5f };
    defaultDoor.color = DARKBROWN;
    defaultDoor.logicOp = "OR";
    working.doors.push_back(defaultDoor);

    working.exitPosition = Vector3{ 0, 0, -11 };
    working.exitRadius = 1.5f;
    working.fallResetY = -8.0f;
    currentFilePath.clear();
    ClearSelection();
    tool = EditorTool::SELECT;
    camYaw = 35.0f; camPitch = 55.0f; camDistance = 30.0f; camTarget = Vector3{ 0, 0, 0 };
    SetStatus("Nuovo livello creato.", false);
}

void LevelEditor::LoadLevel(const LevelData& lvl, const std::string& filePath) {
    working = lvl;
    currentFilePath = filePath;
    ClearSelection();
    tool = EditorTool::SELECT;
    camYaw = 35.0f; camPitch = 55.0f; camDistance = 30.0f;
    camTarget = Vector3{ working.playerStart.x, 0, working.playerStart.z };
    SetStatus("Livello caricato: " + filePath, false);
}

void LevelEditor::SetStatus(const std::string& msg, bool isError) {
    statusMessage = msg;
    statusIsError = isError;
    statusTimer = 4.0f;
}

void LevelEditor::ClearSelection() {
    selType = EditorSelType::NONE;
    selIndex = -1;
    isDraggingSel = false;
}

void LevelEditor::PickAt(Vector2 w) {
    for (int i = (int)working.switches.size() - 1; i >= 0; i--) {
        const auto& s = working.switches[i];
        float dx = s.position.x - w.x, dz = s.position.z - w.y;
        if (dx * dx + dz * dz <= 1.0f) { selType = EditorSelType::SWITCH; selIndex = i; return; }
    }
    for (int i = (int)working.emitters.size() - 1; i >= 0; i--) {
        const auto& e = working.emitters[i];
        float dx = e.position.x - w.x, dz = e.position.z - w.y;
        if (dx * dx + dz * dz <= 0.6f * 0.6f) { selType = EditorSelType::EMITTER; selIndex = i; return; }
    }
    for (int i = (int)working.receivers.size() - 1; i >= 0; i--) {
        const auto& r = working.receivers[i];
        float dx = r.position.x - w.x, dz = r.position.z - w.y;
        float rad = std::max(0.6f, r.radius);
        if (dx * dx + dz * dz <= rad * rad) { selType = EditorSelType::RECEIVER; selIndex = i; return; }
    }
    for (int i = (int)working.mirrors.size() - 1; i >= 0; i--) {
        const auto& m = working.mirrors[i];
        float dx = m.position.x - w.x, dz = m.position.z - w.y;
        float rad = std::max(0.8f, m.length / 2.0f);
        if (dx * dx + dz * dz <= rad * rad) { selType = EditorSelType::MIRROR; selIndex = i; return; }
    }
    {
        float dx = working.playerStart.x - w.x, dz = working.playerStart.z - w.y;
        if (dx * dx + dz * dz <= 1.0f) { selType = EditorSelType::START; selIndex = -1; return; }
    }
    {
        float dx = working.exitPosition.x - w.x, dz = working.exitPosition.z - w.y;
        float r = working.exitRadius;
        if (dx * dx + dz * dz <= r * r) { selType = EditorSelType::EXIT; selIndex = -1; return; }
    }
    for (int i = (int)working.doors.size() - 1; i >= 0; i--) {
        const auto& d = working.doors[i];
        Vector3 eff = GetDoorEffectiveSize(d);
        float halfX = eff.x / 2.0f, halfZ = std::max(0.6f, eff.z / 2.0f);
        if (fabsf(d.position.x - w.x) <= halfX && fabsf(d.position.z - w.y) <= halfZ) {
            selType = EditorSelType::DOOR; selIndex = i; return;
        }
    }
    for (int i = (int)working.pads.size() - 1; i >= 0; i--) {
        const auto& p = working.pads[i];
        if (fabsf(p.position.x - w.x) <= p.size.x / 2.0f && fabsf(p.position.z - w.y) <= p.size.z / 2.0f) {
            selType = EditorSelType::PAD; selIndex = i; return;
        }
    }
    for (int i = (int)working.draggables.size() - 1; i >= 0; i--) {
        const auto& d = working.draggables[i];
        if (fabsf(d.position.x - w.x) <= d.size.x / 2.0f && fabsf(d.position.z - w.y) <= d.size.z / 2.0f) {
            selType = EditorSelType::DRAGGABLE; selIndex = i; return;
        }
    }
    for (int i = (int)working.obstacles.size() - 1; i >= 0; i--) {
        const auto& o = working.obstacles[i];
        if (fabsf(o.position.x - w.x) <= o.size.x / 2.0f && fabsf(o.position.z - w.y) <= o.size.z / 2.0f) {
            selType = EditorSelType::OBSTACLE; selIndex = i; return;
        }
    }
    for (int i = (int)working.platforms.size() - 1; i >= 0; i--) {
        const auto& p = working.platforms[i];
        if (fabsf(p.position.x - w.x) <= p.size.x / 2.0f && fabsf(p.position.z - w.y) <= p.size.z / 2.0f) {
            selType = EditorSelType::PLATFORM; selIndex = i; return;
        }
    }
    ClearSelection();
}

void LevelEditor::DeleteSelected() {
    if (selType == EditorSelType::PLATFORM && selIndex >= 0 && selIndex < (int)working.platforms.size()) {
        working.platforms.erase(working.platforms.begin() + selIndex);
    } else if (selType == EditorSelType::OBSTACLE && selIndex >= 0 && selIndex < (int)working.obstacles.size()) {
        working.obstacles.erase(working.obstacles.begin() + selIndex);
    } else if (selType == EditorSelType::DRAGGABLE && selIndex >= 0 && selIndex < (int)working.draggables.size()) {
        working.draggables.erase(working.draggables.begin() + selIndex);
    } else if (selType == EditorSelType::PAD && selIndex >= 0 && selIndex < (int)working.pads.size()) {
        working.pads.erase(working.pads.begin() + selIndex);
    } else if (selType == EditorSelType::MIRROR && selIndex >= 0 && selIndex < (int)working.mirrors.size()) {
        working.mirrors.erase(working.mirrors.begin() + selIndex);
    } else if (selType == EditorSelType::EMITTER && selIndex >= 0 && selIndex < (int)working.emitters.size()) {
        working.emitters.erase(working.emitters.begin() + selIndex);
    } else if (selType == EditorSelType::RECEIVER && selIndex >= 0 && selIndex < (int)working.receivers.size()) {
        working.receivers.erase(working.receivers.begin() + selIndex);
    } else if (selType == EditorSelType::DOOR && selIndex >= 0 && selIndex < (int)working.doors.size()) {
        working.doors.erase(working.doors.begin() + selIndex);
        for (auto& pad : working.pads) {
            if (pad.linkedDoor == selIndex) pad.linkedDoor = -1;
            else if (pad.linkedDoor > selIndex) pad.linkedDoor--;
        }
        for (auto& rec : working.receivers) {
            if (rec.linkedDoor == selIndex) rec.linkedDoor = -1;
            else if (rec.linkedDoor > selIndex) rec.linkedDoor--;
        }
    } else if (selType == EditorSelType::SWITCH && selIndex >= 0 && selIndex < (int)working.switches.size()) {
        if (working.switches.size() <= 1) { SetStatus("Deve rimanere almeno un interruttore.", true); return; }
        working.switches.erase(working.switches.begin() + selIndex);
        for (auto& door : working.doors) {
            if (door.linkedSwitch == selIndex) door.linkedSwitch = -1;
            else if (door.linkedSwitch > selIndex) door.linkedSwitch--;

            for (auto it = door.linkedSwitches.begin(); it != door.linkedSwitches.end(); ) {
                if (*it == selIndex) {
                    it = door.linkedSwitches.erase(it);
                } else {
                    if (*it > selIndex) (*it)--;
                    ++it;
                }
            }
        }
        for (size_t k = 0; k < working.fixedSequence.size(); ) {
            if (working.fixedSequence[k] == selIndex) {
                working.fixedSequence.erase(working.fixedSequence.begin() + k);
                continue;
            }
            if (working.fixedSequence[k] > selIndex) working.fixedSequence[k]--;
            k++;
        }
    } else {
        SetStatus("Questo oggetto non si puo' eliminare.", true);
        return;
    }
    ClearSelection();
}

bool LevelEditor::ConsumePlaytestRequest() {
    bool v = playtestRequested;
    playtestRequested = false;
    return v;
}

bool LevelEditor::ConsumeExitRequest() {
    bool v = exitRequested;
    exitRequested = false;
    return v;
}


// Update: interazione con la scena 3D (orbita camera, creare/spostare/selezionare oggetti)


void LevelEditor::Update() {
    if (statusTimer > 0.0f) statusTimer -= GetFrameTime();

    if (showSaveBox || showLoadBox) return;

    UpdateCameraControls();

    Vector2 mouseScreen = GetMousePosition();
    bool inCanvas = MouseInCanvas();
    Vector2 mw = ScreenToWorld(mouseScreen);

    if (tool == EditorTool::SELECT) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            PickAt(mw);
            if (selType != EditorSelType::NONE) {
                isDraggingSel = true;
                float ox = 0, oz = 0;
                switch (selType) {
                    case EditorSelType::SWITCH:
                        if (selIndex >= 0) { ox = working.switches[selIndex].position.x; oz = working.switches[selIndex].position.z; }
                        break;
                    case EditorSelType::START: ox = working.playerStart.x; oz = working.playerStart.z; break;
                    case EditorSelType::EXIT: ox = working.exitPosition.x; oz = working.exitPosition.z; break;
                    case EditorSelType::DOOR:
                        if (selIndex >= 0) { ox = working.doors[selIndex].position.x; oz = working.doors[selIndex].position.z; }
                        break;
                    case EditorSelType::PLATFORM:
                        if (selIndex >= 0) { ox = working.platforms[selIndex].position.x; oz = working.platforms[selIndex].position.z; }
                        break;
                    case EditorSelType::OBSTACLE:
                        if (selIndex >= 0) { ox = working.obstacles[selIndex].position.x; oz = working.obstacles[selIndex].position.z; }
                        break;
                    case EditorSelType::DRAGGABLE:
                        if (selIndex >= 0) { ox = working.draggables[selIndex].position.x; oz = working.draggables[selIndex].position.z; }
                        break;
                    case EditorSelType::PAD:
                        if (selIndex >= 0) { ox = working.pads[selIndex].position.x; oz = working.pads[selIndex].position.z; }
                        break;
                    case EditorSelType::MIRROR:
                        if (selIndex >= 0) { ox = working.mirrors[selIndex].position.x; oz = working.mirrors[selIndex].position.z; }
                        break;
                    case EditorSelType::EMITTER:
                        if (selIndex >= 0) { ox = working.emitters[selIndex].position.x; oz = working.emitters[selIndex].position.z; }
                        break;
                    case EditorSelType::RECEIVER:
                        if (selIndex >= 0) { ox = working.receivers[selIndex].position.x; oz = working.receivers[selIndex].position.z; }
                        break;
                    default: break;
                }
                dragOffsetWorld = { ox - mw.x, oz - mw.y };
            }
        }
        if (isDraggingSel && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float nx = Clamp(mw.x + dragOffsetWorld.x, -24.0f, 24.0f);
            float nz = Clamp(mw.y + dragOffsetWorld.y, -24.0f, 24.0f);
            switch (selType) {
                case EditorSelType::SWITCH:
                    if (selIndex >= 0) { working.switches[selIndex].position.x = nx; working.switches[selIndex].position.z = nz; }
                    break;
                case EditorSelType::START: working.playerStart.x = nx; working.playerStart.z = nz; break;
                case EditorSelType::EXIT: working.exitPosition.x = nx; working.exitPosition.z = nz; break;
                case EditorSelType::DOOR:
                    if (selIndex >= 0) { working.doors[selIndex].position.x = nx; working.doors[selIndex].position.z = nz; }
                    break;
                case EditorSelType::PLATFORM:
                    if (selIndex >= 0) { working.platforms[selIndex].position.x = nx; working.platforms[selIndex].position.z = nz; }
                    break;
                case EditorSelType::OBSTACLE:
                    if (selIndex >= 0) { working.obstacles[selIndex].position.x = nx; working.obstacles[selIndex].position.z = nz; }
                    break;
                case EditorSelType::DRAGGABLE:
                    if (selIndex >= 0) { working.draggables[selIndex].position.x = nx; working.draggables[selIndex].position.z = nz; }
                    break;
                case EditorSelType::PAD:
                    if (selIndex >= 0) { working.pads[selIndex].position.x = nx; working.pads[selIndex].position.z = nz; }
                    break;
                case EditorSelType::MIRROR:
                    if (selIndex >= 0) { working.mirrors[selIndex].position.x = nx; working.mirrors[selIndex].position.z = nz; }
                    break;
                case EditorSelType::EMITTER:
                    if (selIndex >= 0) { working.emitters[selIndex].position.x = nx; working.emitters[selIndex].position.z = nz; }
                    break;
                case EditorSelType::RECEIVER:
                    if (selIndex >= 0) { working.receivers[selIndex].position.x = nx; working.receivers[selIndex].position.z = nz; }
                    break;
                default: break;
            }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) isDraggingSel = false;

        if (IsKeyPressed(KEY_DELETE) && !nameActive && !descActive && !switchNameActive) {
            DeleteSelected();
        }
    }
    else if (tool == EditorTool::PLATFORM || tool == EditorTool::OBSTACLE || tool == EditorTool::DRAGGABLE) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isDraggingNew = true;
            dragStartWorld = mw;
            dragStartedInCanvas = inCanvas;
        }
        if (isDraggingNew && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            isDraggingNew = false;
            if (dragStartedInCanvas || inCanvas) {
                Vector2 endW = mw;
                float minX = std::min(dragStartWorld.x, endW.x), maxX = std::max(dragStartWorld.x, endW.x);
                float minZ = std::min(dragStartWorld.y, endW.y), maxZ = std::max(dragStartWorld.y, endW.y);
                float sizeX = maxX - minX, sizeZ = maxZ - minZ;
                if (sizeX < 1.0f) { float cx = (minX + maxX) / 2.0f; minX = cx - 1.0f; maxX = cx + 1.0f; sizeX = 2.0f; }
                if (sizeZ < 1.0f) { float cz = (minZ + maxZ) / 2.0f; minZ = cz - 1.0f; maxZ = cz + 1.0f; sizeZ = 2.0f; }

                LevelBox box;
                box.position.x = (minX + maxX) / 2.0f;
                box.position.z = (minZ + maxZ) / 2.0f;
                box.size.x = sizeX;
                box.size.z = sizeZ;
                box.color = kPalette[colorIndex].second;

                if (tool == EditorTool::PLATFORM) {
                    box.size.y = 0.5f;
                    box.position.y = newPlatformTopY - 0.25f;
                    working.platforms.push_back(box);
                } else if (tool == EditorTool::OBSTACLE) {
                    box.size.y = newObstacleHeight;
                    box.position.y = newObstacleHeight / 2.0f;
                    working.obstacles.push_back(box);
                } else {
                    box.size.y = std::min(sizeX, sizeZ);
                    box.position.y = newPlatformTopY + box.size.y / 2.0f;
                    working.draggables.push_back(box);
                }
            }
        }
    }
    else if (tool == EditorTool::SWITCH) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            LevelSwitch sw;
            sw.position = Vector3{ mw.x, newPlatformTopY + 0.5f, mw.y };
            sw.color = kPalette[colorIndex].second;
            sw.name = ColorDisplayName(colorIndex);
            working.switches.push_back(sw);
            working.fixedSequence.push_back((int)working.switches.size() - 1);
            selType = EditorSelType::SWITCH;
            selIndex = (int)working.switches.size() - 1;
            tool = EditorTool::SELECT;
        }
    }
    else if (tool == EditorTool::START) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            working.playerStart.x = mw.x;
            working.playerStart.z = mw.y;
            working.playerStart.y = newPlatformTopY + 1.0f;
        }
    }
    else if (tool == EditorTool::DOOR) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            LevelDoor door;
            door.position = Vector3{ mw.x, 0, mw.y };
            door.size = Vector3{ 4, 3, 0.5f };
            door.color = DARKBROWN;
            door.linkedSwitch = -1;
            door.logicOp = "OR";
            working.doors.push_back(door);
            selType = EditorSelType::DOOR;
            selIndex = (int)working.doors.size() - 1;
            tool = EditorTool::SELECT;
        }
    }
    else if (tool == EditorTool::PAD) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            LevelPad pad;
            pad.position = Vector3{ mw.x, newPlatformTopY + 0.05f, mw.y };
            pad.size = Vector3{ 1.5f, 0.1f, 1.5f };
            pad.color = kPalette[colorIndex].second;
            pad.linkedDoor = -1;
            working.pads.push_back(pad);
            selType = EditorSelType::PAD;
            selIndex = (int)working.pads.size() - 1;
            tool = EditorTool::SELECT;
        }
    }
    else if (tool == EditorTool::MIRROR) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            LevelMirror mir;
            mir.position = Vector3{ mw.x, newPlatformTopY + 1.0f, mw.y };
            mir.angleDeg = newAngleDeg;
            mir.length = 2.0f;
            mir.height = 2.0f;
            mir.color = kPalette[colorIndex].second;
            working.mirrors.push_back(mir);
            selType = EditorSelType::MIRROR;
            selIndex = (int)working.mirrors.size() - 1;
            tool = EditorTool::SELECT;
        }
    }
    else if (tool == EditorTool::EMITTER) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            LevelEmitter em;
            em.position = Vector3{ mw.x, newPlatformTopY + 1.0f, mw.y };
            em.angleDeg = newAngleDeg;
            em.color = kPalette[colorIndex].second;
            working.emitters.push_back(em);
            selType = EditorSelType::EMITTER;
            selIndex = (int)working.emitters.size() - 1;
            tool = EditorTool::SELECT;
        }
    }
    else if (tool == EditorTool::RECEIVER) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            LevelReceiver rec;
            rec.position = Vector3{ mw.x, newPlatformTopY + 1.0f, mw.y };
            rec.radius = 0.4f;
            rec.color = kPalette[colorIndex].second;
            rec.linkedDoor = -1;
            working.receivers.push_back(rec);
            selType = EditorSelType::RECEIVER;
            selIndex = (int)working.receivers.size() - 1;
            tool = EditorTool::SELECT;
        }
    }
    else if (tool == EditorTool::EXIT) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inCanvas) {
            working.exitPosition.x = mw.x;
            working.exitPosition.z = mw.y;
        }
    }
}


// Disegno


void LevelEditor::DrawTopBar() {
    DrawRectangle(0, 0, 1280, 96, Fade(DARKBLUE, 0.08f));
    DrawText("EDITOR LIVELLI", 20, 14, 26, DARKBLUE);

    Rectangle newBtn = { 20, 54, 110, 34 };
    Rectangle loadBtn = { 140, 54, 110, 34 };
    Rectangle saveBtn = { 260, 54, 110, 34 };
    Rectangle testBtn = { 380, 54, 160, 34 };
    Rectangle backBtn = { 1280 - 190.0f, 54, 170, 34 };

    if (DrawButton(newBtn, "NUOVO", 16, DARKGRAY, GRAY, WHITE)) NewLevel();
    if (DrawButton(loadBtn, "CARICA", 16, DARKBLUE, BLUE, WHITE)) {
        loadBrowser.ScanDirectory("levels");
        showLoadBox = true;
    }
    if (DrawButton(saveBtn, "SALVA", 16, DARKGREEN, GREEN, WHITE)) {
        if (!currentFilePath.empty()) {
            std::string err;
            if (LevelManager::SaveToFile(currentFilePath, working, err)) SetStatus("Livello salvato in " + currentFilePath, false);
            else SetStatus(err, true);
        } else {
            fileNameBuffer = working.name;
            for (auto& c : fileNameBuffer) if (!isalnum((unsigned char)c)) c = '_';
            if (fileNameBuffer.empty()) fileNameBuffer = "livello_nuovo";
            showSaveBox = true;
        }
    }
    if (DrawButton(testBtn, "PROVA LIVELLO", 15, GOLD, YELLOW, BLACK)) {
        playtestRequested = true;
    }
    if (DrawButton(backBtn, "MENU PRINCIPALE", 14, MAROON, RED, WHITE)) exitRequested = true;
}

void LevelEditor::DrawSidebar() {
    const float sidebarTop = 104.0f;
    const float sidebarBottom = 648.0f;
    Rectangle sidebarVisibleRect = { 0, sidebarTop, 240, sidebarBottom - sidebarTop };

    if (CheckCollisionPointRec(GetMousePosition(), sidebarVisibleRect)) {
        sidebarScroll -= GetMouseWheelMove() * 30.0f;
    }
    if (sidebarScroll < 0.0f) sidebarScroll = 0.0f;

    g_uiScrollOffsetY = sidebarScroll;
    BeginScissorMode((int)sidebarVisibleRect.x, (int)sidebarVisibleRect.y, (int)sidebarVisibleRect.width, (int)sidebarVisibleRect.height);
    rlPushMatrix();
    rlTranslatef(0, -sidebarScroll, 0);

    float x = 10, w = 220, y = sidebarTop;

    DrawText("STRUMENTI", (int)x, (int)y, 16, DARKBLUE); y += 22;

    struct ToolBtn { EditorTool t; const char* label; };
    ToolBtn tools[] = {
        { EditorTool::SELECT, "Seleziona / Sposta" },
        { EditorTool::PLATFORM, "Piattaforma" },
        { EditorTool::OBSTACLE, "Ostacolo (muro)" },
        { EditorTool::DRAGGABLE, "Cassa trascinabile" },
        { EditorTool::SWITCH, "Interruttore" },
        { EditorTool::START, "Partenza" },
        { EditorTool::DOOR, "Porta" },
        { EditorTool::PAD, "Pedana a colore" },
        { EditorTool::MIRROR, "Specchio" },
        { EditorTool::EMITTER, "Emettitore di luce" },
        { EditorTool::RECEIVER, "Ricevitore di luce" },
        { EditorTool::EXIT, "Uscita" },
    };
    for (auto& tb : tools) {
        Rectangle r = { x, y, w, 28 };
        bool active = (tool == tb.t);
        if (DrawButton(r, tb.label, 14, active ? DARKGREEN : Fade(LIGHTGRAY, 0.9f), active ? GREEN : Fade(SKYBLUE, 0.7f), active ? WHITE : BLACK)) {
            tool = tb.t;
            ClearSelection();
        }
        y += 31;
    }

    y += 6;
    DrawText("ALTEZZA PIATTAFORMA / PARTENZA", (int)x, (int)y, 11, DARKGRAY); y += 15;
    {
        Rectangle m = { x, y, 28, 26 }, p = { x + w - 28, y, 28, 26 };
        if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) newPlatformTopY -= 0.5f;
        if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) newPlatformTopY += 0.5f;
        std::string val = TextFormat("%.1f", newPlatformTopY);
        DrawText(val.c_str(), (int)(x + w / 2 - MeasureText(val.c_str(), 18) / 2), (int)y + 3, 18, BLACK);
        y += 32;
    }

    DrawText("ALTEZZA MURO OSTACOLO", (int)x, (int)y, 11, DARKGRAY); y += 15;
    {
        Rectangle m = { x, y, 28, 26 }, p = { x + w - 28, y, 28, 26 };
        if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) newObstacleHeight = std::max(0.5f, newObstacleHeight - 0.5f);
        if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) newObstacleHeight += 0.5f;
        std::string val = TextFormat("%.1f", newObstacleHeight);
        DrawText(val.c_str(), (int)(x + w / 2 - MeasureText(val.c_str(), 18) / 2), (int)y + 3, 18, BLACK);
        y += 32;
    }

    DrawText("ANGOLO (specchi/emettitori)", (int)x, (int)y, 11, DARKGRAY); y += 15;
    {
        Rectangle m = { x, y, 28, 26 }, p = { x + w - 28, y, 28, 26 };
        if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) newAngleDeg = WrapAngle(newAngleDeg - 15.0f);
        if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) newAngleDeg = WrapAngle(newAngleDeg + 15.0f);
        std::string val = TextFormat("%.0f gradi", newAngleDeg);
        DrawText(val.c_str(), (int)(x + w / 2 - MeasureText(val.c_str(), 16) / 2), (int)y + 4, 16, BLACK);
        y += 32;
    }

    DrawText("COLORE (nuovi oggetti)", (int)x, (int)y, 11, DARKGRAY); y += 15;
    {
        Rectangle prevBtn = { x, y, 28, 26 }, swatch = { x + 32, y, w - 92, 26 }, nextBtn = { x + w - 28, y, 28, 26 };
        if (DrawMiniButton(prevBtn, "<", LIGHTGRAY, GRAY)) colorIndex = (colorIndex - 1 + (int)kPalette.size()) % (int)kPalette.size();
        DrawRectangleRec(swatch, kPalette[colorIndex].second);
        DrawRectangleLinesEx(swatch, 1, BLACK);
        if (DrawMiniButton(nextBtn, ">", LIGHTGRAY, GRAY)) colorIndex = (colorIndex + 1) % (int)kPalette.size();
        y += 30;
        DrawText(kPalette[colorIndex].first, (int)x, (int)y, 13, DARKGRAY);
        y += 20;
    }

    y += 4;
    DrawText("NOME LIVELLO", (int)x, (int)y, 11, DARKGRAY); y += 15;
    { Rectangle r = { x, y, w, 26 }; TextBoxUpdate(r, working.name, nameActive, 40, "Nome del livello"); y += 32; }

    DrawText("DESCRIZIONE", (int)x, (int)y, 11, DARKGRAY); y += 15;
    { Rectangle r = { x, y, w, 26 }; TextBoxUpdate(r, working.description, descActive, 80, "Breve descrizione"); y += 32; }

    y += 2;
    {
        Rectangle r = { x, y, w, 27 };
        std::string label = std::string("Gravita': ") + (working.gravityEnabled ? "ON" : "OFF");
        if (DrawButton(r, label.c_str(), 13, working.gravityEnabled ? DARKGREEN : DARKGRAY, working.gravityEnabled ? GREEN : GRAY, WHITE))
            working.gravityEnabled = !working.gravityEnabled;
        y += 31;
    }
    {
        Rectangle r = { x, y, w, 27 };
        std::string label = std::string("Sequenza: ") + (working.randomSequence ? "CASUALE" : "FISSA");
        if (DrawButton(r, label.c_str(), 13, working.randomSequence ? DARKBLUE : DARKGRAY, working.randomSequence ? BLUE : GRAY, WHITE))
            working.randomSequence = !working.randomSequence;
        y += 31;
    }

    if (!working.randomSequence) {
        if (working.fixedSequence.size() != working.switches.size()) {
            working.fixedSequence.resize(working.switches.size());
            for (size_t k = 0; k < working.switches.size(); k++) working.fixedSequence[k] = (int)k;
        }
        DrawText("Ordine sequenza fissa:", (int)x, (int)y, 11, DARKGRAY); y += 16;
        for (size_t k = 0; k < working.fixedSequence.size(); k++) {
            int swIdx = working.fixedSequence[k];
            std::string rowLabel = std::to_string(k + 1) + ". " +
                ((swIdx >= 0 && swIdx < (int)working.switches.size()) ? working.switches[swIdx].name : "?");
            Rectangle rowRect = { x, y, w - 56, 24 };
            DrawRectangleRec(rowRect, Fade(LIGHTGRAY, 0.4f));
            DrawText(rowLabel.c_str(), (int)x + 4, (int)y + 4, 14, BLACK);
            Rectangle up = { x + w - 52, y, 24, 24 }, down = { x + w - 26, y, 24, 24 };
            if (DrawMiniButton(up, "^", LIGHTGRAY, GRAY) && k > 0) {
                std::swap(working.fixedSequence[k], working.fixedSequence[k - 1]);
            }
            if (DrawMiniButton(down, "v", LIGHTGRAY, GRAY) && k + 1 < working.fixedSequence.size()) {
                std::swap(working.fixedSequence[k], working.fixedSequence[k + 1]);
            }
            y += 27;
        }
    }

    y += 4;
    if (selType != EditorSelType::NONE) {
        DrawRectangle((int)x, (int)y, (int)w, 2, Fade(BLACK, 0.2f)); y += 10;
        DrawText("OGGETTO SELEZIONATO", (int)x, (int)y, 13, DARKBLUE); y += 18;

        if (selType == EditorSelType::SWITCH && selIndex >= 0 && selIndex < (int)working.switches.size()) {
            LevelSwitch& sw = working.switches[selIndex];
            DrawText("Nome (segue il colore):", (int)x, (int)y, 11, DARKGRAY); y += 16;
            DrawText(sw.name.c_str(), (int)x, (int)y, 18, BLACK); y += 26;

            int curIdx = 0;
            for (size_t i = 0; i < kPalette.size(); i++) if (ColorsEq(kPalette[i].second, sw.color)) { curIdx = (int)i; break; }
            Rectangle prevBtn = { x, y, 28, 24 }, swatch = { x + 32, y, w - 92, 24 }, nextBtn = { x + w - 28, y, 28, 24 };
            if (DrawMiniButton(prevBtn, "<", LIGHTGRAY, GRAY)) {
                curIdx = (curIdx - 1 + (int)kPalette.size()) % (int)kPalette.size();
                sw.color = kPalette[curIdx].second;
                sw.name = ColorDisplayName(curIdx);
            }
            DrawRectangleRec(swatch, sw.color); DrawRectangleLinesEx(swatch, 1, BLACK);
            if (DrawMiniButton(nextBtn, ">", LIGHTGRAY, GRAY)) {
                curIdx = (curIdx + 1) % (int)kPalette.size();
                sw.color = kPalette[curIdx].second;
                sw.name = ColorDisplayName(curIdx);
            }
            y += 30;

            Rectangle del = { x, y, w, 26 };
            if (DrawButton(del, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
            y += 30;
        }
        else if ((selType == EditorSelType::PLATFORM || selType == EditorSelType::OBSTACLE || selType == EditorSelType::DRAGGABLE)) {
            std::vector<LevelBox>& list = (selType == EditorSelType::PLATFORM) ? working.platforms
                : (selType == EditorSelType::OBSTACLE) ? working.obstacles : working.draggables;
            if (selIndex >= 0 && selIndex < (int)list.size()) {
                LevelBox& box = list[selIndex];

                DrawText("Larghezza (X):", (int)x, (int)y, 11, DARKGRAY); y += 14;
                { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
                  if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) box.size.x = std::max(0.5f, box.size.x - 0.5f);
                  if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) box.size.x += 0.5f;
                  std::string s = TextFormat("%.1f", box.size.x);
                  DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
                  y += 28; }

                DrawText("Profondita' (Z):", (int)x, (int)y, 11, DARKGRAY); y += 14;
                { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
                  if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) box.size.z = std::max(0.5f, box.size.z - 0.5f);
                  if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) box.size.z += 0.5f;
                  std::string s = TextFormat("%.1f", box.size.z);
                  DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
                  y += 28; }

                DrawText("Altezza:", (int)x, (int)y, 11, DARKGRAY); y += 14;
                { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
                  if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) box.position.y -= 0.5f;
                  if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) box.position.y += 0.5f;
                  float topY = box.position.y + box.size.y / 2.0f;
                  std::string s = TextFormat("%.1f", topY);
                  DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
                  y += 28; }

                int curIdx = 0;
                for (size_t i = 0; i < kPalette.size(); i++) if (ColorsEq(kPalette[i].second, box.color)) { curIdx = (int)i; break; }
                Rectangle prevBtn = { x, y, 28, 24 }, swatch = { x + 32, y, w - 92, 24 }, nextBtn = { x + w - 28, y, 28, 24 };
                if (DrawMiniButton(prevBtn, "<", LIGHTGRAY, GRAY)) { curIdx = (curIdx - 1 + (int)kPalette.size()) % (int)kPalette.size(); box.color = kPalette[curIdx].second; }
                DrawRectangleRec(swatch, box.color); DrawRectangleLinesEx(swatch, 1, BLACK);
                if (DrawMiniButton(nextBtn, ">", LIGHTGRAY, GRAY)) { curIdx = (curIdx + 1) % (int)kPalette.size(); box.color = kPalette[curIdx].second; }
                y += 30;

                Rectangle del = { x, y, w, 26 };
                if (DrawButton(del, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
                y += 30;
            }
        }
        else if (selType == EditorSelType::DOOR && selIndex >= 0 && selIndex < (int)working.doors.size()) {
            LevelDoor& door = working.doors[selIndex];
            DrawText("Larghezza porta:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) door.size.x = std::max(1.0f, door.size.x - 0.5f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) door.size.x += 0.5f;
              std::string s = TextFormat("%.1f", door.size.x);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Altezza porta:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) door.size.y = std::max(1.0f, door.size.y - 0.5f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) door.size.y += 0.5f;
              std::string s = TextFormat("%.1f", door.size.y);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Logica porta (AND / OR):", (int)x, (int)y, 11, DARKGRAY); y += 14;
            {
                Rectangle logicBtn = { x, y, w, 26 };
                std::string logicLabel = "Operatore: " + door.logicOp;
                if (DrawButton(logicBtn, logicLabel.c_str(), 13, DARKBLUE, BLUE, WHITE)) {
                    door.logicOp = (door.logicOp == "AND") ? "OR" : "AND";
                }
                y += 30;
            }

            DrawText("Interruttori collegati (multi-select):", (int)x, (int)y, 11, DARKGRAY); y += 16;
            if (working.switches.empty()) {
                DrawText("Nessun interruttore nel livello.", (int)x, (int)y, 12, GRAY); y += 22;
            } else {
                for (size_t sIdx = 0; sIdx < working.switches.size(); sIdx++) {
                    auto it = std::find(door.linkedSwitches.begin(), door.linkedSwitches.end(), (int)sIdx);
                    bool isLinked = (it != door.linkedSwitches.end());

                    std::string swLabel = std::to_string(sIdx + 1) + ". " + working.switches[sIdx].name;
                    Rectangle swBtn = { x, y, w, 24 };
                    if (DrawButton(swBtn, swLabel.c_str(), 12, isLinked ? DARKGREEN : LIGHTGRAY, isLinked ? GREEN : GRAY, isLinked ? WHITE : BLACK)) {
                        if (isLinked) {
                            door.linkedSwitches.erase(it);
                        } else {
                            door.linkedSwitches.push_back((int)sIdx);
                        }
                    }
                    y += 27;
                }
            }

            {
                Rectangle rotBtn = { x, y, w, 26 };
                std::string rotLabel = std::string("Orientamento: ") + (door.rotated ? "Z (ruotata)" : "X (normale)");
                if (DrawButton(rotBtn, rotLabel.c_str(), 13, DARKBLUE, BLUE, WHITE)) door.rotated = !door.rotated;
                y += 30;
            }

            Rectangle del = { x, y, w, 26 };
            if (DrawButton(del, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
            y += 30;
        }
        else if (selType == EditorSelType::PAD && selIndex >= 0 && selIndex < (int)working.pads.size()) {
            LevelPad& pad = working.pads[selIndex];
            DrawText("Colore pedana (deve combaciare con una cassa):", (int)x, (int)y, 10, DARKGRAY); y += 16;
            {
                int curIdx = 0;
                for (size_t i = 0; i < kPalette.size(); i++) if (ColorsEq(kPalette[i].second, pad.color)) { curIdx = (int)i; break; }
                Rectangle prevBtn = { x, y, 28, 24 }, swatch = { x + 32, y, w - 92, 24 }, nextBtn = { x + w - 28, y, 28, 24 };
                if (DrawMiniButton(prevBtn, "<", LIGHTGRAY, GRAY)) { curIdx = (curIdx - 1 + (int)kPalette.size()) % (int)kPalette.size(); pad.color = kPalette[curIdx].second; }
                DrawRectangleRec(swatch, pad.color); DrawRectangleLinesEx(swatch, 1, BLACK);
                if (DrawMiniButton(nextBtn, ">", LIGHTGRAY, GRAY)) { curIdx = (curIdx + 1) % (int)kPalette.size(); pad.color = kPalette[curIdx].second; }
                y += 30;
            }

            DrawText("Apre la porta:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            {
                std::string label = (pad.linkedDoor < 0 || pad.linkedDoor >= (int)working.doors.size())
                    ? "Nessuna"
                    : ("Porta " + std::to_string(pad.linkedDoor + 1));
                Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
                int n = (int)working.doors.size();
                if (DrawMiniButton(m, "<", LIGHTGRAY, GRAY)) {
                    pad.linkedDoor = (pad.linkedDoor <= -1) ? (n - 1) : (pad.linkedDoor - 1);
                }
                DrawText(label.c_str(), (int)(x + w / 2 - MeasureText(label.c_str(), 13) / 2), (int)y + 5, 13, BLACK);
                if (DrawMiniButton(p, ">", LIGHTGRAY, GRAY)) {
                    pad.linkedDoor = (pad.linkedDoor >= n - 1) ? -1 : (pad.linkedDoor + 1);
                }
                y += 28;
            }

            Rectangle del2 = { x, y, w, 26 };
            if (DrawButton(del2, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
            y += 30;
        }
        else if (selType == EditorSelType::MIRROR && selIndex >= 0 && selIndex < (int)working.mirrors.size()) {
            LevelMirror& mir = working.mirrors[selIndex];
            DrawText("Angolo pannello:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) mir.angleDeg = WrapAngle(mir.angleDeg - 15.0f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) mir.angleDeg = WrapAngle(mir.angleDeg + 15.0f);
              std::string s = TextFormat("%.0f gradi", mir.angleDeg);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 15) / 2), (int)y + 4, 15, BLACK);
              y += 28; }

            DrawText("Larghezza pannello:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) mir.length = std::max(0.5f, mir.length - 0.5f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) mir.length += 0.5f;
              std::string s = TextFormat("%.1f", mir.length);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Altezza pannello:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) mir.height = std::max(0.5f, mir.height - 0.5f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) mir.height += 0.5f;
              std::string s = TextFormat("%.1f", mir.height);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Quota (Y):", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) mir.position.y -= 0.5f;
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) mir.position.y += 0.5f;
              std::string s = TextFormat("%.1f", mir.position.y);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            Rectangle del = { x, y, w, 26 };
            if (DrawButton(del, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
            y += 30;
        }
        else if (selType == EditorSelType::EMITTER && selIndex >= 0 && selIndex < (int)working.emitters.size()) {
            LevelEmitter& em = working.emitters[selIndex];
            DrawText("Direzione del raggio:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) em.angleDeg = WrapAngle(em.angleDeg - 15.0f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) em.angleDeg = WrapAngle(em.angleDeg + 15.0f);
              std::string s = TextFormat("%.0f gradi", em.angleDeg);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 15) / 2), (int)y + 4, 15, BLACK);
              y += 28; }

            DrawText("Quota (Y):", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) em.position.y -= 0.5f;
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) em.position.y += 0.5f;
              std::string s = TextFormat("%.1f", em.position.y);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Colore del raggio:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            {
                int curIdx = 0;
                for (size_t i = 0; i < kPalette.size(); i++) if (ColorsEq(kPalette[i].second, em.color)) { curIdx = (int)i; break; }
                Rectangle prevBtn = { x, y, 28, 24 }, swatch = { x + 32, y, w - 92, 24 }, nextBtn = { x + w - 28, y, 28, 24 };
                if (DrawMiniButton(prevBtn, "<", LIGHTGRAY, GRAY)) { curIdx = (curIdx - 1 + (int)kPalette.size()) % (int)kPalette.size(); em.color = kPalette[curIdx].second; }
                DrawRectangleRec(swatch, em.color); DrawRectangleLinesEx(swatch, 1, BLACK);
                if (DrawMiniButton(nextBtn, ">", LIGHTGRAY, GRAY)) { curIdx = (curIdx + 1) % (int)kPalette.size(); em.color = kPalette[curIdx].second; }
                y += 30;
            }

            Rectangle del = { x, y, w, 26 };
            if (DrawButton(del, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
            y += 30;
        }
        else if (selType == EditorSelType::RECEIVER && selIndex >= 0 && selIndex < (int)working.receivers.size()) {
            LevelReceiver& rec = working.receivers[selIndex];
            DrawText("Raggio ricevitore:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) rec.radius = std::max(0.15f, rec.radius - 0.1f);
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) rec.radius += 0.1f;
              std::string s = TextFormat("%.2f", rec.radius);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Quota (Y):", (int)x, (int)y, 11, DARKGRAY); y += 14;
            { Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
              if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) rec.position.y -= 0.5f;
              if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) rec.position.y += 0.5f;
              std::string s = TextFormat("%.1f", rec.position.y);
              DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
              y += 28; }

            DrawText("Colore richiesto (bianco = qualsiasi):", (int)x, (int)y, 10, DARKGRAY); y += 16;
            {
                int curIdx = 0;
                for (size_t i = 0; i < kPalette.size(); i++) if (ColorsEq(kPalette[i].second, rec.color)) { curIdx = (int)i; break; }
                Rectangle prevBtn = { x, y, 28, 24 }, swatch = { x + 32, y, w - 92, 24 }, nextBtn = { x + w - 28, y, 28, 24 };
                if (DrawMiniButton(prevBtn, "<", LIGHTGRAY, GRAY)) { curIdx = (curIdx - 1 + (int)kPalette.size()) % (int)kPalette.size(); rec.color = kPalette[curIdx].second; }
                DrawRectangleRec(swatch, rec.color); DrawRectangleLinesEx(swatch, 1, BLACK);
                if (DrawMiniButton(nextBtn, ">", LIGHTGRAY, GRAY)) { curIdx = (curIdx + 1) % (int)kPalette.size(); rec.color = kPalette[curIdx].second; }
                y += 30;
            }

            DrawText("Apre la porta:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            {
                std::string label = (rec.linkedDoor < 0 || rec.linkedDoor >= (int)working.doors.size())
                    ? "Nessuna"
                    : ("Porta " + std::to_string(rec.linkedDoor + 1));
                Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
                int n = (int)working.doors.size();
                if (DrawMiniButton(m, "<", LIGHTGRAY, GRAY)) {
                    rec.linkedDoor = (rec.linkedDoor <= -1) ? (n - 1) : (rec.linkedDoor - 1);
                }
                DrawText(label.c_str(), (int)(x + w / 2 - MeasureText(label.c_str(), 13) / 2), (int)y + 5, 13, BLACK);
                if (DrawMiniButton(p, ">", LIGHTGRAY, GRAY)) {
                    rec.linkedDoor = (rec.linkedDoor >= n - 1) ? -1 : (rec.linkedDoor + 1);
                }
                y += 28;
            }

            Rectangle del = { x, y, w, 26 };
            if (DrawButton(del, "ELIMINA", 13, MAROON, RED, WHITE)) DeleteSelected();
            y += 30;
        }
        else if (selType == EditorSelType::EXIT) {
            DrawText("Raggio uscita:", (int)x, (int)y, 11, DARKGRAY); y += 14;
            Rectangle m = { x, y, 28, 24 }, p = { x + w - 28, y, 28, 24 };
            if (DrawMiniButton(m, "-", LIGHTGRAY, GRAY)) working.exitRadius = std::max(0.5f, working.exitRadius - 0.25f);
            if (DrawMiniButton(p, "+", LIGHTGRAY, GRAY)) working.exitRadius += 0.25f;
            std::string s = TextFormat("%.2f", working.exitRadius);
            DrawText(s.c_str(), (int)(x + w / 2 - MeasureText(s.c_str(), 16) / 2), (int)y + 3, 16, BLACK);
            y += 28;
        }
        else if (selType == EditorSelType::START) {
            std::string s = TextFormat("Partenza: (%.1f, %.1f, %.1f)", working.playerStart.x, working.playerStart.y, working.playerStart.z);
            DrawText(s.c_str(), (int)x, (int)y, 11, DARKGRAY);
            y += 18;
        }
    }

    y += 10;
    float contentHeight = y - sidebarTop;

    rlPopMatrix();
    EndScissorMode();
    g_uiScrollOffsetY = 0.0f;

    float maxScroll = std::max(0.0f, contentHeight - (sidebarBottom - sidebarTop));
    if (sidebarScroll > maxScroll) sidebarScroll = maxScroll;

    if (maxScroll > 0.0f) {
        Rectangle track = { 232, sidebarTop, 5, sidebarBottom - sidebarTop };
        DrawRectangleRec(track, Fade(LIGHTGRAY, 0.6f));
        float thumbH = std::max(20.0f, track.height * (track.height / contentHeight));
        float thumbY = track.y + (track.height - thumbH) * (sidebarScroll / maxScroll);
        DrawRectangleRec(Rectangle{ track.x, thumbY, track.width, thumbH }, DARKGRAY);
    }

    DrawText("Sinistro: crea/sposta.  Destro: ruota vista.", (int)x, 656, 10, GRAY);
    DrawText("Centrale: sposta vista.  Rotella: zoom.", (int)x, 670, 10, GRAY);
    DrawText("CANC: elimina l'oggetto selezionato.", (int)x, 684, 10, GRAY);
}

void LevelEditor::DrawCanvas() {
    DrawRectangleRec(canvasRect, Fade(SKYBLUE, 0.12f));
    DrawRectangleLinesEx(canvasRect, 2, DARKGRAY);

    BeginScissorMode((int)canvasRect.x, (int)canvasRect.y, (int)canvasRect.width, (int)canvasRect.height);

    BeginMode3D(camera);
        DrawGrid(48, 1.0f);

        SceneRenderState rs = MakeIdleSceneState(working);
        rs.beams = ComputeLightBeams(working, rs.doorHeights, rs.draggablePositions, rs.receiverLit);
        DrawLevelScene(working, rs, camera, nullptr);

        // Contorno dorato sull'oggetto selezionato
        switch (selType) {
            case EditorSelType::PLATFORM:
                if (selIndex >= 0 && selIndex < (int)working.platforms.size()) {
                    auto& b = working.platforms[selIndex];
                    DrawCubeWires(b.position, b.size.x + 0.06f, b.size.y + 0.06f, b.size.z + 0.06f, GOLD);
                }
                break;
            case EditorSelType::OBSTACLE:
                if (selIndex >= 0 && selIndex < (int)working.obstacles.size()) {
                    auto& b = working.obstacles[selIndex];
                    DrawCubeWires(b.position, b.size.x + 0.06f, b.size.y + 0.06f, b.size.z + 0.06f, GOLD);
                }
                break;
            case EditorSelType::DRAGGABLE:
                if (selIndex >= 0 && selIndex < (int)working.draggables.size()) {
                    auto& b = working.draggables[selIndex];
                    DrawCubeWires(b.position, b.size.x + 0.06f, b.size.y + 0.06f, b.size.z + 0.06f, GOLD);
                }
                break;
            case EditorSelType::SWITCH:
                if (selIndex >= 0 && selIndex < (int)working.switches.size())
                    DrawCubeWires(working.switches[selIndex].position, 1.1f, 1.1f, 1.1f, GOLD);
                break;
            case EditorSelType::DOOR:
                if (selIndex >= 0 && selIndex < (int)working.doors.size()) {
                    auto& d = working.doors[selIndex];
                    Vector3 eff = GetDoorEffectiveSize(d);
                    Vector3 c = { d.position.x, eff.y / 2.0f, d.position.z };
                    DrawCubeWires(c, eff.x + 0.1f, eff.y + 0.1f, eff.z + 0.1f, GOLD);
                }
                break;
            case EditorSelType::PAD:
                if (selIndex >= 0 && selIndex < (int)working.pads.size()) {
                    auto& p = working.pads[selIndex];
                    DrawCubeWires(p.position, p.size.x + 0.06f, p.size.y + 0.06f, p.size.z + 0.06f, GOLD);
                }
                break;
            case EditorSelType::MIRROR:
                if (selIndex >= 0 && selIndex < (int)working.mirrors.size())
                    DrawSphereWires(working.mirrors[selIndex].position, 0.45f, 8, 8, GOLD);
                break;
            case EditorSelType::EMITTER:
                if (selIndex >= 0 && selIndex < (int)working.emitters.size())
                    DrawSphereWires(working.emitters[selIndex].position, 0.4f, 8, 8, GOLD);
                break;
            case EditorSelType::RECEIVER:
                if (selIndex >= 0 && selIndex < (int)working.receivers.size())
                    DrawSphereWires(working.receivers[selIndex].position, working.receivers[selIndex].radius + 0.15f, 8, 8, GOLD);
                break;
            case EditorSelType::START:
                DrawSphereWires(working.playerStart, 0.6f, 8, 8, GOLD);
                break;
            case EditorSelType::EXIT:
                DrawCircle3D(working.exitPosition, working.exitRadius + 0.15f, Vector3{ 1, 0, 0 }, 90.0f, GOLD);
                break;
            default: break;
        }

        // Anteprima del box mentre si trascina per crearne uno nuovo
        if (isDraggingNew && (tool == EditorTool::PLATFORM || tool == EditorTool::OBSTACLE || tool == EditorTool::DRAGGABLE)) {
            Vector2 cur = ScreenToWorld(GetMousePosition());
            float minX = std::min(dragStartWorld.x, cur.x), maxX = std::max(dragStartWorld.x, cur.x);
            float minZ = std::min(dragStartWorld.y, cur.y), maxZ = std::max(dragStartWorld.y, cur.y);
            float cx = (minX + maxX) / 2.0f, cz = (minZ + maxZ) / 2.0f;
            float sx = std::max(0.2f, maxX - minX), sz = std::max(0.2f, maxZ - minZ);
            float h = (tool == EditorTool::OBSTACLE) ? newObstacleHeight : 0.5f;
            Vector3 c = { cx, newPlatformTopY + (tool == EditorTool::OBSTACLE ? h / 2.0f : -0.25f), cz };
            Color previewColor = tool == EditorTool::PLATFORM ? DARKGREEN : (tool == EditorTool::OBSTACLE ? MAROON : ORANGE);
            DrawCubeWires(c, sx, h, sz, previewColor);
        }
    EndMode3D();

    // Etichette 2D sovrapposte (proiettate dalla scena 3D)
    for (size_t i = 0; i < working.switches.size(); i++) {
        Vector2 sp = WorldToScreen(Vector3Add(working.switches[i].position, Vector3{ 0, 0.9f, 0 }));
        const char* label = working.switches[i].name.c_str();
        DrawText(label, (int)sp.x - MeasureText(label, 14) / 2, (int)sp.y - 16, 14, BLACK);
    }
    {
        Vector2 sp = WorldToScreen(Vector3Add(working.playerStart, Vector3{ 0, 1.1f, 0 }));
        DrawText("START", (int)sp.x - 22, (int)sp.y - 16, 13, MAROON);
    }
    for (size_t i = 0; i < working.doors.size(); i++) {
        const auto& d = working.doors[i];
        Vector2 sp = WorldToScreen(Vector3{ d.position.x, d.size.y + 0.3f, d.position.z });
        std::string info = "P" + std::to_string(i + 1) + " [" + d.logicOp + "]";
        DrawText(info.c_str(), (int)sp.x - MeasureText(info.c_str(), 13) / 2, (int)sp.y - 14, 13, DARKBLUE);
    }
    for (size_t i = 0; i < working.pads.size(); i++) {
        const auto& p = working.pads[i];
        if (p.linkedDoor >= 0 && p.linkedDoor < (int)working.doors.size()) {
            Vector2 sp = WorldToScreen(Vector3Add(p.position, Vector3{ 0, 0.4f, 0 }));
            std::string info = "->P" + std::to_string(p.linkedDoor + 1);
            DrawText(info.c_str(), (int)sp.x - MeasureText(info.c_str(), 12) / 2, (int)sp.y - 12, 12, DARKBLUE);
        }
    }
    for (size_t i = 0; i < working.emitters.size(); i++) {
        Vector2 sp = WorldToScreen(Vector3Add(working.emitters[i].position, Vector3{ 0, 0.45f, 0 }));
        DrawText("EMIT", (int)sp.x - 15, (int)sp.y - 14, 11, BLACK);
    }
    for (size_t i = 0; i < working.receivers.size(); i++) {
        const auto& rec = working.receivers[i];
        Vector2 sp = WorldToScreen(Vector3Add(rec.position, Vector3{ 0, rec.radius + 0.35f, 0 }));
        std::string label = "RICEV";
        if (rec.linkedDoor >= 0 && rec.linkedDoor < (int)working.doors.size()) label += " ->P" + std::to_string(rec.linkedDoor + 1);
        DrawText(label.c_str(), (int)sp.x - MeasureText(label.c_str(), 11) / 2, (int)sp.y - 12, 11, DARKBLUE);
    }
    for (size_t i = 0; i < working.mirrors.size(); i++) {
        const auto& mir = working.mirrors[i];
        Vector2 sp = WorldToScreen(Vector3Add(mir.position, Vector3{ 0, mir.height / 2.0f + 0.25f, 0 }));
        std::string label = TextFormat("%.0f gradi", mir.angleDeg);
        DrawText(label.c_str(), (int)sp.x - MeasureText(label.c_str(), 11) / 2, (int)sp.y - 12, 11, DARKGRAY);
    }

    EndScissorMode();
}

void LevelEditor::DrawSaveDialog() {
    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.5f));
    Rectangle box = { 1280 / 2.0f - 260, 720 / 2.0f - 90, 520, 180 };
    DrawRectangleRounded(box, 0.05f, 8, RAYWHITE);
    DrawRectangleLinesEx(box, 2.0f, DARKGRAY);
    DrawText("Salva livello come:", (int)box.x + 20, (int)box.y + 16, 20, DARKBLUE);

    Rectangle tb = { box.x + 20, box.y + 56, box.width - 100, 32 };
    TextBoxUpdate(tb, fileNameBuffer, fileNameActive, 40, "nome_file");
    DrawText(".json", (int)(tb.x + tb.width + 6), (int)(tb.y + 6), 18, DARKGRAY);

    Rectangle confirm = { box.x + 20, box.y + box.height - 50, 220, 38 };
    Rectangle cancel = { box.x + box.width - 240, box.y + box.height - 50, 220, 38 };
    if (DrawButton(confirm, "SALVA", 18, DARKGREEN, GREEN, WHITE)) {
        std::string clean;
        for (char c : fileNameBuffer) clean += (isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
        if (clean.empty()) clean = "livello";
        std::string path = "levels/" + clean + ".json";
        std::string err;
        if (LevelManager::SaveToFile(path, working, err)) {
            currentFilePath = path;
            SetStatus("Livello salvato in " + path, false);
            showSaveBox = false;
        } else {
            SetStatus(err, true);
        }
    }
    if (DrawButton(cancel, "ANNULLA", 18, DARKGRAY, GRAY, WHITE)) showSaveBox = false;
}

void LevelEditor::DrawLoadDialog() {
    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.5f));
    Rectangle box = { 1280 / 2.0f - 320, 60, 640, 600 };
    DrawRectangleRounded(box, 0.03f, 8, RAYWHITE);
    DrawRectangleLinesEx(box, 2.0f, DARKGRAY);
    DrawText("Carica un livello esistente", (int)box.x + 20, (int)box.y + 16, 22, DARKBLUE);

    const auto& levels = loadBrowser.GetLevels();
    float ly = box.y + 60;
    for (size_t i = 0; i < levels.size(); i++) {
        Rectangle r = { box.x + 20, ly, box.width - 40, 46 };
        if (DrawButton(r, levels[i].name.c_str(), 18, Fade(LIGHTGRAY, 0.9f), Fade(SKYBLUE, 0.9f), BLACK)) {
            LoadLevel(levels[i], levels[i].filePath);
            showLoadBox = false;
        }
        ly += 54;
    }
    if (levels.empty()) {
        DrawText("Nessun livello trovato nella cartella 'levels'.", (int)box.x + 20, (int)ly + 10, 16, MAROON);
    }

    Rectangle cancel = { box.x + box.width / 2 - 100, box.y + box.height - 56, 200, 40 };
    if (DrawButton(cancel, "ANNULLA", 18, DARKGRAY, GRAY, WHITE)) showLoadBox = false;
}

void LevelEditor::Draw() {
    UpdateCameraFromOrbit();
    DrawTopBar();
    DrawCanvas();
    DrawSidebar();

    if (statusTimer > 0.0f && !statusMessage.empty()) {
        DrawText(statusMessage.c_str(), 240, 80, 16, statusIsError ? MAROON : DARKGREEN);
    }

    if (showSaveBox) DrawSaveDialog();
    if (showLoadBox) DrawLoadDialog();
}
