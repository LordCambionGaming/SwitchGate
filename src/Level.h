#pragma once

#include "raylib.h"
#include "objects/Switch.h"
#include "objects/Box.h"
#include "objects/Door.h"
#include "objects/Pad.h"
#include "objects/Mirror.h"
#include "objects/Emitter.h"
#include "objects/Receiver.h"
#include <string>
#include <vector>

// Tutti i dati che descrivono un livello giocabile. Ogni tipo di oggetto
// (interruttori, porte, specchi...) e' definito nel proprio file sotto
// objects/; qui vengono solo raccolti in un'unica struttura.
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

    std::vector<LevelMirror> mirrors;
    std::vector<LevelEmitter> emitters;
    std::vector<LevelReceiver> receivers;

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
