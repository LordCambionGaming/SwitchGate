#include "raylib.h"
#include "raymath.h"
#include "Level.h"
#include "Config.h"
#include "Scores.h"
#include "Editor.h"
#include "UI.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <algorithm>
#include <random>

#if defined(_WIN32) && defined(NDEBUG)
// Nelle build Release su Windows, nasconde la finestra della console nera
// che altrimenti si apre insieme alla finestra di gioco.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

// Stato globale dell'applicazione


enum class GameState { MENU, LEVEL_SELECT, PLAYING, HELP, SETTINGS, EDITOR };

struct Player {
    Vector3 position{ 0, 1.0f, 8 };
    Vector3 velocity{ 0, 0, 0 };
    float radius = 0.5f;
    bool onGround = false;
};

static const float GRAVITY = -24.0f;
static const float JUMP_SPEED = 9.0f;
static const float MOVE_SPEED = 6.0f;
static const float RENDER_DISTANCE = 60.0f;

// True se un oggetto (approssimato a una sfera) puo' ricadere nel campo visivo
// della camera: usato per saltare del tutto la chiamata DrawCube per tutto
// cio' che e' troppo lontano o fuori dal cono di vista, invece di mandarlo
// alla scheda video e farlo scartare li'.
static bool IsVisibleToCamera(const Camera3D& camera, Vector3 objPos, float objRadius) {
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

// Fisica di base


// Trova la quota della superficie di appoggio piu' alta sotto il giocatore.
// Se ritorna false, non c'e' nessuna piattaforma sotto: il giocatore sta cadendo.
static bool FindGroundY(const LevelData& level, float x, float z, float& outY) {
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

// Spinge il giocatore fuori dagli ostacoli solidi (trattati come muri a tutta
// altezza): semplice risoluzione per assi separati (x poi z).
static void ResolveBoxCollision(Vector3& pos, float radius, Vector3 boxPos, Vector3 boxSize) {
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

static void ResolveObstacles(Vector3& pos, float radius, const std::vector<LevelBox>& obstacles) {
    for (const auto& o : obstacles) ResolveBoxCollision(pos, radius, o.position, o.size);
}

// Una porta blocca il passaggio solo mentre non e' (quasi) del tutto aperta;
// l'altezza attuale viene dall'animazione della partita in corso, non dal
// dato statico del livello.
static void ResolveDoors(Vector3& pos, float radius, const std::vector<LevelDoor>& doors, const std::vector<float>& doorHeights) {
    for (size_t i = 0; i < doors.size(); i++) {
        if (i < doorHeights.size() && doorHeights[i] <= 0.1f) continue;
        ResolveBoxCollision(pos, radius, doors[i].position, GetDoorEffectiveSize(doors[i]));
    }
}

static void UpdatePlayerPhysics(Player& player, const LevelData& level, const std::vector<float>& doorHeights, Vector3 moveInput, float dt, bool jumpPressed) {
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


// Stato di una partita in corso (indipendente dal livello, si resetta a ogni R)


struct RunState {
    std::vector<bool> activated;
    std::vector<int> sequence;
    int currentStep = 0;
    bool solved = false;
    bool won = false;
    std::vector<float> doorHeights;
    float flashTimer = 0.0f;
    bool flashWrong = false;
    Player player;
    float elapsedTime = 0.0f;
    int score = 0;
    bool isNewRecord = false;
    float cameraYaw = 0.0f;
    float targetCameraYaw = 0.0f;

    std::vector<Vector3> draggablePos;
    std::vector<Vector3> draggableVel;
    std::vector<bool> padPressed;
};

static void StartRun(RunState& run, const LevelData& level, std::mt19937& rng) {
    run.activated.assign(level.switches.size(), false);
    run.sequence.resize(level.switches.size());
    for (size_t i = 0; i < level.switches.size(); i++) run.sequence[i] = (int)i;

    if (level.randomSequence) {
        std::shuffle(run.sequence.begin(), run.sequence.end(), rng);
    } else if (level.fixedSequence.size() == level.switches.size()) {
        run.sequence = level.fixedSequence;
    }

    run.currentStep = 0;
    run.solved = false;
    run.won = false;
    run.doorHeights.resize(level.doors.size());
    for (size_t i = 0; i < level.doors.size(); i++) run.doorHeights[i] = level.doors[i].size.y;
    run.padPressed.assign(level.pads.size(), false);
    run.flashTimer = 0.0f;
    run.flashWrong = false;
    run.elapsedTime = 0.0f;
    run.score = 0;
    run.isNewRecord = false;
    run.cameraYaw = 0.0f;
    run.targetCameraYaw = 0.0f;
    run.draggablePos.clear();
    run.draggableVel.assign(level.draggables.size(), Vector3{ 0, 0, 0 });
    for (const auto& d : level.draggables) run.draggablePos.push_back(d.position);
    run.player.position = level.playerStart;
    run.player.velocity = { 0, 0, 0 };
    run.player.onGround = false;
}

// Genera una texture procedurale in stile "cassa di legno" (assi orizzontali
// e rinforzi incrociati), su base quasi neutra cosi' che il tint per-cassa
// (il colore scelto nell'editor) resti ben leggibile sopra il disegno.
static Texture2D GenerateCrateTexture() {
    const int size = 128;
    Image img = GenImageColor(size, size, Color{ 215, 205, 188, 255 });
    Color plank = Color{ 150, 138, 118, 255 };
    Color darkEdge = Color{ 100, 90, 75, 255 };

    ImageDrawRectangleLines(&img, Rectangle{ 0, 0, (float)size, (float)size }, 6, darkEdge);
    int step = size / 4;
    for (int y = step; y < size; y += step) ImageDrawLineEx(&img, Vector2{ 0, (float)y }, Vector2{ (float)size, (float)y }, 3, plank);
    ImageDrawLineEx(&img, Vector2{ 0, 0 }, Vector2{ (float)size, (float)size }, 4, plank);
    ImageDrawLineEx(&img, Vector2{ (float)size, 0 }, Vector2{ 0, (float)size }, 4, plank);

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Puzzle 3D - Interruttori");
    rlEnableBackfaceCulling();
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    Texture2D crateTexture = GenerateCrateTexture();
    Model crateModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    SetMaterialTexture(&crateModel.materials[0], MATERIAL_MAP_DIFFUSE, crateTexture);

    InitAudioDevice();
    const char* musicPath = "assets/audio/menu_theme.mp3";
    bool hasMusic = FileExists(musicPath);
    Music menuMusic{};
    if (hasMusic) {
        menuMusic = LoadMusicStream(musicPath);
        menuMusic.looping = true;
        SetMusicVolume(menuMusic, 0.5f);
        PlayMusicStream(menuMusic);
    }
    bool musicMuted = false;

    Camera3D camera = { 0 };
    camera.position = Vector3{ 0.0f, 14.0f, 15.0f };
    camera.target   = Vector3{ 0.0f, 0.0f, -2.0f };
    camera.up       = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    std::random_device rd;
    std::mt19937 rng(rd());

    KeyBindings kb;
    if (!LoadKeyBindings("keybindings.json", kb)) SaveKeyBindings("keybindings.json", kb);

    HighscoreMap highscores;
    LoadHighscores("scores.json", highscores);

    LevelEditor editor;
    editor.NewLevel();
    bool playtestFromEditor = false;
    int rebindingIndex = -1;
    float settingsScroll = 0.0f;

    LevelManager levelManager;
    levelManager.ScanDirectory("levels");

    GameState state = GameState::MENU;
    int selectedLevel = -1;
    LevelData currentLevel;
    RunState run;
    float menuTime = 0.0f;
    GameState returnFromHelp = GameState::MENU;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        menuTime += dt;

        if (hasMusic) {
            UpdateMusicStream(menuMusic);
            bool wantMusic = (state != GameState::PLAYING) && !musicMuted;
            if (wantMusic && !IsMusicStreamPlaying(menuMusic)) ResumeMusicStream(menuMusic);
            if (!wantMusic && IsMusicStreamPlaying(menuMusic)) PauseMusicStream(menuMusic);
        }

        if (IsKeyPressed(kb.toggleMusic)) musicMuted = !musicMuted;

   
        // Aggiornamento logico per stato
    
        if (state == GameState::EDITOR) {
            editor.Update();
        }

        if (state == GameState::PLAYING) {
            if (IsKeyPressed(kb.rotateLeft)) run.targetCameraYaw += PI / 2.0f;
            if (IsKeyPressed(kb.rotateRight)) run.targetCameraYaw -= PI / 2.0f;

            {
                Rectangle rotLeftBtn = { screenWidth - 110.0f, screenHeight - 60.0f, 46, 46 };
                Rectangle rotRightBtn = { screenWidth - 56.0f, screenHeight - 60.0f, 46, 46 };
                Vector2 mousePos = GetMousePosition();
                bool onRotButton = CheckCollisionPointRec(mousePos, rotLeftBtn) || CheckCollisionPointRec(mousePos, rotRightBtn);
                if (!onRotButton && !run.won) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) run.targetCameraYaw += PI / 2.0f;
                    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) run.targetCameraYaw -= PI / 2.0f;
                }
            }

            // Anima la camera verso l'angolo scelto (multiplo di 90 gradi),
            // prendendo sempre il verso piu' breve.
            {
                float diff = run.targetCameraYaw - run.cameraYaw;
                while (diff > PI) diff -= 2.0f * PI;
                while (diff < -PI) diff += 2.0f * PI;
                float rotSpeed = 5.0f * kb.mouseSensitivity;
                if (fabsf(diff) < 0.02f) run.cameraYaw = run.targetCameraYaw;
                else run.cameraYaw += Clamp(diff, -rotSpeed * dt, rotSpeed * dt);
            }

            // "Avanti" e "destra" sono relativi a dove guarda la camera, non
            // agli assi fissi del mondo: cosi' WASD si comporta in modo
            // naturale in ognuna delle 4 direzioni scelte.
            Vector3 camForward = { sinf(run.cameraYaw), 0, -cosf(run.cameraYaw) };
            Vector3 camRight = { cosf(run.cameraYaw), 0, sinf(run.cameraYaw) };
            Vector3 move = { 0, 0, 0 };
            if (IsKeyDown(kb.moveUp))    move = Vector3Add(move, camForward);
            if (IsKeyDown(kb.moveDown))  move = Vector3Subtract(move, camForward);
            if (IsKeyDown(kb.moveLeft))  move = Vector3Subtract(move, camRight);
            if (IsKeyDown(kb.moveRight)) move = Vector3Add(move, camRight);
            bool jumpPressed = IsKeyPressed(kb.jump);

            UpdatePlayerPhysics(run.player, currentLevel, run.doorHeights, move, dt, jumpPressed);

            camera.target = run.player.position;
            const float cameraDistance = 10.0f;
            camera.position = Vector3{
                run.player.position.x - cameraDistance * sinf(run.cameraYaw),
                run.player.position.y + 10.0f,
                run.player.position.z + cameraDistance * cosf(run.cameraYaw)
            };

            // Il giocatore spinge le casse camminandoci contro: si spostano
            // solo lungo l'asse (X o Z) su cui la sovrapposizione e' minore,
            // cosi' il movimento resta sempre allineato a una delle 4 direzioni.
            for (size_t i = 0; i < run.draggablePos.size(); i++) {
                Vector3& cratePos = run.draggablePos[i];
                Vector3 crateSize = currentLevel.draggables[i].size;
                float dx = cratePos.x - run.player.position.x;
                float dz = cratePos.z - run.player.position.z;
                float overlapX = (crateSize.x / 2.0f + run.player.radius) - fabsf(dx);
                float overlapZ = (crateSize.z / 2.0f + run.player.radius) - fabsf(dz);

                if (overlapX > 0.0f && overlapZ > 0.0f) {
                    if (overlapX < overlapZ) {
                        cratePos.x += (dx >= 0.0f) ? overlapX : -overlapX;
                    } else {
                        cratePos.z += (dz >= 0.0f) ? overlapZ : -overlapZ;
                    }

                    // La cassa non puo' essere spinta dentro un muro o una porta chiusa.
                    float crateRadius = std::max(crateSize.x, crateSize.z) / 2.0f;
                    ResolveObstacles(cratePos, crateRadius, currentLevel.obstacles);
                    ResolveDoors(cratePos, crateRadius, currentLevel.doors, run.doorHeights);

                    // E il giocatore non resta a compenetrare la cassa, sia che
                    // si sia spostata (spinta) sia che sia rimasta ferma (bloccata).
                    ResolveBoxCollision(run.player.position, run.player.radius, cratePos, crateSize);
                }
            }

            for (size_t i = 0; i < run.draggablePos.size(); i++) {
                Vector3& dp = run.draggablePos[i];
                Vector3& dv = run.draggableVel[i];
                if (currentLevel.gravityEnabled) dv.y += GRAVITY * dt; else dv.y = 0.0f;
                dp.y += dv.y * dt;

                float halfH = currentLevel.draggables[i].size.y / 2.0f;
                float groundY;
                bool hasGround = FindGroundY(currentLevel, dp.x, dp.z, groundY);
                if (hasGround && dp.y - halfH <= groundY && dv.y <= 0.0f) {
                    dp.y = groundY + halfH;
                    dv.y = 0.0f;
                }
                if (dp.y < currentLevel.fallResetY) {
                    dp = currentLevel.draggables[i].position;
                    dv = { 0, 0, 0 };
                }
            }

            if (!run.solved && IsKeyPressed(kb.interact)) {
                for (int i = 0; i < (int)currentLevel.switches.size(); i++) {
                    if (run.activated[i]) continue;
                    const Vector3& sp = currentLevel.switches[i].position;
                    BoundingBox box = {
                        Vector3{ sp.x - 0.5f, sp.y - 0.5f, sp.z - 0.5f },
                        Vector3{ sp.x + 0.5f, sp.y + 0.5f, sp.z + 0.5f }
                    };
                    if (!CheckCollisionBoxSphere(box, run.player.position, run.player.radius)) continue;

                    if (i == run.sequence[run.currentStep]) {
                        run.activated[i] = true;
                        run.currentStep++;
                        if (run.currentStep >= (int)run.sequence.size()) run.solved = true;
                    } else {
                        std::fill(run.activated.begin(), run.activated.end(), false);
                        run.currentStep = 0;
                        run.flashWrong = true;
                        run.flashTimer = 1.0f;
                    }
                    break;
                }
            }

            if (run.flashTimer > 0.0f) run.flashTimer -= dt; else run.flashWrong = false;

            for (size_t i = 0; i < currentLevel.pads.size(); i++) {
                const LevelPad& pad = currentLevel.pads[i];
                bool pressed = false;
                for (size_t k = 0; k < run.draggablePos.size(); k++) {
                    const Color& cc = currentLevel.draggables[k].color;
                    if (cc.r != pad.color.r || cc.g != pad.color.g || cc.b != pad.color.b) continue;
                    const Vector3& cp = run.draggablePos[k];
                    bool overXZ = fabsf(cp.x - pad.position.x) <= pad.size.x / 2.0f &&
                                  fabsf(cp.z - pad.position.z) <= pad.size.z / 2.0f;
                    bool onTop = fabsf((cp.y - currentLevel.draggables[k].size.y / 2.0f) - pad.position.y) < 0.3f;
                    if (overXZ && onTop) { pressed = true; break; }
                }
                run.padPressed[i] = pressed;
            }

            std::vector<bool> doorOpenedByPad(currentLevel.doors.size(), false);
            for (size_t i = 0; i < currentLevel.pads.size(); i++) {
                int ld = currentLevel.pads[i].linkedDoor;
                if (run.padPressed[i] && ld >= 0 && ld < (int)doorOpenedByPad.size()) doorOpenedByPad[ld] = true;
            }

            for (size_t i = 0; i < currentLevel.doors.size(); i++) {
                const LevelDoor& door = currentLevel.doors[i];
                bool shouldBeOpen = doorOpenedByPad[i] || ((door.linkedSwitch < 0)
                    ? run.solved
                    : (door.linkedSwitch < (int)run.activated.size() && run.activated[door.linkedSwitch]));
                float target = shouldBeOpen ? 0.0f : door.size.y;
                float speed = dt * 2.0f;
                if (run.doorHeights[i] < target) run.doorHeights[i] = std::min(target, run.doorHeights[i] + speed);
                else if (run.doorHeights[i] > target) run.doorHeights[i] = std::max(target, run.doorHeights[i] - speed);
            }

            if (!run.won) run.elapsedTime += dt;

            if (run.solved && !run.won) {
                float dx = run.player.position.x - currentLevel.exitPosition.x;
                float dz = run.player.position.z - currentLevel.exitPosition.z;
                float r = currentLevel.exitRadius;
                if (dx * dx + dz * dz < r * r) {
                    run.won = true;
                    run.score = (int)(currentLevel.switches.size() * 500 +
                                       std::max(0.0f, 3000.0f - run.elapsedTime * 20.0f));
                    if (!playtestFromEditor && !currentLevel.filePath.empty()) {
                        HighscoreEntry& hs = highscores[currentLevel.filePath];
                        run.isNewRecord = (!hs.hasScore || run.score > hs.bestScore);
                        if (run.isNewRecord) {
                            hs.hasScore = true;
                            hs.bestScore = run.score;
                            hs.bestTime = run.elapsedTime;
                            SaveHighscores("scores.json", highscores);
                        }
                    }
                }
            }

            if (IsKeyPressed(kb.resetLevel)) {
                StartRun(run, currentLevel, rng);
            }
            if (IsKeyPressed(kb.back)) {
                state = playtestFromEditor ? GameState::EDITOR : GameState::LEVEL_SELECT;
                playtestFromEditor = false;
            }
        }

   
        // Disegno
   
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (state == GameState::MENU) {
            // Sfondo 3D animato, semplice, dietro al menu
            Camera3D bgCam = camera;
            bgCam.position = Vector3{ sinf(menuTime * 0.15f) * 16.0f, 10.0f, cosf(menuTime * 0.15f) * 16.0f };
            bgCam.target = Vector3{ 0, 0, 0 };
            BeginMode3D(bgCam);
            DrawPlane(Vector3{ 0, 0, 0 }, Vector2{ 30, 30 }, LIGHTGRAY);
            DrawGrid(30, 1.0f);
            Color demoColors[4] = { RED, BLUE, GREEN, YELLOW };
            for (int i = 0; i < 4; i++) {
                float ang = menuTime * 0.6f + i * (PI / 2.0f);
                Vector3 p = { cosf(ang) * 5.0f, 0.5f + 0.3f * sinf(menuTime + i), sinf(ang) * 5.0f };
                DrawCube(p, 1, 1, 1, demoColors[i]);
                DrawCubeWires(p, 1, 1, 1, DARKGRAY);
            }
            EndMode3D();

            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(RAYWHITE, 0.55f));

            const char* title = "PUZZLE 3D";
            int titleSize = 72;
            int tw = MeasureText(title, titleSize);
            DrawText(title, screenWidth / 2 - tw / 2 + 3, 133, titleSize, Fade(BLACK, 0.25f));
            DrawText(title, screenWidth / 2 - tw / 2, 130, titleSize, DARKBLUE);
            const char* subtitle = "Interruttori - livelli creati dagli utenti (JSON)";
            int stw = MeasureText(subtitle, 20);
            DrawText(subtitle, screenWidth / 2 - stw / 2, 210, 20, DARKGRAY);

            Rectangle playBtn = { screenWidth / 2.0f - 140, 280, 280, 52 };
            Rectangle editorBtn = { screenWidth / 2.0f - 140, 344, 280, 52 };
            Rectangle settingsBtn = { screenWidth / 2.0f - 140, 408, 280, 52 };
            Rectangle helpBtn = { screenWidth / 2.0f - 140, 472, 280, 52 };
            Rectangle quitBtn = { screenWidth / 2.0f - 140, 536, 280, 52 };

            if (DrawButton(playBtn, "GIOCA", 26, Fade(DARKBLUE, 0.85f), DARKBLUE, WHITE)) {
                levelManager.ScanDirectory("levels");
                state = GameState::LEVEL_SELECT;
            }
            if (DrawButton(editorBtn, "EDITOR LIVELLI", 22, Fade(DARKGREEN, 0.85f), DARKGREEN, WHITE)) {
                editor.NewLevel();
                state = GameState::EDITOR;
            }
            if (DrawButton(settingsBtn, "IMPOSTAZIONI", 22, Fade(DARKBLUE, 0.6f), DARKBLUE, WHITE)) {
                rebindingIndex = -1;
                state = GameState::SETTINGS;
            }
            if (DrawButton(helpBtn, "COME SI GIOCA", 22, Fade(DARKGRAY, 0.8f), DARKGRAY, WHITE)) {
                returnFromHelp = GameState::MENU;
                state = GameState::HELP;
            }
            if (DrawButton(quitBtn, "ESCI", 26, Fade(MAROON, 0.85f), MAROON, WHITE)) {
                break;
            }

            const char* musicHint = hasMusic
                ? (musicMuted ? "M: musica disattivata (premi per riattivare)" : "M: disattiva/attiva la musica")
                : "Nessun file musicale trovato in assets/audio/menu_theme.mp3";
            DrawText(musicHint, 10, screenHeight - 28, 16, DARKGRAY);
        }
        else if (state == GameState::LEVEL_SELECT) {
            DrawText("SELEZIONA UN LIVELLO", 40, 30, 32, DARKBLUE);
            DrawText("I livelli sono file .json nella cartella \"levels/\": creane uno tuo e ricompare qui premendo AGGIORNA.", 40, 70, 16, DARKGRAY);

            const auto& levels = levelManager.GetLevels();
            const auto& errs = levelManager.GetErrors();

            float listY = 110;
            for (int i = 0; i < (int)levels.size(); i++) {
                Rectangle card = { 40, listY, screenWidth - 80.0f, 64 };
                bool isSel = (selectedLevel == i);
                Color base = isSel ? Fade(DARKGREEN, 0.85f) : Fade(LIGHTGRAY, 0.9f);
                Color hover = isSel ? DARKGREEN : Fade(SKYBLUE, 0.9f);
                Color textCol = isSel ? WHITE : BLACK;
                std::string label = TextFormat("%d.  %s", i + 1, levels[i].name.c_str());
                if (DrawButton(card, label.c_str(), 22, base, hover, textCol)) {
                    selectedLevel = i;
                }
                if (!levels[i].description.empty()) {
                    DrawText(levels[i].description.c_str(), 56, (int)(listY + 38), 14,
                              isSel ? Fade(WHITE, 0.9f) : DARKGRAY);
                }
                auto hsIt = highscores.find(levels[i].filePath);
                if (hsIt != highscores.end() && hsIt->second.hasScore) {
                    std::string recordText = "Record: " + FormatTime(hsIt->second.bestTime) +
                                              "  -  " + std::to_string(hsIt->second.bestScore) + " punti";
                    int rtw = MeasureText(recordText.c_str(), 14);
                    DrawText(recordText.c_str(), (int)(screenWidth - 60 - rtw), (int)(listY + 22), 14,
                              isSel ? GOLD : DARKGREEN);
                }
                listY += 74;
            }

            if (levels.empty()) {
                DrawText("Nessun livello trovato nella cartella 'levels'.", 40, (int)listY + 10, 20, MAROON);
            }
            for (size_t i = 0; i < errs.size(); i++) {
                DrawText(errs[i].c_str(), 40, (int)(listY + 10 + i * 20), 14, MAROON);
            }

            Rectangle backBtn = { 40, screenHeight - 80.0f, 200, 50 };
            Rectangle refreshBtn = { 260, screenHeight - 80.0f, 200, 50 };
            Rectangle startBtn = { screenWidth - 260.0f, screenHeight - 80.0f, 220, 50 };

            if (DrawButton(backBtn, "INDIETRO", 20, DARKGRAY, GRAY, WHITE)) {
                state = GameState::MENU;
            }
            if (DrawButton(refreshBtn, "AGGIORNA ELENCO", 18, DARKBLUE, BLUE, WHITE)) {
                levelManager.ScanDirectory("levels");
                if (selectedLevel >= (int)levelManager.GetLevels().size()) selectedLevel = -1;
            }
            bool canStart = selectedLevel >= 0 && selectedLevel < (int)levels.size();
            if (canStart && DrawButton(startBtn, "GIOCA", 22, DARKGREEN, GREEN, WHITE)) {
                currentLevel = levels[selectedLevel];
                StartRun(run, currentLevel, rng);
                state = GameState::PLAYING;
            }
        }
        else if (state == GameState::HELP) {
            DrawText("COME SI GIOCA", 40, 40, 32, DARKBLUE);
            const char* lines[] = {
                "- Muoviti con i tasti WASD (riassegnabili in IMPOSTAZIONI).",
                "- SPAZIO per saltare (utile su piattaforme rialzate).",
                "- Frecce SINISTRA/DESTRA, click sinistro/destro del mouse, o i pulsanti in basso a destra: ruota la camera di 90 gradi.",
                "- Cammina addosso a un interruttore e premi E per attivarlo, nell'ordine mostrato in alto.",
                "- Cammina contro le casse arancioni per spingerle, lungo una delle 4 direzioni.",
                "- Devi essere abbastanza vicino a un interruttore per attivarlo.",
                "- Sbagliando l'ordine il progresso del livello si azzera.",
                "- Completata la sequenza la porta si apre: raggiungi il cerchio verde per vincere.",
                "- Attento ai vuoti tra le piattaforme: cadendo torni al punto di partenza.",
                "- Il tempo impiegato determina il punteggio: piu' veloce = punteggio piu' alto.",
                "- R: ricomincia il livello corrente (nuova sequenza se e' casuale).",
                "- ESC: torna alla selezione dei livelli.",
                "- M: attiva/disattiva la musica del menu.",
                "",
                "Puoi creare nuovi livelli con l'EDITOR LIVELLI dal menu principale,",
                "oppure scrivendo a mano un file .json nella cartella 'levels/'.",
                "Vedi il file README.md per lo schema completo con tutti i campi disponibili.",
            };
            int y = 100;
            for (const char* line : lines) {
                DrawText(line, 40, y, 18, line[0] == '\0' ? RAYWHITE : DARKGRAY);
                y += 28;
            }
            Rectangle backBtn = { 40, screenHeight - 80.0f, 200, 50 };
            if (DrawButton(backBtn, "INDIETRO", 20, DARKGRAY, GRAY, WHITE)) {
                state = returnFromHelp;
            }
        }
        else if (state == GameState::SETTINGS) {
            DrawText("IMPOSTAZIONI", 40, 30, 32, DARKBLUE);
            DrawText("Clicca su un tasto per riassegnarlo, poi premi il nuovo tasto desiderato.", 40, 70, 16, DARKGRAY);

            struct BindRow { const char* label; int* key; };
            BindRow rows[] = {
                { "Avanti", &kb.moveUp },
                { "Indietro", &kb.moveDown },
                { "Sinistra", &kb.moveLeft },
                { "Destra", &kb.moveRight },
                { "Salta", &kb.jump },
                { "Interagisci (interruttori)", &kb.interact },
                { "Ruota camera a sinistra", &kb.rotateLeft },
                { "Ruota camera a destra", &kb.rotateRight },
                { "Ricomincia livello", &kb.resetLevel },
                { "Torna indietro / Esci dal livello", &kb.back },
                { "Muta musica", &kb.toggleMusic },
            };
            const int rowCount = (int)(sizeof(rows) / sizeof(rows[0]));

            if (rebindingIndex >= 0 && rebindingIndex < rowCount) {
                int newKey = GetKeyPressed();
                if (newKey != 0) {
                    *(rows[rebindingIndex].key) = newKey;
                    SaveKeyBindings("keybindings.json", kb);
                    rebindingIndex = -1;
                }
            }

            const float settingsTop = 100.0f;
            const float settingsBottom = 610.0f;
            Rectangle settingsVisibleRect = { 0, settingsTop, (float)screenWidth, settingsBottom - settingsTop };
            if (CheckCollisionPointRec(GetMousePosition(), settingsVisibleRect)) {
                settingsScroll -= GetMouseWheelMove() * 30.0f;
            }
            if (settingsScroll < 0.0f) settingsScroll = 0.0f;

            g_uiScrollOffsetY = settingsScroll;
            BeginScissorMode((int)settingsVisibleRect.x, (int)settingsVisibleRect.y, (int)settingsVisibleRect.width, (int)settingsVisibleRect.height);
            rlPushMatrix();
            rlTranslatef(0, -settingsScroll, 0);

            float y = settingsTop + 8.0f;
            for (int i = 0; i < rowCount; i++) {
                DrawText(rows[i].label, 60, (int)y + 8, 18, BLACK);
                Rectangle keyBtn = { 460, y, 220, 38 };
                bool waiting = (rebindingIndex == i);
                std::string label = waiting ? "Premi un tasto..." : KeyToName(*(rows[i].key));
                if (DrawButton(keyBtn, label.c_str(), 16, waiting ? Fade(GOLD, 0.9f) : DARKBLUE,
                               waiting ? GOLD : BLUE, WHITE)) {
                    rebindingIndex = i;
                }
                y += 46;
            }

            Rectangle resetBtn = { 60, y + 14, 260, 42 };
            if (DrawButton(resetBtn, "RIPRISTINA PREDEFINITI", 16, DARKGRAY, GRAY, WHITE)) {
                kb = KeyBindings{};
                SaveKeyBindings("keybindings.json", kb);
                rebindingIndex = -1;
            }

            float sensY = y + 14;
            DrawText("Velocita' rotazione camera:", 400, (int)sensY + 9, 18, BLACK);
            Rectangle sensMinus = { 700, sensY, 42, 42 };
            Rectangle sensPlus  = { 870, sensY, 42, 42 };
            if (DrawMiniButton(sensMinus, "-", LIGHTGRAY, GRAY)) {
                kb.mouseSensitivity = std::max(0.2f, kb.mouseSensitivity - 0.1f);
                SaveKeyBindings("keybindings.json", kb);
            }
            std::string sensVal = TextFormat("%.1fx", kb.mouseSensitivity);
            int svw = MeasureText(sensVal.c_str(), 22);
            DrawText(sensVal.c_str(), (int)(742 + (870 - 742) / 2 - svw / 2), (int)sensY + 10, 22, DARKBLUE);
            if (DrawMiniButton(sensPlus, "+", LIGHTGRAY, GRAY)) {
                kb.mouseSensitivity = std::min(3.0f, kb.mouseSensitivity + 0.1f);
                SaveKeyBindings("keybindings.json", kb);
            }

            y = sensY + 14.0f;
            float contentHeight = y - settingsTop;

            rlPopMatrix();
            EndScissorMode();
            g_uiScrollOffsetY = 0.0f;

            float maxScroll = std::max(0.0f, contentHeight - (settingsBottom - settingsTop));
            if (settingsScroll > maxScroll) settingsScroll = maxScroll;

            if (maxScroll > 0.0f) {
                Rectangle track = { screenWidth - 20.0f, settingsTop, 6, settingsBottom - settingsTop };
                DrawRectangleRec(track, Fade(LIGHTGRAY, 0.6f));
                float thumbH = std::max(24.0f, track.height * (track.height / contentHeight));
                float thumbY = track.y + (track.height - thumbH) * (settingsScroll / maxScroll);
                DrawRectangleRec(Rectangle{ track.x, thumbY, track.width, thumbH }, DARKGRAY);
            }

            Rectangle backBtn = { 40, screenHeight - 80.0f, 200, 50 };
            if (DrawButton(backBtn, "INDIETRO", 20, DARKGRAY, GRAY, WHITE)) {
                rebindingIndex = -1;
                state = GameState::MENU;
            }
        }
        else if (state == GameState::EDITOR) {
            editor.Draw();
            if (editor.ConsumePlaytestRequest()) {
                currentLevel = editor.BuildLevelData();
                StartRun(run, currentLevel, rng);
                playtestFromEditor = true;
                state = GameState::PLAYING;
            }
            if (editor.ConsumeExitRequest()) {
                state = GameState::MENU;
            }
        }
        else if (state == GameState::PLAYING) {
            BeginMode3D(camera);

            for (const auto& p : currentLevel.platforms) {
                float radius = Vector3Length(Vector3Scale(p.size, 0.5f));
                if (!IsVisibleToCamera(camera, p.position, radius)) continue;
                DrawCube(p.position, p.size.x, p.size.y, p.size.z, p.color);
                DrawCubeWires(p.position, p.size.x, p.size.y, p.size.z, Fade(BLACK, 0.25f));
            }
            for (const auto& o : currentLevel.obstacles) {
                float radius = Vector3Length(Vector3Scale(o.size, 0.5f));
                if (!IsVisibleToCamera(camera, o.position, radius)) continue;
                DrawCube(o.position, o.size.x, o.size.y, o.size.z, o.color);
                DrawCubeWires(o.position, o.size.x, o.size.y, o.size.z, DARKGRAY);
            }
            for (size_t i = 0; i < currentLevel.draggables.size(); i++) {
                Vector3 dp = run.draggablePos[i];
                Vector3 ds = currentLevel.draggables[i].size;
                float radius = Vector3Length(Vector3Scale(ds, 0.5f));
                if (!IsVisibleToCamera(camera, dp, radius)) continue;
                DrawModelEx(crateModel, dp, Vector3{ 0, 1, 0 }, 0.0f, ds, currentLevel.draggables[i].color);
                DrawCubeWires(dp, ds.x, ds.y, ds.z, BLACK);
            }

            for (size_t i = 0; i < currentLevel.pads.size(); i++) {
                const LevelPad& pad = currentLevel.pads[i];
                if (!IsVisibleToCamera(camera, pad.position, pad.size.x)) continue;
                Color c = run.padPressed[i] ? pad.color : Fade(pad.color, 0.45f);
                DrawCube(pad.position, pad.size.x, pad.size.y, pad.size.z, c);
                DrawCubeWires(pad.position, pad.size.x, pad.size.y, pad.size.z, DARKGRAY);
            }

            for (size_t i = 0; i < currentLevel.switches.size(); i++) {
                const auto& s = currentLevel.switches[i];
                if (!IsVisibleToCamera(camera, s.position, 0.87f)) continue;
                Color c = run.activated[i] ? s.color : Fade(s.color, 0.4f);
                DrawCube(s.position, 1, 1, 1, c);
                DrawCubeWires(s.position, 1, 1, 1, DARKGRAY);
            }

            for (size_t i = 0; i < currentLevel.doors.size(); i++) {
                const auto& door = currentLevel.doors[i];
                float h = run.doorHeights[i];
                if (h <= 0.01f) continue;
                Vector3 effSize = GetDoorEffectiveSize(door);
                Vector3 dp = { door.position.x, h / 2.0f, door.position.z };
                DrawCube(dp, effSize.x, h, effSize.z, door.color);
                DrawCubeWires(dp, effSize.x, h, effSize.z, BLACK);
            }

            DrawCircle3D(currentLevel.exitPosition, currentLevel.exitRadius, Vector3{ 1, 0, 0 }, 90.0f,
                          run.solved ? GREEN : GRAY);

            DrawSphere(run.player.position, run.player.radius, ORANGE);
            DrawSphereWires(run.player.position, run.player.radius, 8, 8, MAROON);

            EndMode3D();

            // --- HUD ---
            DrawText(currentLevel.name.c_str(), 10, 10, 22, DARKBLUE);
            DrawText("Muoviti con WASD, SPAZIO per saltare. Tocca un interruttore e premi E per attivarlo.",
                      10, 36, 16, DARKGRAY);

            std::string seqText = "Sequenza: ";
            for (size_t k = 0; k < run.sequence.size(); k++) {
                seqText += currentLevel.switches[run.sequence[k]].name;
                if (k + 1 < run.sequence.size()) seqText += " > ";
            }
            DrawText(seqText.c_str(), 10, 58, 18, BLACK);
            DrawText(TextFormat("Passo attuale: %d / %d", run.currentStep, (int)run.sequence.size()),
                      10, 80, 18, DARKBLUE);
            DrawText(TextFormat("Tempo: %s", FormatTime(run.elapsedTime).c_str()), 10, 102, 18, DARKGREEN);
            DrawText("ESC: torna indietro   |   R: ricomincia", 10, screenHeight - 26, 16, DARKGRAY);

            {
                Rectangle rotLeftBtn = { screenWidth - 110.0f, screenHeight - 60.0f, 46, 46 };
                Rectangle rotRightBtn = { screenWidth - 56.0f, screenHeight - 60.0f, 46, 46 };
                if (DrawButton(rotLeftBtn, "<", 26, Fade(DARKBLUE, 0.75f), BLUE, WHITE)) run.targetCameraYaw += PI / 2.0f;
                if (DrawButton(rotRightBtn, ">", 26, Fade(DARKBLUE, 0.75f), BLUE, WHITE)) run.targetCameraYaw -= PI / 2.0f;
            }

            if (run.flashWrong) {
                DrawText("SBAGLIATO! Riprova.", screenWidth / 2 - 100, 90, 24, RED);
            }
            if (run.solved && !run.won) {
                DrawText("Porta aperta! Vai verso il cerchio verde per uscire.", screenWidth / 2 - 260, 90, 24, DARKGREEN);
            }
            if (run.won) {
                DrawRectangle(0, screenHeight / 2 - 100, screenWidth, 220, Fade(BLACK, 0.65f));
                DrawText("HAI VINTO!", screenWidth / 2 - 110, screenHeight / 2 - 90, 44, GOLD);
                std::string statsText = "Tempo: " + FormatTime(run.elapsedTime) + "   Punteggio: " + std::to_string(run.score);
                int stw = MeasureText(statsText.c_str(), 22);
                DrawText(statsText.c_str(), screenWidth / 2 - stw / 2, screenHeight / 2 - 38, 22, WHITE);
                if (playtestFromEditor) {
                    const char* note = "(modalita' prova editor: il record non viene salvato)";
                    int ntw = MeasureText(note, 16);
                    DrawText(note, screenWidth / 2 - ntw / 2, screenHeight / 2 - 10, 16, LIGHTGRAY);
                } else if (run.isNewRecord) {
                    const char* rec = "NUOVO RECORD!";
                    int rtw = MeasureText(rec, 20);
                    DrawText(rec, screenWidth / 2 - rtw / 2, screenHeight / 2 - 10, 20, GOLD);
                }
                Rectangle again = { screenWidth / 2 - 220.0f, screenHeight / 2 + 30.0f, 200, 46 };
                Rectangle back  = { screenWidth / 2 + 20.0f,  screenHeight / 2 + 30.0f, 200, 46 };
                if (DrawButton(again, "RIGIOCA", 20, DARKGREEN, GREEN, WHITE)) {
                    StartRun(run, currentLevel, rng);
                }
                const char* backLabel = playtestFromEditor ? "TORNA ALL'EDITOR" : "ALTRI LIVELLI";
                if (DrawButton(back, backLabel, playtestFromEditor ? 16 : 20, DARKBLUE, BLUE, WHITE)) {
                    state = playtestFromEditor ? GameState::EDITOR : GameState::LEVEL_SELECT;
                    playtestFromEditor = false;
                }
            }
        }

        EndDrawing();
    }

    if (hasMusic) UnloadMusicStream(menuMusic);
    CloseAudioDevice();
    UnloadModel(crateModel); // libera anche crateTexture, assegnata al suo materiale
    CloseWindow();
    return 0;
}