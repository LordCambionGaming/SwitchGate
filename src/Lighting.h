#pragma once

#include "raylib.h"
#include "Level.h"
#include <vector>

// Raggi di luce e specchi
//
// I fasci di luce viaggiano sempre orizzontalmente (asse Y costante = quota
// dell'emettitore), quindi tutta la geometria si riduce a un problema 2D nel
// piano XZ: molto piu' semplice da calcolare e, soprattutto, da capire per
// chi scrive i livelli in JSON (basta un angolo in gradi, niente vettori 3D).

// Un pezzo di raggio disegnabile (dal punto di partenza al primo colpo, o da
// uno specchio al successivo).
struct LightBeamSegment {
    Vector3 a;
    Vector3 b;
    Color color;
};

// Direzione (nel piano XZ) corrispondente a un angolo in gradi: 0 = +Z,
// 90 = +X, 180 = -Z, 270 = -X. Usata sia per gli emettitori (direzione del
// raggio) sia per gli specchi (direzione del pannello).
Vector3 DirFromAngleDeg(float angleDeg);

// Calcola il tragitto (con eventuali riflessioni sugli specchi) di ogni
// emettitore del livello, fermandosi a ostacoli/porte chiuse/casse o al
// primo ricevitore colpito. 'draggablePositions' sono le posizioni ATTUALI
// delle casse trascinabili (non quelle di partenza salvate nel livello):
// una cassa spinta davanti a uno specchio blocca il raggio esattamente come
// un muro. Riempie 'receiverLit' (un bool per ogni ricevitore: true se in
// quel momento e' illuminato da un raggio del colore giusto) e ritorna i
// segmenti da disegnare.
// 'subworld' e' il sub-mondo attualmente attivo (vedi RunState::currentSubworld
// in main.cpp): emettitori, specchi, ricevitori, ostacoli, porte e casse con
// un subworld specifico diverso da quello corrente vengono ignorati, come se
// non esistessero in questo momento.
std::vector<LightBeamSegment> ComputeLightBeams(const LevelData& level,
                                                 const std::vector<float>& doorHeights,
                                                 const std::vector<Vector3>& draggablePositions,
                                                 int subworld,
                                                 std::vector<bool>& receiverLit);