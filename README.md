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

### Editor livelli (vista 3D)

L'editor mostra il livello in una vera scena 3D (la stessa identica resa
grafica del gioco, specchi e fasci di luce inclusi, aggiornati in tempo
reale man mano che sposti gli oggetti), non piu' una mappa piatta vista
dall'alto:

- **Click sinistro**: seleziona/sposta un oggetto, oppure crea un nuovo
  oggetto con lo strumento attivo (trascina per piattaforme/ostacoli/casse).
- **Trascina col tasto destro**: ruota la visuale attorno al livello.
- **Trascina col tasto centrale**: sposta il punto di osservazione.
- **Rotella del mouse**: zoom in/out.
- **CANC**: elimina l'oggetto selezionato.

L'altezza dei nuovi oggetti e l'angolo di specchi/emettitori si impostano
dai controlli "+/-" nella barra laterale prima di piazzarli; una volta
piazzati si possono affinare dal pannello "OGGETTO SELEZIONATO".

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
```

### Raggi di luce e specchi

Un terzo modo (oltre a interruttori e pedane) per aprire una porta: un **emettitore**
spara un raggio continuo, gli **specchi** lo riflettono, e un **ricevitore** colpito
dal raggio giusto puo' aprire una porta collegata (`linked_door`), esattamente come
fanno le pedane. Il raggio viaggia sempre in orizzontale: basta un angolo in gradi,
niente vettori 3D da calcolare a mano.

- `angle`: `0` = raggio/pannello rivolto verso **+Z**, `90` = verso **+X**, `180` =
  verso **-Z**, `270` = verso **-X** (e qualunque valore intermedio).
- Gli specchi bloccano il raggio solo se la sua quota (`y` dell'emettitore, che resta
  costante lungo tutto il percorso) rientra nell'altezza del pannello (`height`).
- Ostacoli e porte chiuse fermano il raggio; i ricevitori si "accendono" solo se il
  colore del raggio combacia col loro (oppure se il ricevitore ha colore `"white"`,
  che accetta qualunque colore).

```json
{
  "emitters": [
    { "position": [8, 1, 6], "angle": 180, "color": "red" }
  ],
  "mirrors": [
    { "position": [8, 1, 0], "angle": 45, "length": 2.5, "height": 2, "color": "skyblue" },
    { "position": [0, 1, 0], "angle": 45, "length": 2.5, "height": 2, "color": "skyblue" }
  ],
  "receivers": [
    { "position": [0, 1, -8], "radius": 0.6, "color": "red", "linked_door": 0 }
  ]
}
```

Livello di esempio completo: `levels/level6_specchi.json` — il raggio parte
dall'emettitore, rimbalza su entrambi gli specchi ad angolo retto e raggiunge il
ricevitore che apre la porta verso l'uscita.
