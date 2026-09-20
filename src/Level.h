#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// Un interruttore da cliccare in un certo ordine.
struct LevelSwitch {
    Vector3 position{ 0, 0.5f, 0 };
    Color color = RED;
    std::string name = "?";
};

// Un parallelepipedo generico usato sia per le piattaforme (pavimento/gradini)
// sia per gli ostacoli (muri pieni che bloccano il movimento).
struct LevelBox {
    Vector3 position{ 0, 0, 0 };
    Vector3 size{ 1, 1, 1 };
    Color color = LIGHTGRAY;
};

// Una porta che si apre quando un interruttore specifico viene attivato
// (linkedSwitch = indice in LevelData::switches), oppure quando l'intero
// puzzle e' risolto se linkedSwitch e' -1. "rotated" scambia larghezza (X) e
// profondita' (Z), per poterla orientare anche su un muro laterale.
struct LevelDoor {
    Vector3 position{ 0, 0, -9 };
    Vector3 size{ 4, 3, 0.5f };
    Color color = DARKBROWN;
    int linkedSwitch = -1;
    bool rotated = false;
};

// Dimensioni della porta come vanno effettivamente usate per disegno e
// collisioni: se "rotated" e' true, X e Z sono scambiate.
inline Vector3 GetDoorEffectiveSize(const LevelDoor& door) {
    if (!door.rotated) return door.size;
    return Vector3{ door.size.z, door.size.y, door.size.x };
}

// Una pedana a pressione: si attiva quando una cassa trascinabile dello
// stesso colore ci si ferma sopra, e in quel caso puo' aprire una porta
// collegata (linkedDoor = indice in LevelData::doors, -1 = nessun effetto).
struct LevelPad {
    Vector3 position{ 0, 0.05f, 0 };
    Vector3 size{ 1.5f, 0.1f, 1.5f };
    Color color = RED;
    int linkedDoor = -1;
};

// Tutti i dati che descrivono un livello giocabile.
struct LevelData {
    std::string filePath;              // percorso del file .json di origine
    std::string name = "Livello";
    std::string description;

    Vector3 playerStart{ 0.0f, 1.0f, 8.0f };
    bool gravityEnabled = true;

    std::vector<LevelSwitch> switches;
    bool randomSequence = true;        // se true la sequenza viene mischiata a ogni partita/reset
    std::vector<int> fixedSequence;    // usata solo se randomSequence == false

    std::vector<LevelBox> platforms;   // superfici su cui camminare/saltare (il "pavimento")
    std::vector<LevelBox> obstacles;   // muri pieni (bloccano il movimento orizzontale)
    std::vector<LevelBox> draggables;  // casse che il giocatore puo' trascinare col mouse
    std::vector<LevelPad> pads;        // pedane a colore che aprono porte collegate

    std::vector<LevelDoor> doors;

    Vector3 exitPosition{ 0, 0, -11 };
    float exitRadius = 1.5f;

    // Sotto questa quota il giocatore e' "caduto" (es. in un buco tra le piattaforme)
    // e viene rimandato al punto di partenza: e' cosi' che nascono i "pozzi" (pits),
    // semplicemente lasciando un vuoto tra due piattaforme nel file JSON.
    float fallResetY = -8.0f;
};

// Carica e tiene in memoria l'elenco dei livelli trovati in una cartella.
class LevelManager {
public:
    // Rilegge la cartella da disco e ricostruisce l'elenco dei livelli validi.
    // I file che non si riescono a interpretare vengono segnalati in GetErrors().
    void ScanDirectory(const std::string& directory);

    const std::vector<LevelData>& GetLevels() const { return levels; }
    const std::vector<std::string>& GetErrors() const { return errors; }

    // Carica un singolo file. Ritorna true e riempie 'out' se ha successo,
    // altrimenti ritorna false e riempie 'errorMessage'.
    static bool LoadFromFile(const std::string& path, LevelData& out, std::string& errorMessage);

    // Scrive un LevelData su disco in formato JSON, con lo stesso schema
    // riconosciuto da LoadFromFile. Usata dall'editor di livelli.
    static bool SaveToFile(const std::string& path, const LevelData& level, std::string& errorMessage);

private:
    std::vector<LevelData> levels;
    std::vector<std::string> errors;
};