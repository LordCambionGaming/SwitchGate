# Puzzle 3D - Interruttori

Gioco 3D in C++ con [raylib](https://www.raylib.com/), con:
- **Menu principale** con musica di sottofondo (mp3, in loop).
- **Livelli caricati da file JSON**: chiunque puo' creare un livello nuovo
  scrivendo un file `.json` nella cartella `levels/`, senza toccare il codice.
- **Fisica di base**: gravita', salto, piattaforme a piu' altezze, ostacoli
  solidi e "pozzi" (i vuoti tra le piattaforme fanno cadere il giocatore, che
  viene rimandato al punto di partenza del livello).

## Come si gioca
- **Frecce direzionali / WASD**: muovi il personaggio (la sfera arancione).
- **SPAZIO**: salta (utile per raggiungere piattaforme rialzate).
- **Mouse (click sinistro)**: clicca gli interruttori colorati, **nell'ordine
  mostrato in alto**, restando abbastanza vicino.
- Sbagliando l'ordine il progresso del livello si azzera.
- Completata la sequenza, la porta si abbassa: raggiungi il cerchio verde per vincere.
- **R**: ricomincia il livello corrente (nuova sequenza casuale, se il livello la prevede).
- **ESC**: torna alla selezione dei livelli.
- **M**: attiva/disattiva la musica del menu.

## Come compilarlo

### Windows con Visual Studio
1. Apri **Visual Studio 2022** (workload "Sviluppo di applicazioni desktop con C++").
2. `File -> Apri -> Cartella...` e seleziona la cartella del progetto.
3. Visual Studio rileva `CMakeLists.txt` e configura tutto da solo (la prima
   volta scarica raylib e nlohmann/json da GitHub: serve internet).
4. Seleziona `Puzzle3D.exe` come target di avvio e premi **Ctrl+F5**.

In alternativa, da terminale:
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
L'eseguibile sara' in `build\Release\Puzzle3D.exe`.

### Linux/macOS
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/Puzzle3D
```

Ad ogni build, le cartelle `levels/` e `assets/` vengono copiate automaticamente
accanto all'eseguibile, cosi' il gioco trova sia i livelli JSON che la musica.

## Struttura del progetto
```
puzzle3d/
├── CMakeLists.txt        # scarica raylib + nlohmann/json via FetchContent
├── src/
│   ├── main.cpp          # menu, stato del gioco, fisica, rendering
│   ├── Level.h            # strutture dati di un livello
│   └── Level.cpp           # caricamento/parsing dei file JSON dei livelli
├── levels/                # livelli inclusi (puoi aggiungerne quanti vuoi!)
│   ├── level1_classico.json
│   ├── level2_ostacoli.json
│   └── level3_scalinata.json
├── assets/
│   └── audio/
│       └── menu_theme.mp3 # musica del menu (placeholder generato, sostituiscila liberamente)
└── README.md
```

## Creare un nuovo livello (JSON)

Basta aggiungere un file `.json` dentro `levels/`. Al prossimo avvio del gioco
(o premendo "AGGIORNA ELENCO" nella schermata di selezione) comparira' nella lista.

Schema completo, con tutti i campi disponibili:

```jsonc
{
  // Nome mostrato nella lista dei livelli
  "name": "Il mio livello",

  // Testo descrittivo opzionale, mostrato sotto al nome
  "description": "Una breve descrizione del livello.",

  // Posizione di partenza del giocatore [x, y, z]
  "player_start": [0, 1.0, 8],

  // Se true (default) la gravita' e' attiva e si puo' saltare con SPAZIO.
  // Se false il giocatore fluttua alla quota di partenza (utile per livelli
  // puramente "puzzle" senza piattaforme).
  "gravity": true,

  // Quota sotto la quale il giocatore viene considerato "caduto" e rimandato
  // al punto di partenza (default -8). Alzala/abbassala in base all'altezza
  // delle tue piattaforme.
  "fall_reset_y": -8,

  // Elenco degli interruttori da attivare, nell'ordine corretto (vedi sotto
  // "random_sequence"/"sequence" per decidere l'ordine di gioco).
  "switches": [
    { "position": [-6, 0.5, -4], "color": "red",    "name": "ROSSO" },
    { "position": [-2, 0.5, -4], "color": "blue",   "name": "BLU" },
    { "position": [2,  0.5, -4], "color": "green",  "name": "VERDE" },
    { "position": [6,  0.5, -4], "color": "yellow", "name": "GIALLO" }
  ],

  // Se true (default), a ogni partita/reset la sequenza da rispettare viene
  // mischiata a caso tra gli interruttori sopra elencati.
  "random_sequence": true,

  // Se "random_sequence" e' false, questa e' la sequenza fissa da rispettare:
  // un array con gli INDICI (0-based) degli interruttori sopra, nell'ordine
  // in cui vanno cliccati. Deve contenere ogni indice una sola volta.
  "sequence": [2, 0, 3, 1],

  // Parallelepipedi su cui si puo' camminare/saltare (il "pavimento" e i
  // gradini/piattaforme rialzate). Se ometti questo campo, viene generato
  // un pavimento piatto di default. Lasciare un vuoto (gap) tra due
  // piattaforme crea automaticamente un "pozzo": ci si cade dentro e si
  // torna al punto di partenza.
  "platforms": [
    { "position": [0, -0.25, 0], "size": [24, 0.5, 24], "color": "lightgray" },
    { "position": [0, 1.0, -8],  "size": [4, 0.5, 4],   "color": "gray" }
  ],

  // Parallelepipedi solidi trattati come muri a tutta altezza: bloccano il
  // movimento orizzontale, vanno aggirati (non scavalcati saltando).
  "obstacles": [
    { "position": [3, 1.5, -3], "size": [1, 3, 6], "color": "brown" }
  ],

  // La porta che si abbassa quando il puzzle e' risolto.
  "door": { "position": [0, 0, -9], "size": [4, 3, 0.5] },

  // Il cerchio-traguardo: raggiungerlo (dopo aver risolto il puzzle) fa vincere.
  "exit": { "position": [0, 0, -11], "radius": 1.5 }
}
```

Note sui campi colore (`"color"`): puoi usare nomi come `"red"`, `"blue"`,
`"green"`, `"yellow"`, `"orange"`, `"purple"`, `"pink"`, `"gold"`, `"lime"`,
`"skyblue"`, `"white"`, `"black"`, `"gray"`, `"lightgray"`, `"darkgray"`,
`"brown"`, `"maroon"`, `"beige"` (funzionano anche i corrispettivi italiani
come `"rosso"`, `"blu"`, `"verde"`...), oppure un array RGB(A) tipo
`[200, 30, 30]` o `[200, 30, 30, 255]`.

Se un file `.json` ha un errore di sintassi o dati mancanti, il gioco non si
blocca: il livello viene semplicemente escluso dall'elenco e l'errore viene
mostrato nella schermata di selezione dei livelli.

### Sostituire la musica del menu
Il file `assets/audio/menu_theme.mp3` incluso e' un placeholder generato
proceduralmente. Per usare una tua canzone, sostituisci semplicemente quel
file con un altro mp3 con lo stesso nome (o cambia il percorso nel codice,
in `main.cpp`, alla costante `musicPath`).

## Note tecniche
- raylib e nlohmann/json vengono scaricati automaticamente da GitHub tramite
  `FetchContent`: non serve installarli manualmente, ma la prima configurazione
  richiede una connessione internet.
- La fisica e' volutamente semplice (gravita' + salto + collisioni AABB con
  pavimento/ostacoli), pensata come base facilmente estendibile.
