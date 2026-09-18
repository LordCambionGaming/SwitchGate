#pragma once

#include "raylib.h"
#include <string>

// Offset applicato alla posizione del mouse usata per il rilevamento dei
// click nei pannelli scrollabili (es. la barra laterale dell'editor): il
// contenuto viene disegnato spostato via rlTranslatef, quindi anche il
// controllo di collisione deve tenerne conto. Chi apre un pannello
// scrollabile lo imposta prima di disegnare e lo azzera subito dopo.
inline float g_uiScrollOffsetY = 0.0f;

inline bool DrawButton(Rectangle rect, const char* text, int fontSize, Color base, Color hover, Color textColor) {
    Vector2 mouse = GetMousePosition();
    mouse.y += g_uiScrollOffsetY;
    bool isHover = CheckCollisionPointRec(mouse, rect);
    DrawRectangleRounded(rect, 0.2f, 8, isHover ? hover : base);
    DrawRectangleLinesEx(rect, 2.0f, Fade(BLACK, 0.35f));
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(rect.x + (rect.width - textWidth) / 2), (int)(rect.y + (rect.height - fontSize) / 2), fontSize, textColor);
    return isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// Bottone piccolo pensato per "+"/"-"/frecce, con solo bordo e testo grande.
inline bool DrawMiniButton(Rectangle rect, const char* text, Color base, Color hover) {
    Vector2 mouse = GetMousePosition();
    mouse.y += g_uiScrollOffsetY;
    bool isHover = CheckCollisionPointRec(mouse, rect);
    DrawRectangleRec(rect, isHover ? hover : base);
    DrawRectangleLinesEx(rect, 1.5f, Fade(BLACK, 0.4f));
    int fontSize = (int)(rect.height * 0.6f);
    int tw = MeasureText(text, fontSize);
    DrawText(text, (int)(rect.x + (rect.width - tw) / 2), (int)(rect.y + (rect.height - fontSize) / 2), fontSize, BLACK);
    return isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// Casella di testo semplice: click per attivare, digitazione per scrivere.
// Ritorna true nel frame in cui il testo e' cambiato.
inline bool TextBoxUpdate(Rectangle rect, std::string& text, bool& active, size_t maxLen, const char* placeholder = nullptr) {
    Vector2 mouse = GetMousePosition();
    mouse.y += g_uiScrollOffsetY;
    bool hover = CheckCollisionPointRec(mouse, rect);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) active = hover;

    bool changed = false;
    if (active) {
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (ch >= 32 && ch <= 125 && text.size() < maxLen) {
                text += (char)ch;
                changed = true;
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) {
            text.pop_back();
            changed = true;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) active = false;
    }

    DrawRectangleRec(rect, active ? Fade(SKYBLUE, 0.25f) : Fade(LIGHTGRAY, 0.5f));
    DrawRectangleLinesEx(rect, 2.0f, active ? BLUE : GRAY);

    bool showPlaceholder = text.empty() && !active && placeholder != nullptr;
    const std::string& shown = showPlaceholder ? std::string(placeholder) : text;
    Color textColor = showPlaceholder ? GRAY : BLACK;
    DrawText(shown.c_str(), (int)rect.x + 6, (int)(rect.y + rect.height / 2 - 8), 16, textColor);

    if (active && (((int)(GetTime() * 2.0)) % 2 == 0)) {
        int tw = MeasureText(text.c_str(), 16);
        DrawText("|", (int)rect.x + 6 + tw, (int)(rect.y + rect.height / 2 - 8), 16, BLACK);
    }
    return changed;
}