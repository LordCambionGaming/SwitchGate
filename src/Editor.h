#pragma once
#include "raylib.h"
#include "Level.h"
#include <string>
#include <vector>

enum class EditorTool { SELECT, PLATFORM, OBSTACLE, SWITCH, START, DOOR, EXIT };
enum class EditorSelType { NONE, PLATFORM, OBSTACLE, SWITCH, START, DOOR, EXIT };

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
    int colorIndex = 0;

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
    float scale = 10.0f;
    Vector2 canvasCenter{ 0, 0 };

    Vector2 WorldToScreen(float x, float z) const;
    Vector2 ScreenToWorld(Vector2 screen) const;
    bool MouseInCanvas() const;
    void RecalcCanvas();

    void SetStatus(const std::string& msg, bool isError);
    void ClearSelection();
    void DeleteSelected();
    void PickAt(Vector2 worldPos);

    void DrawTopBar();
    void DrawSidebar();
    void DrawCanvas();
    void DrawSaveDialog();
    void DrawLoadDialog();
};