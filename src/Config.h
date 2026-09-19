#pragma once
#include <string>

struct KeyBindings {
    int moveUp = 87;     // W
    int moveDown = 83;   // S
    int moveLeft = 65;   // A
    int moveRight = 68;  // D
    int jump = 32;       // SPACE
    int resetLevel = 82; // R
    int back = 256;      // ESCAPE
    int toggleMusic = 77;// M
    int interact = 69;   // E
    int rotateLeft = 263;  // FRECCIA SINISTRA
    int rotateRight = 262; // FRECCIA DESTRA
    float mouseSensitivity = 1.0f;
};

// Nome leggibile di un tasto (es. 341 -> "LEFT_CONTROL", 65 -> "A").
std::string KeyToName(int key);

// Converte un nome leggibile nel codice tasto raylib corrispondente.
// Ritorna -1 se il nome non e' riconosciuto.
int NameToKey(const std::string& name);

// Carica i tasti da file. Se il file non esiste o e' invalido, 'out' resta
// invariato (di solito gia' impostato ai valori di default) e ritorna false.
bool LoadKeyBindings(const std::string& path, KeyBindings& out);

// Salva i tasti su file in formato JSON leggibile.
bool SaveKeyBindings(const std::string& path, const KeyBindings& kb);