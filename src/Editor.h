#pragma once
#include "raylib.h"
#include "Level.h"
#include <string>
#include <vector>

enum class EditorTool { SELECT, PLATFORM, OBSTACLE, DRAGGABLE, SWITCH, START, DOOR, PAD, MIRROR, EMITTER, RECEIVER, EXIT };
enum class EditorSelType { NONE, PLATFORM, OBSTACLE, DRAGGABLE, SWITCH, START, DOOR, PAD, MIRROR, EMITTER, RECEIVER, EXIT };

class LevelEditor {
public:
    void NewLevel();
    void LoadLevel(const LevelData& lvl, const std::string& filePath);
    LevelData BuildLevelData() const { return working; }

    void Update();
    void Draw();

    bool ConsumePlaytestRequest();
    bool ConsumeExitRequest();

private:
    LevelData working;
    std::string currentFilePath;

    EditorTool tool = EditorTool::SELECT;
    EditorSelType selType = EditorSelType::NONE;
    int selIndex = -1;

    bool isDraggingNew = false;
    bool dragStartedInCanvas = false;
    Vector2 dragStartWorld{ 0, 0 };
    bool isDraggingSel = false;
    Vector2 dragOffsetWorld{ 0, 0 };

    float newPlatformTopY = 0.0f;
    float newObstacleHeight = 3.0f;
    float newAngleDeg = 45.0f;     // angolo di default per nuovi specchi/emettitori
    int colorIndex = 0;
    float sidebarScroll = 0.0f;

    // Sub-mondo che si sta visualizzando/costruendo in questo momento:
    // -1 = "Tutti i mondi" (si vede e si modifica tutto, com'era prima),
    // 0-3 = si vedono solo gli oggetti di quel mondo piu' quelli condivisi
    // (subworld -1 sull'oggetto). I nuovi oggetti creati ereditano questo
    // valore, cosi' costruire un mondo alla volta e' naturale.
    int activeSubworld = -1;
    bool GetSelectedSubworld(int& outSw) const;
    void SetSelectedSubworld(int sw);

    // Snap a griglia: arrotonda le coordinate X/Z (piazzamento e trascinamento)
    // al multiplo piu' vicino di gridSize, per rendere facile allineare gli
    // oggetti e costruire livelli simmetrici invece di piazzare tutto a mano libera.
    bool gridSnap = true;
    float gridSize = 1.0f;
    float SnapToGrid(float v) const;

    bool nameActive = false;
    bool descActive = false;
    bool fileNameActive = false;
    bool switchNameActive = false;
    std::string fileNameBuffer;

    bool showSaveBox = false;
    bool showLoadBox = false;
    LevelManager loadBrowser;

    std::string statusMessage;
    float statusTimer = 0.0f;
    bool statusIsError = false;

    bool playtestRequested = false;
    bool exitRequested = false;

    Rectangle canvasRect{ 240, 108, 1010, 560 };

    // Vista 3D del canvas: camera "a orbita" attorno a un punto (camTarget),
    // ruotabile e zoomabile, invece della vecchia proiezione ortogonale
    // dall'alto disegnata a mano con rettangoli 2D.
    Camera3D camera{};
    float camYaw = 35.0f;      // gradi, rotazione orizzontale attorno al target
    float camPitch = 55.0f;    // gradi, 0 = orizzontale, 90 = dall'alto
    float camDistance = 30.0f;
    Vector3 camTarget{ 0, 0, 0 };
    bool isOrbiting = false;
    bool isPanning = false;
    Vector2 lastCamMouse{ 0, 0 };

    void UpdateCameraFromOrbit();
    void UpdateCameraControls();

    // Converte la posizione del mouse in una coordinata (x, z) sul piano
    // orizzontale y=0, proiettando un raggio dalla camera 3D: sostituisce la
    // vecchia mappatura piatta scala/offset, ma restituisce lo stesso tipo
    // (Vector2 dove .x = x mondo, .y = z mondo) cosi' il resto del codice di
    // interazione (selezione, trascinamento, creazione) resta invariato.
    Vector2 ScreenToWorld(Vector2 screen) const;
    Vector2 WorldToScreen(Vector3 worldPos) const;
    bool MouseInCanvas() const;

    void SetStatus(const std::string& msg, bool isError);
    void ClearSelection();
    void DeleteSelected();

    // Seleziona l'oggetto sotto al mouse lanciando un raggio 3D vero e
    // proprio dalla camera (stessa idea del "click" in un motore 3D): ogni
    // oggetto viene testato con la sua forma reale (sfera, box o pannello),
    // non con una proiezione sul piano y=0. Cosi' funziona bene con la
    // camera inclinata e anche cliccando sulla parte alta di un oggetto
    // (es. il bordo superiore di una porta) invece che solo vicino a terra.
    void PickAt();

    // Gizmo di traslazione (le "freccette" stile Blender) per l'oggetto
    // selezionato: una per asse (X rosso, Y verde, Z blu). Trascinandone una
    // l'oggetto si sposta SOLO lungo quell'asse, invece che liberamente sul
    // piano orizzontale come col trascinamento diretto.
    int gizmoDragAxis = -1;          // -1 = nessuno, 0 = X, 1 = Y, 2 = Z
    Vector3 gizmoDragOrigPos{ 0, 0, 0 };
    float gizmoDragStartT = 0.0f;
    static constexpr float kGizmoLength = 1.4f;

    // Legge/scrive la posizione dell'oggetto attualmente selezionato,
    // qualunque sia il suo tipo (switch, specchio, porta, piattaforma...):
    // centralizza l'accesso ai vari campi "position" cosi' il gizmo (e in
    // futuro altri strumenti) non devono conoscere ogni tipo singolarmente.
    bool GetSelectedPosition(Vector3& outPos) const;
    void SetSelectedPosition(Vector3 pos);

    // Punto sull'asse (origine + t*dir) piu' vicino al raggio del mouse:
    // tecnica standard per il trascinamento vincolato a un singolo asse.
    float ClosestTOnAxis(Ray ray, Vector3 axisOrigin, Vector3 axisDir) const;

    void DrawGizmo(Vector3 pos);

    void DrawTopBar();
    void DrawSidebar();
    void DrawCanvas();
    void DrawSaveDialog();
    void DrawLoadDialog();
};