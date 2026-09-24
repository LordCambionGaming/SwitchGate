#pragma once

#include "raylib.h"
#include "Level.h"
#include "Lighting.h"
#include <vector>

static const float RENDER_DISTANCE = 60.0f;

// True se un oggetto (approssimato a una sfera) puo' ricadere nel campo visivo
// della camera: usato per saltare la chiamata di disegno per tutto cio' che
// e' troppo lontano o fuori dal cono di vista.
bool IsVisibleToCamera(const Camera3D& camera, Vector3 objPos, float objRadius);

// Stato dinamico necessario per disegnare la scena di un livello: quanto sono
// aperte le porte, quali interruttori/pedane/ricevitori sono attivi, dove si
// trovano in questo momento le casse trascinabili, e il fascio di luce gia'
// calcolato. E' lo stesso identico stato sia durante una partita vera, sia
// in un'anteprima statica (usata dall'editor).
struct SceneRenderState {
    std::vector<float> doorHeights;          // altezza attuale di ogni porta (0 = aperta)
    std::vector<bool> switchActivated;
    std::vector<bool> padPressed;
    std::vector<bool> receiverLit;
    std::vector<LightBeamSegment> beams;
    std::vector<Vector3> draggablePositions; // stessa dimensione/ordine di level.draggables

    bool exitOpen = false;

    bool showPlayer = false;
    Vector3 playerPosition{ 0, 0, 0 };
    float playerRadius = 0.5f;

    // Sub-mondo da disegnare: -1 = mostra tutto (usato dall'editor in
    // modalita' "Tutti i mondi"), 0-3 = mostra solo gli oggetti con quel
    // subworld piu' quelli condivisi (subworld == -1 sull'oggetto stesso).
    int subworld = -1;
    int selectedIndex = -1;
    int selectedType = -1;
};

// Stato "neutro" per mostrare un livello senza una partita in corso (usato
// dall'editor): porte chiuse, niente attivato, casse nella loro posizione di
// partenza, nessun giocatore disegnato.
SceneRenderState MakeIdleSceneState(const LevelData& level);

// Disegna tutti gli oggetti del livello: piattaforme, ostacoli, casse,
// pedane, interruttori, porte, specchi, emettitori, ricevitori, i fasci di
// luce, l'uscita e, se richiesto, il giocatore.
//
// Va chiamata tra BeginMode3D(camera) e EndMode3D(), passando lo stesso
// 'camera' qui e li'. 'crateModel' e' opzionale: se nullptr le casse vengono
// disegnate come semplici cubi colorati invece che col modello testurizzato
// usato dal gioco vero e proprio.
void DrawLevelScene(const LevelData& level, const SceneRenderState& state, const Camera3D& camera, const Model* crateModel = nullptr);

bool IsSelectedInEditor(const SceneRenderState& state, int objType, int objIndex);