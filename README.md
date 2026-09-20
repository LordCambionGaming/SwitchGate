# Switch Gate

**Switch Gate** è un puzzle game tridimensionale sviluppato in C++ utilizzando la libreria **Raylib**. L'obiettivo del gioco è risolvere enigmi ambientali attivando sequenze di interruttori, risolvendo puzzle con casse e pedane colorate, e sbloccando porte logiche per raggiungere l'uscita nel minor tempo possibile.

---

## Caratteristiche Principali

* **Meccaniche di Puzzle Avanzate**: 
  * Interruttori con sequenze fisse o casuali.
  * Porte con logica booleana (`AND` / `OR`) collegate a interruttori multipli o pedane a pressione.
  * Casse trascinabili e pedane sensibili al colore.
* **Editor di Livelli Integrato**: Crea e modifica i tuoi livelli direttamente dall'interfaccia di gioco, testandoli in tempo reale prima di salvarli.
* **Formato JSON**: I livelli sono memorizzati in file `.json` leggeri e facili da condividere o modificare manualmente nella cartella `levels/`.
* **Personalizzazione Completa**: Riassegnazione dei tasti di controllo (Keybindings), modalità daltonici e gestione dei record (Highscores) locali.

---

## Controlli Predefiniti

| Azione | Tasto |
| :--- | :--- |
| **Movimento** | `W`, `A`, `S`, `D` |
| **Salto** | `Spazio` |
| **Interagisci** | `E` |
| **Ruota Camera** | Frecce `Sinistra` / `Destra` (o click del mouse) |
| **Suggerimento** | `H` |
| **Ricomincia** | `R` |
| **Menu / Indietro** | `ESC` |
| **Attiva/Disattiva Musica** | `M` |

---

## Creazione Livelli (JSON)

I livelli personalizzati possono essere creati tramite l'editor interno o scritti manualmente in formato JSON all'interno della cartella `levels/`. 

Esempio di struttura di base per una porta logica `AND` collegata a più interruttori:

```json
{
  "name": "Nome Livello",
  "description": "Descrizione del puzzle.",
  "gravity": true,
  "random_sequence": true,
  "player_start": [0.0, 1.0, 8.0],
  "exit": {
    "position": [0.0, 0.0, -11.0],
    "radius": 1.5
  },
  "doors": [
    {
      "color": "darkbrown",
      "position": [0.0, 0.0, -9.0],
      "size": [4.0, 3.0, 0.5],
      "linked_switches": [0, 1],
      "logic_op": "AND",
      "rotated": false
    }
  ],
  "switches": [
    { "name": "ROSSO", "color": "red", "position": [3.0, 0.5, 0.0] },
    { "name": "BLU", "color": "blue", "position": [-3.0, 0.5, 0.0] }
  ]
}
