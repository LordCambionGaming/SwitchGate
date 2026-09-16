

#include "raylib.h"
#include "raymath.h"
#include "Level.h"
#include <vector>
#include <string>
#include <algorithm>
#include <random>


// Stato globale dell'applicazione


enum class GameState { MENU, LEVEL_SELECT, PLAYING, HELP };

struct Player {
    Vector3 position{ 0, 1.0f, 8 };
    Vector3 velocity{ 0, 0, 0 };
    float radius = 0.5f;
    bool onGround = false;
};

static const float GRAVITY = -24.0f;
static const float JUMP_SPEED = 9.0f;
static const float MOVE_SPEED = 6.0f;
static const float INTERACT_RANGE = 3.0f;


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
static void ResolveObstacles(Vector3& pos, float radius, const std::vector<LevelBox>& obstacles) {
    for (const auto& o : obstacles) {
        float minX = o.position.x - o.size.x / 2.0f - radius;
        float maxX = o.position.x + o.size.x / 2.0f + radius;
        float minZ = o.position.z - o.size.z / 2.0f - radius;
        float maxZ = o.position.z + o.size.z / 2.0f + radius;

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
}

static void UpdatePlayerPhysics(Player& player, const LevelData& level, Vector3 moveInput, float dt) {
    // Movimento orizzontale
    if (moveInput.x != 0.0f || moveInput.z != 0.0f) {
        moveInput = Vector3Normalize(moveInput);
        player.position.x += moveInput.x * MOVE_SPEED * dt;
        player.position.z += moveInput.z * MOVE_SPEED * dt;
    }
    ResolveObstacles(player.position, player.radius, level.obstacles);
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

    if (IsKeyPressed(KEY_SPACE) && player.onGround && level.gravityEnabled) {
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
    float doorHeight = 3.0f;
    float flashTimer = 0.0f;
    bool flashWrong = false;
    Player player;
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
    run.doorHeight = level.doorSize.y;
    run.flashTimer = 0.0f;
    run.flashWrong = false;
    run.player.position = level.playerStart;
    run.player.velocity = { 0, 0, 0 };
    run.player.onGround = false;
}


// Interfaccia: bottoni semplici cliccabili col mouse


static bool DrawButton(Rectangle rect, const char* text, int fontSize, Color base, Color hover, Color textColor) {
    Vector2 mouse = GetMousePosition();
    bool isHover = CheckCollisionPointRec(mouse, rect);
    DrawRectangleRounded(rect, 0.2f, 8, isHover ? hover : base);
    DrawRectangleRoundedLines(rect, 0.2f, 8, 2.0f, Fade(BLACK, 0.35f));
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(rect.x + (rect.width - textWidth) / 2), (int)(rect.y + (rect.height - fontSize) / 2), fontSize, textColor);
    return isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}


// MAIN


int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Puzzle 3D - Interruttori");
    SetTargetFPS(60);

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

        if (IsKeyPressed(KEY_M)) musicMuted = !musicMuted;


        // Aggiornamento logico per stato

        if (state == GameState::PLAYING) {
            Vector3 move = { 0, 0, 0 };
            if (IsKeyDown(KEY_W)) move.z -= 1.0f;
            if (IsKeyDown(KEY_S)) move.z += 1.0f;
            if (IsKeyDown(KEY_A)) move.x -= 1.0f;
            if (IsKeyDown(KEY_D)) move.x += 1.0f;

            UpdatePlayerPhysics(run.player, currentLevel, move, dt);

            // La telecamera segue il giocatore: cosi' funziona correttamente anche
            // su livelli JSON creati dall'utente, di qualunque dimensione o forma,
            // invece di restare fissa su un'inquadratura pensata per un solo livello.
            camera.target = run.player.position;
            camera.position = Vector3{
                run.player.position.x,
                run.player.position.y + 10.0f,
                run.player.position.z + 10.0f
            };

            if (!run.solved && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Ray ray = GetMouseRay(GetMousePosition(), camera);
                for (int i = 0; i < (int)currentLevel.switches.size(); i++) {
                    if (run.activated[i]) continue;
                    const Vector3& sp = currentLevel.switches[i].position;
                    BoundingBox box = {
                        Vector3{ sp.x - 0.5f, sp.y - 0.5f, sp.z - 0.5f },
                        Vector3{ sp.x + 0.5f, sp.y + 0.5f, sp.z + 0.5f }
                    };
                    RayCollision col = GetRayCollisionBox(ray, box);
                    if (col.hit) {
                        float dist = Vector3Distance(run.player.position, sp);
                        if (dist > INTERACT_RANGE) {
                            run.flashWrong = true;
                            run.flashTimer = 1.0f;
                            break;
                        }
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
            }

            if (run.flashTimer > 0.0f) run.flashTimer -= dt; else run.flashWrong = false;

            if (run.solved && run.doorHeight > 0.01f) {
                run.doorHeight -= dt * 2.0f;
                if (run.doorHeight < 0.0f) run.doorHeight = 0.0f;
            }

            if (run.solved && !run.won) {
                float dx = run.player.position.x - currentLevel.exitPosition.x;
                float dz = run.player.position.z - currentLevel.exitPosition.z;
                float r = currentLevel.exitRadius;
                if (dx * dx + dz * dz < r * r) run.won = true;
            }

            if (IsKeyPressed(KEY_R)) {
                StartRun(run, currentLevel, rng);
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                state = GameState::LEVEL_SELECT;
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

            Rectangle playBtn = { screenWidth / 2.0f - 140, 300, 280, 56 };
            Rectangle helpBtn = { screenWidth / 2.0f - 140, 370, 280, 56 };
            Rectangle quitBtn = { screenWidth / 2.0f - 140, 440, 280, 56 };

            if (DrawButton(playBtn, "GIOCA", 26, Fade(DARKBLUE, 0.85f), DARKBLUE, WHITE)) {
                levelManager.ScanDirectory("levels");
                state = GameState::LEVEL_SELECT;
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
                "- Muoviti con W A S D.",
                "- SPAZIO per saltare (utile su piattaforme rialzate).",
                "- Clicca col MOUSE gli interruttori colorati nell'ordine mostrato in alto.",
                "- Devi essere abbastanza vicino a un interruttore per attivarlo.",
                "- Sbagliando l'ordine il progresso del livello si azzera.",
                "- Completata la sequenza la porta si apre: raggiungi il cerchio verde per vincere.",
                "- Attento ai vuoti tra le piattaforme: cadendo torni al punto di partenza.",
                "- R: ricomincia il livello corrente (nuova sequenza se e' casuale).",
                "- ESC: torna alla selezione dei livelli.",
                "- M: attiva/disattiva la musica del menu.",
                "",
                "Puoi creare nuovi livelli scrivendo file .json nella cartella 'levels/'.",
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
        else if (state == GameState::PLAYING) {
            BeginMode3D(camera);

            for (const auto& p : currentLevel.platforms) {
                DrawCube(p.position, p.size.x, p.size.y, p.size.z, p.color);
                DrawCubeWires(p.position, p.size.x, p.size.y, p.size.z, Fade(BLACK, 0.25f));
            }
            for (const auto& o : currentLevel.obstacles) {
                DrawCube(o.position, o.size.x, o.size.y, o.size.z, o.color);
                DrawCubeWires(o.position, o.size.x, o.size.y, o.size.z, DARKGRAY);
            }

            for (size_t i = 0; i < currentLevel.switches.size(); i++) {
                const auto& s = currentLevel.switches[i];
                Color c = run.activated[i] ? s.color : Fade(s.color, 0.4f);
                DrawCube(s.position, 1, 1, 1, c);
                DrawCubeWires(s.position, 1, 1, 1, DARKGRAY);
            }

            if (run.doorHeight > 0.01f) {
                Vector3 dp = { currentLevel.doorPosition.x, run.doorHeight / 2.0f, currentLevel.doorPosition.z };
                DrawCube(dp, currentLevel.doorSize.x, run.doorHeight, currentLevel.doorSize.z, DARKBROWN);
                DrawCubeWires(dp, currentLevel.doorSize.x, run.doorHeight, currentLevel.doorSize.z, BLACK);
            }

            DrawCircle3D(currentLevel.exitPosition, currentLevel.exitRadius, Vector3{ 1, 0, 0 }, 90.0f,
                          run.solved ? GREEN : GRAY);

            DrawSphere(run.player.position, run.player.radius, ORANGE);
            DrawSphereWires(run.player.position, run.player.radius, 8, 8, MAROON);

            EndMode3D();

            // --- HUD ---
            DrawText(currentLevel.name.c_str(), 10, 10, 22, DARKBLUE);
            DrawText("Muoviti con WASD, SPAZIO per saltare. Clicca gli interruttori nell'ordine giusto.",
                      10, 36, 16, DARKGRAY);

            std::string seqText = "Sequenza: ";
            for (size_t k = 0; k < run.sequence.size(); k++) {
                seqText += currentLevel.switches[run.sequence[k]].name;
                if (k + 1 < run.sequence.size()) seqText += " > ";
            }
            DrawText(seqText.c_str(), 10, 58, 18, BLACK);
            DrawText(TextFormat("Passo attuale: %d / %d", run.currentStep, (int)run.sequence.size()),
                      10, 80, 18, DARKBLUE);
            DrawText("ESC: torna alla selezione livelli   |   R: ricomincia", 10, screenHeight - 26, 16, DARKGRAY);

            if (run.flashWrong) {
                DrawText("SBAGLIATO! Riprova.", screenWidth / 2 - 100, 90, 24, RED);
            }
            if (run.solved && !run.won) {
                DrawText("Porta aperta! Vai verso il cerchio verde per uscire.", screenWidth / 2 - 260, 90, 24, DARKGREEN);
            }
            if (run.won) {
                DrawRectangle(0, screenHeight / 2 - 70, screenWidth, 160, Fade(BLACK, 0.6f));
                DrawText("HAI VINTO!", screenWidth / 2 - 110, screenHeight / 2 - 55, 44, GOLD);
                Rectangle again = { screenWidth / 2 - 220.0f, screenHeight / 2 + 5.0f, 200, 46 };
                Rectangle back  = { screenWidth / 2 + 20.0f,  screenHeight / 2 + 5.0f, 200, 46 };
                if (DrawButton(again, "RIGIOCA", 20, DARKGREEN, GREEN, WHITE)) {
                    StartRun(run, currentLevel, rng);
                }
                if (DrawButton(back, "ALTRI LIVELLI", 20, DARKBLUE, BLUE, WHITE)) {
                    state = GameState::LEVEL_SELECT;
                }
            }
        }

        EndDrawing();
    }

    if (hasMusic) UnloadMusicStream(menuMusic);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
