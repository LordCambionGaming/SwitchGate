#include "raylib.h"
#include "raymath.h"
#include "Level.h"
#include "Physics.h"
#include "Lighting.h"
#include "Rendering.h"
#include "Config.h"
#include "Scores.h"
#include "Editor.h"
#include "UI.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cctype>

#if defined(_WIN32) && defined(NDEBUG)
// Nelle build Release su Windows, nasconde la finestra della console nera
// che altrimenti si apre insieme alla finestra di gioco.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

// Stato globale dell'applicazione


enum class GameState { MENU, LEVEL_SELECT, PLAYING, HELP, SETTINGS, EDITOR };


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
    float hintTimer = 0.0f;
    Player player;
    float elapsedTime = 0.0f;
    int score = 0;
    bool isNewRecord = false;
    float cameraYaw = 0.0f;
    float targetCameraYaw = 0.0f;

    // Sub-mondo "attivo" in questo momento: 0-3, uno per ognuna delle 4
    // direzioni fisse della camera (che scatta sempre a multipli di 90
    // gradi). Ricalcolato ogni frame da targetCameraYaw (si aggiorna
    // nell'istante in cui il giocatore decide di girarsi, non solo a fine
    // animazione): gli oggetti di un altro sub-mondo diventano invisibili E
    // intangibili, quelli con subworld = -1 restano sempre presenti.
    int currentSubworld = 0;

    std::vector<Vector3> draggablePos;
    std::vector<Vector3> draggableVel;
    std::vector<bool> padPressed;
    std::vector<bool> receiverLit;
    std::vector<LightBeamSegment> beams;

    // Angolo ATTUALE di ogni specchio durante la partita: il giocatore puo'
    // ruotarli con Q/E per indirizzare il laser (vedi sotto). Parte sempre
    // dall'angolo configurato nel livello (currentLevel.mirrors non viene
    // mai modificato), cosi' "Ricomincia livello" li riporta all'originale.
    std::vector<float> mirrorAngles;
    std::vector<float> targetMirrorAngles;
};

// Copia level applicando gli angoli specchio CORRENTI della partita
// (run.mirrorAngles, che il giocatore puo' cambiare con Q/E) al posto di
// quelli statici salvati nel livello. Usata per il calcolo dei raggi di luce
// e per il disegno durante il gioco, senza mai toccare currentLevel.mirrors.
static LevelData ApplyMirrorAngles(const LevelData& level, const std::vector<float>& mirrorAngles) {
    LevelData out = level;
    for (size_t i = 0; i < out.mirrors.size() && i < mirrorAngles.size(); i++) {
        out.mirrors[i].angleDeg = mirrorAngles[i];
    }
    return out;
}

static void StartRun(RunState& run, const LevelData& level, std::mt19937& rng) {
    run.activated.assign(level.switches.size(), false);
    run.sequence.resize(level.switches.size());
    for (size_t i = 0; i < level.switches.size(); i++) run.sequence[i] = (int)i;

    if (level.randomSequence && !level.switches.empty()) {
        std::shuffle(run.sequence.begin(), run.sequence.end(), rng);
    } else if (level.fixedSequence.size() == level.switches.size()) {
        run.sequence = level.fixedSequence;
    }

    run.currentStep = 0;
    run.solved = level.switches.empty();
    run.won = false;
    run.doorHeights.resize(level.doors.size());
    for (size_t i = 0; i < level.doors.size(); i++) run.doorHeights[i] = level.doors[i].size.y;
    run.padPressed.assign(level.pads.size(), false);
    run.receiverLit.assign(level.receivers.size(), false);
    run.beams.clear();
    run.flashTimer = 0.0f;
    run.hintTimer = 0.0f;
    run.flashWrong = false;
    run.elapsedTime = 0.0f;
    run.score = 0;
    run.isNewRecord = false;
    run.cameraYaw = 0.0f;
    run.targetCameraYaw = 0.0f;
    run.currentSubworld = 0;
    run.draggablePos.clear();
    run.draggableVel.assign(level.draggables.size(), Vector3{ 0, 0, 0 });
    for (const auto& d : level.draggables) run.draggablePos.push_back(d.position);
    run.mirrorAngles.clear();
    run.targetMirrorAngles.clear();
    for (const auto& m : level.mirrors) {
        run.mirrorAngles.push_back(m.angleDeg);
        run.targetMirrorAngles.push_back(m.angleDeg);
    }
    run.player.position = level.playerStart;
    run.player.velocity = { 0, 0, 0 };
    run.player.onGround = false;
}

// Genera una texture procedurale in stile "cassa di legno" (assi orizzontali
// e rinforzi incrociati), su base quasi neutra cosi' che il tint per-cassa
// (il colore scelto nell'editor) resti ben leggibile sopra il disegno.
static std::string ColorToDisplayName(Color c) {
    struct Named { const char* name; Color color; };
    static const Named table[] = {
        { "ROSSO", RED }, { "BLU", BLUE }, { "VERDE", GREEN }, { "GIALLO", YELLOW },
        { "ARANCIONE", ORANGE }, { "VIOLA", PURPLE }, { "ROSA", PINK }, { "ORO", GOLD },
        { "LIME", LIME }, { "AZZURRO", SKYBLUE }, { "GRIGIO CHIARO", LIGHTGRAY },
        { "GRIGIO", GRAY }, { "GRIGIO SCURO", DARKGRAY }, { "MARRONE", BROWN },
    };
    for (const auto& n : table) {
        if (n.color.r == c.r && n.color.g == c.g && n.color.b == c.b && n.color.a == c.a) return n.name;
    }
    return "?";
}

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

    InitWindow(screenWidth, screenHeight, "Switch Gate");
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
    float helpScroll = 0.0f;

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
            // Il sub-mondo cambia nell'istante in cui il giocatore decide di
            // girare la camera (targetCameraYaw), non solo quando l'animazione
            // dello scatto finisce: cosi' oggetti/collisioni del nuovo mondo
            // sono gia' coerenti mentre la camera sta ancora ruotando verso di
            // esso, invece di restare "vecchi" per una frazione di secondo.
            {
                int q = (int)lroundf(run.targetCameraYaw / (PI / 2.0f));
                run.currentSubworld = ((q % 4) + 4) % 4;
            }

            Vector3 camForward = { sinf(run.cameraYaw), 0, -cosf(run.cameraYaw) };
            Vector3 camRight = { cosf(run.cameraYaw), 0, sinf(run.cameraYaw) };
            Vector3 move = { 0, 0, 0 };
            if (IsKeyDown(kb.moveUp))    move = Vector3Add(move, camForward);
            if (IsKeyDown(kb.moveDown))  move = Vector3Subtract(move, camForward);
            if (IsKeyDown(kb.moveLeft))  move = Vector3Subtract(move, camRight);
            if (IsKeyDown(kb.moveRight)) move = Vector3Add(move, camRight);
            bool jumpPressed = IsKeyPressed(kb.jump);

            UpdatePlayerPhysics(run.player, currentLevel, run.doorHeights, move, dt, jumpPressed, run.currentSubworld);

            // Il giocatore puo' ruotare lo specchio piu' vicino (se abbastanza
            // vicino) con Q (senso antiorario) / E (senso orario), per
            // indirizzare il laser verso il ricevitore giusto. Si ruota
            // run.mirrorAngles, MAI currentLevel.mirrors: cosi' "Ricomincia
            // livello" (StartRun) riporta sempre gli specchi all'angolo
            // originale del livello.
            if (!currentLevel.mirrors.empty()) {
                const float mirrorInteractRange = 3.0f;
                int nearestMirror = -1;
                float nearestDistSq = mirrorInteractRange * mirrorInteractRange;
                for (size_t i = 0; i < currentLevel.mirrors.size(); i++) {
                    float dx = currentLevel.mirrors[i].position.x - run.player.position.x;
                    float dz = currentLevel.mirrors[i].position.z - run.player.position.z;
                    float d2 = dx * dx + dz * dz;
                    if (d2 < nearestDistSq) { nearestDistSq = d2; nearestMirror = (int)i; }
                }
                if (nearestMirror >= 0) {
                    // Ogni pressione sposta il TARGET di 45 gradi (angoli
                    // sempre "puliti": 0, 45, 90, 135...); l'angolo vero e
                    // proprio (run.mirrorAngles) lo raggiunge con un'animazione
                    // fluida qui sotto, invece di scattare di colpo.
                    if (IsKeyPressed(kb.rotateMirrorLeft)) run.targetMirrorAngles[nearestMirror] -= 45.0f;
                    if (IsKeyPressed(kb.rotateMirrorRight)) run.targetMirrorAngles[nearestMirror] += 45.0f;
                }
            }

            // Animazione fluida dell'angolo di ogni specchio verso il target,
            // prendendo sempre il verso piu' breve.
            for (size_t i = 0; i < run.mirrorAngles.size(); i++) {
                float diff = run.targetMirrorAngles[i] - run.mirrorAngles[i];
                while (diff > 180.0f) diff -= 360.0f;
                while (diff < -180.0f) diff += 360.0f;

                const float mirrorAnimSpeed = 360.0f; // gradi al secondo
                if (fabsf(diff) < 0.5f) {
                    run.mirrorAngles[i] = run.targetMirrorAngles[i];
                } else {
                    run.mirrorAngles[i] += Clamp(diff, -mirrorAnimSpeed * dt, mirrorAnimSpeed * dt);
                }

                while (run.mirrorAngles[i] < 0.0f) run.mirrorAngles[i] += 360.0f;
                while (run.mirrorAngles[i] >= 360.0f) run.mirrorAngles[i] -= 360.0f;
            }

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

                // Il push va applicato SOLO se il giocatore si sovrappone anche
                // in verticale alla cassa: senza questo controllo, saltando
                // sopra una cassa (es. scavalcando un muro) la si spingeva lo
                // stesso solo perche' orizzontalmente vicina, anche a mezz'aria
                // ben sopra di essa - causando spinte enormi e casse che
                // finivano teletrasportate dentro/oltre i muri.
                float crateMinY = cratePos.y - crateSize.y / 2.0f;
                float crateMaxY = cratePos.y + crateSize.y / 2.0f;
                bool verticalOverlap = (run.player.position.y + run.player.radius > crateMinY) &&
                                       (run.player.position.y - run.player.radius < crateMaxY);

                if (verticalOverlap && overlapX > 0.0f && overlapZ > 0.0f) {
                    if (overlapX < overlapZ) {
                        cratePos.x += (dx >= 0.0f) ? overlapX : -overlapX;
                    } else {
                        cratePos.z += (dz >= 0.0f) ? overlapZ : -overlapZ;
                    }

                    // Se la cassa spinta incontra un'altra cassa o un muro, si ferma (viene bloccata)
                    for (int iter = 0; iter < 2; iter++) {
                        for (size_t j = 0; j < run.draggablePos.size(); j++) {
                            if (i == j) continue;
                            ResolveBoxToBoxCollision(run.draggablePos[i], currentLevel.draggables[i].size,
                                                     run.draggablePos[j], currentLevel.draggables[j].size);
                        }
                        float crateRadius = std::max(crateSize.x, crateSize.z) / 2.0f;
                        ResolveObstacles(cratePos, crateRadius, currentLevel.obstacles, run.currentSubworld);
                        ResolveDoors(cratePos, crateRadius, currentLevel.doors, run.doorHeights, run.currentSubworld);
                    }

                    ResolveBoxCollision(run.player.position, run.player.radius, cratePos, crateSize);
                }
            }

            // Risoluzione generale tra tutte le casse e ostacoli
            for (int iter = 0; iter < 2; iter++) {
                for (size_t i = 0; i < run.draggablePos.size(); i++) {
                    for (size_t j = 0; j < run.draggablePos.size(); j++) {
                        if (i == j) continue;
                        ResolveBoxToBoxCollision(run.draggablePos[i], currentLevel.draggables[i].size,
                                                 run.draggablePos[j], currentLevel.draggables[j].size);
                    }
                }
                for (size_t i = 0; i < run.draggablePos.size(); i++) {
                    float crateRadius = std::max(currentLevel.draggables[i].size.x, currentLevel.draggables[i].size.z) / 2.0f;
                    ResolveObstacles(run.draggablePos[i], crateRadius, currentLevel.obstacles, run.currentSubworld);
                    ResolveDoors(run.draggablePos[i], crateRadius, currentLevel.doors, run.doorHeights, run.currentSubworld);
                }
            }

            for (size_t i = 0; i < run.draggablePos.size(); i++) {
                Vector3& dp = run.draggablePos[i];
                Vector3& dv = run.draggableVel[i];
                float halfH = currentLevel.draggables[i].size.y / 2.0f;
                float prevBottomY = dp.y - halfH;

                if (currentLevel.gravityEnabled) dv.y += GRAVITY * dt; else dv.y = 0.0f;
                dp.y += dv.y * dt;

                float groundY;
                bool hasGround = FindGroundY(currentLevel, dp.x, dp.z, prevBottomY + 0.05f, run.currentSubworld, groundY);
                if (hasGround && dp.y - halfH <= groundY && dv.y <= 0.0f) {
                    dp.y = groundY + halfH;
                    dv.y = 0.0f;
                }
                if (dp.y < currentLevel.fallResetY) {
                    dp = currentLevel.draggables[i].position;
                    dv = { 0, 0, 0 };
                }
            }

            if (!run.solved && !currentLevel.switches.empty() && IsKeyPressed(kb.interact)) {
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
            if (!run.solved && !currentLevel.switches.empty() && IsKeyPressed(kb.hint)) run.hintTimer = 3.0f;
            if (run.hintTimer > 0.0f) run.hintTimer -= dt;

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

            run.beams = ComputeLightBeams(ApplyMirrorAngles(currentLevel, run.mirrorAngles), run.doorHeights, run.draggablePos, run.currentSubworld, run.receiverLit);

            // aggiornamento porte and / or
            for (size_t i = 0; i < currentLevel.doors.size(); i++) {
                const LevelDoor& door = currentLevel.doors[i];

                std::string op = door.logicOp;
                std::transform(op.begin(), op.end(), op.begin(), ::tolower);
                bool isAnd = (op == "and");

                // 1. Interruttori collegati
                bool switchesSatisfied = true;
                bool hasSwitchInput = !door.linkedSwitches.empty() || door.linkedSwitch >= 0;

                if (!door.linkedSwitches.empty()) {
                    if (isAnd) {
                        switchesSatisfied = true;
                        for (int swIdx : door.linkedSwitches) {
                            if (swIdx < 0 || swIdx >= (int)run.activated.size() || !run.activated[swIdx]) {
                                switchesSatisfied = false;
                                break;
                            }
                        }
                    } else { // "OR"
                        switchesSatisfied = false;
                        for (int swIdx : door.linkedSwitches) {
                            if (swIdx >= 0 && swIdx < (int)run.activated.size() && run.activated[swIdx]) {
                                switchesSatisfied = true;
                                break;
                            }
                        }
                    }
                } else if (door.linkedSwitch >= 0) {
                    switchesSatisfied = (door.linkedSwitch < (int)run.activated.size() && run.activated[door.linkedSwitch]);
                } else {
                    hasSwitchInput = false;
                    switchesSatisfied = false;
                }

                // 2. Pedane collegate
                std::vector<bool> connectedPadsStates;
                for (size_t pIdx = 0; pIdx < currentLevel.pads.size(); pIdx++) {
                    if (currentLevel.pads[pIdx].linkedDoor == (int)i) {
                        connectedPadsStates.push_back(run.padPressed[pIdx]);
                    }
                }

                bool padsSatisfied = true;
                bool hasPadInput = !connectedPadsStates.empty();

                if (hasPadInput) {
                    if (isAnd) {
                        padsSatisfied = true;
                        for (bool pressed : connectedPadsStates) {
                            if (!pressed) {
                                padsSatisfied = false;
                                break;
                            }
                        }
                    } else { // "OR"
                        padsSatisfied = false;
                        for (bool pressed : connectedPadsStates) {
                            if (pressed) {
                                padsSatisfied = true;
                                break;
                            }
                        }
                    }
                } else {
                    padsSatisfied = false;
                }

                // 3. Ricevitori di luce collegati
                std::vector<bool> connectedReceiverStates;
                for (size_t rIdx = 0; rIdx < currentLevel.receivers.size(); rIdx++) {
                    if (currentLevel.receivers[rIdx].linkedDoor == (int)i) {
                        connectedReceiverStates.push_back(run.receiverLit[rIdx]);
                    }
                }

                bool receiversSatisfied = true;
                bool hasReceiverInput = !connectedReceiverStates.empty();

                if (hasReceiverInput) {
                    if (isAnd) {
                        receiversSatisfied = true;
                        for (bool lit : connectedReceiverStates) {
                            if (!lit) { receiversSatisfied = false; break; }
                        }
                    } else { // "OR"
                        receiversSatisfied = false;
                        for (bool lit : connectedReceiverStates) {
                            if (lit) { receiversSatisfied = true; break; }
                        }
                    }
                } else {
                    receiversSatisfied = false;
                }

                // 4. Combinazione finale (tutte le sorgenti collegate a questa porta,
                // qualunque sia il loro tipo, vengono combinate con lo stesso AND/OR)
                bool hasAnyInput = hasSwitchInput || hasPadInput || hasReceiverInput;
                bool shouldBeOpen;
                if (hasAnyInput) {
                    shouldBeOpen = isAnd;
                    if (isAnd) {
                        if (hasSwitchInput) shouldBeOpen = shouldBeOpen && switchesSatisfied;
                        if (hasPadInput) shouldBeOpen = shouldBeOpen && padsSatisfied;
                        if (hasReceiverInput) shouldBeOpen = shouldBeOpen && receiversSatisfied;
                    } else {
                        shouldBeOpen = false;
                        if (hasSwitchInput) shouldBeOpen = shouldBeOpen || switchesSatisfied;
                        if (hasPadInput) shouldBeOpen = shouldBeOpen || padsSatisfied;
                        if (hasReceiverInput) shouldBeOpen = shouldBeOpen || receiversSatisfied;
                    }
                } else {
                    shouldBeOpen = run.solved;
                }

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
                    run.score = (int)(std::max(1.0f, (float)currentLevel.switches.size()) * 500 +
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

            const char* title = "SWITCH GATE";
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
            DrawText("COME SI GIOCA", 40, 30, 32, DARKBLUE);
            DrawText("Usa la rotellina del mouse per scorrere l'elenco delle istruzioni.", 40, 70, 16, DARKGRAY);

            const char* lines[] = {
                "- Muoviti con i tasti WASD (riassegnabili in IMPOSTAZIONI).",
                "- SPAZIO per saltare (utile su piattaforme rialzate).",
                "- Frecce SINISTRA/DESTRA, click sinistro/destro del mouse, o i pulsanti in basso a destra: ruota la camera di 90 gradi.",
                "- Cammina addosso a un interruttore e premi E per attivarlo, nell'ordine mostrato in alto.",
                "- H: mostra per qualche secondo quale interruttore premere adesso.",
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
                "",
                "In IMPOSTAZIONI puoi attivare la modalita' daltonici: mostra il nome",
                "del colore anche su casse e pedane, non solo sugli interruttori.",
                "",
                "Puoi creare nuovi livelli con l'EDITOR LIVELLI dal menu principale,",
                "oppure scrivendo a mano un file .json nella cartella 'levels/'.",
            };
            const int lineCount = (int)(sizeof(lines) / sizeof(lines[0]));

            const float helpTop = 100.0f;
            const float helpBottom = 610.0f;
            Rectangle helpVisibleRect = { 0, helpTop, (float)screenWidth, helpBottom - helpTop };
            if (CheckCollisionPointRec(GetMousePosition(), helpVisibleRect)) {
                helpScroll -= GetMouseWheelMove() * 30.0f;
            }
            if (helpScroll < 0.0f) helpScroll = 0.0f;

            g_uiScrollOffsetY = helpScroll;
            BeginScissorMode((int)helpVisibleRect.x, (int)helpVisibleRect.y, (int)helpVisibleRect.width, (int)helpVisibleRect.height);
            rlPushMatrix();
            rlTranslatef(0, -helpScroll, 0);

            float y = helpTop + 8.0f;
            for (int i = 0; i < lineCount; i++) {
                DrawText(lines[i], 60, (int)y, 18, lines[i][0] == '\0' ? RAYWHITE : DARKGRAY);
                y += 28;
            }
            float contentHeight = y - helpTop;

            rlPopMatrix();
            EndScissorMode();
            g_uiScrollOffsetY = 0.0f;

            float maxScroll = std::max(0.0f, contentHeight - (helpBottom - helpTop));
            if (helpScroll > maxScroll) helpScroll = maxScroll;

            if (maxScroll > 0.0f) {
                Rectangle track = { screenWidth - 20.0f, helpTop, 6, helpBottom - helpTop };
                DrawRectangleRec(track, Fade(LIGHTGRAY, 0.6f));
                float thumbH = std::max(24.0f, track.height * (track.height / contentHeight));
                float thumbY = track.y + (track.height - thumbH) * (helpScroll / maxScroll);
                DrawRectangleRec(Rectangle{ track.x, thumbY, track.width, thumbH }, DARKGRAY);
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
                { "Ruota specchio/emettitore (sinistra)", &kb.rotateMirrorLeft },
                { "Ruota specchio/emettitore (destra)", &kb.rotateMirrorRight },
                { "Ruota camera a sinistra", &kb.rotateLeft },
                { "Ruota camera a destra", &kb.rotateRight },
                { "Suggerimento", &kb.hint },
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

            y = sensY + 56.0f;
            {
                Rectangle cbBtn = { 60, y, 460, 42 };
                std::string cbLabel = std::string("Modalita' daltonici: ") + (kb.colorblindMode ? "ON" : "OFF");
                if (DrawButton(cbBtn, cbLabel.c_str(), 16, kb.colorblindMode ? DARKGREEN : DARKGRAY,
                               kb.colorblindMode ? GREEN : GRAY, WHITE)) {
                    kb.colorblindMode = !kb.colorblindMode;
                    SaveKeyBindings("keybindings.json", kb);
                }
                DrawText("Mostra il nome del colore anche su casse e pedane, non solo sugli interruttori.",
                          540, (int)y + 13, 14, DARKGRAY);
            }

            y += 50.0f;
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
            SceneRenderState rs;
            rs.doorHeights = run.doorHeights;
            rs.switchActivated = run.activated;
            rs.padPressed = run.padPressed;
            rs.receiverLit = run.receiverLit;
            rs.beams = run.beams;
            rs.draggablePositions = run.draggablePos;
            rs.exitOpen = run.solved;
            rs.showPlayer = true;
            rs.playerPosition = run.player.position;
            rs.playerRadius = run.player.radius;
            rs.subworld = run.currentSubworld;

            BeginMode3D(camera);
            DrawLevelScene(ApplyMirrorAngles(currentLevel, run.mirrorAngles), rs, camera, &crateModel);
            EndMode3D();

            for (size_t i = 0; i < currentLevel.switches.size(); i++) {
                Vector3 labelPos = Vector3Add(currentLevel.switches[i].position, Vector3{ 0, 0.9f, 0 });
                Vector2 sp = GetWorldToScreen(labelPos, camera);
                if (sp.x < -100 || sp.x > screenWidth + 100 || sp.y < -100 || sp.y > screenHeight + 100) continue;
                const char* label = currentLevel.switches[i].name.c_str();
                int tw = MeasureText(label, 16);
                DrawText(label, (int)sp.x - tw / 2 + 1, (int)sp.y + 1, 16, BLACK);
                DrawText(label, (int)sp.x - tw / 2, (int)sp.y, 16, run.activated[i] ? GREEN : WHITE);
            }

            if (kb.colorblindMode) {
                for (size_t i = 0; i < currentLevel.draggables.size(); i++) {
                    std::string label = ColorToDisplayName(currentLevel.draggables[i].color);
                    Vector3 labelPos = Vector3Add(run.draggablePos[i], Vector3{ 0, currentLevel.draggables[i].size.y / 2.0f + 0.3f, 0 });
                    Vector2 sp = GetWorldToScreen(labelPos, camera);
                    if (sp.x < -100 || sp.x > screenWidth + 100 || sp.y < -100 || sp.y > screenHeight + 100) continue;
                    int tw = MeasureText(label.c_str(), 16);
                    DrawText(label.c_str(), (int)sp.x - tw / 2 + 1, (int)sp.y + 1, 16, BLACK);
                    DrawText(label.c_str(), (int)sp.x - tw / 2, (int)sp.y, 16, WHITE);
                }
                for (size_t i = 0; i < currentLevel.pads.size(); i++) {
                    std::string label = ColorToDisplayName(currentLevel.pads[i].color);
                    Vector2 sp = GetWorldToScreen(currentLevel.pads[i].position, camera);
                    if (sp.x < -100 || sp.x > screenWidth + 100 || sp.y < -100 || sp.y > screenHeight + 100) continue;
                    int tw = MeasureText(label.c_str(), 14);
                    DrawText(label.c_str(), (int)sp.x - tw / 2 + 1, (int)sp.y + 1, 14, BLACK);
                    DrawText(label.c_str(), (int)sp.x - tw / 2, (int)sp.y, 14, WHITE);
                }
            }

            if (run.hintTimer > 0.0f && !run.solved && !currentLevel.switches.empty() && run.currentStep < (int)run.sequence.size()) {
                int targetIdx = run.sequence[run.currentStep];
                Vector3 targetPos = currentLevel.switches[targetIdx].position;
                float bounce = 1.6f + 0.2f * sinf((float)GetTime() * 6.0f);
                Vector3 arrowPos = Vector3Add(targetPos, Vector3{ 0, bounce, 0 });
                Vector2 sp = GetWorldToScreen(arrowPos, camera);
                DrawText("v", (int)sp.x - 6, (int)sp.y - 12, 28, GOLD);
                std::string hintText = "Prossimo: " + currentLevel.switches[targetIdx].name;
                int htw = MeasureText(hintText.c_str(), 20);
                DrawText(hintText.c_str(), screenWidth / 2 - htw / 2, 120, 20, GOLD);
            }

            // --- HUD ---
            DrawText(currentLevel.name.c_str(), 10, 10, 22, DARKBLUE);
            DrawText("Muoviti con WASD, SPAZIO per saltare. Tocca un interruttore e premi E per attivarlo.",
                      10, 36, 16, DARKGRAY);

            if (!currentLevel.switches.empty()) {
                std::string seqText = "Sequenza: ";
                for (size_t k = 0; k < run.sequence.size(); k++) {
                    seqText += currentLevel.switches[run.sequence[k]].name;
                    if (k + 1 < run.sequence.size()) seqText += " > ";
                }
                DrawText(seqText.c_str(), 10, 58, 18, BLACK);
                DrawText(TextFormat("Passo attuale: %d / %d", run.currentStep, (int)run.sequence.size()),
                          10, 80, 18, DARKBLUE);
            } else {
                DrawText("Livello senza interruttori (usa pedane e casse)", 10, 58, 18, DARKGREEN);
            }

            DrawText(TextFormat("Tempo: %s", FormatTime(run.elapsedTime).c_str()), 10, 102, 18, DARKGREEN);
            DrawText(TextFormat("Mondo: %d / 4", run.currentSubworld + 1), 10, 124, 18, PURPLE);
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
    UnloadModel(crateModel);
    CloseWindow();
    return 0;
}