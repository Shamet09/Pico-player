# Saldatura di XSMT sul DAC PCM5102 — istruzioni operative

Guida da banco per collegare il pin **XSMT** del modulo DAC, che nel progetto
originale non era previsto e senza il quale **il lettore funziona ma non esce
audio**.

Tempo richiesto: 10 minuti di prova + 10 minuti di saldatura.
Difficolta': bassa, ma le piazzole sono vicine tra loro — leggi le avvertenze.

---

## 1. Perche' si fa

Il modulo e' un **GY-PCM5102A** (la scheda viola). Il chip PCM5102A ha un pin
`XSMT` che comanda il **soft mute**:

* `XSMT` **alto** (3,3 V) → il DAC suona.
* `XSMT` **basso o flottante** → il DAC e' **muto**, e non da' nessun altro
  segno: l'I2S continua a girare, il display continua a scorrere, il contatore
  del tempo avanza, i brani cambiano da soli. Semplicemente non si sente nulla.

Il PCM5102A **non ha pull-up interni** su quel pin. Sul retro del modulo ci
sono quattro ponticelli a saldare (`H1L`..`H4L`) che servirebbero a fissare il
livello di FLT, DEMP, XSMT e FMT, ma di fabbrica sono spesso lasciati aperti:
in quel caso XSMT resta libero e in pratica tende al basso.

Nella piedinatura del documento di progetto XSMT non compare: erano previsti
solo VIN, GND, LCK, DIN, BCK e SCK. E' quindi molto probabile che non sia mai
stato collegato.

### Come si riconosce che e' proprio questo il problema

Se **il contatore del tempo avanza in tempo reale** (un brano da 3:45 dura
davvero 3:45) e i brani finiscono e cambiano da soli, allora PIO, DMA e I2S
stanno funzionando e i dati escono davvero dal Pico: se cosi' non fosse la
riproduzione si bloccherebbe e il contatore si fermerebbe. Quindi il guasto e'
per forza **a valle del Pico**, e XSMT e' il primo indiziato.

---

## 2. Cosa serve

**Indispensabile**

* Saldatore da 25–40 W, meglio se a temperatura regolabile, **punta fine**
  (1–2 mm). Temperatura: ~330 °C con stagno al piombo, ~360 °C con senza piombo.
* Stagno con **anima di flussante**, diametro 0,5–0,8 mm. Il 60/40 al piombo e'
  molto piu' facile da lavorare del senza piombo.
* Un pezzetto di **filo isolato sottile**, AWG 26–30 (0,14–0,25 mm²). Il filo
  da wire-wrap (Kynar) e' l'ideale. In alternativa va benissimo sacrificare un
  cavetto dupont e usarne il conduttore.
* Tronchesino.

**Molto utile**

* Multimetro con prova di continuita' (il "cicalino").
* Terza mano o un morsetto per tenere fermo il modulo.
* Treccia dissaldante o pompetta, per rimediare a un errore.

**Opzionale**

* Alcol isopropilico e cotton fioc, per pulire il flussante a fine lavoro.
* Una lente o la fotocamera del telefono in zoom, per controllare le saldature.

> **Non hai il saldatore?** La prova temporanea del capitolo 5 ti permette
> comunque di **confermare la diagnosi** tenendo un filo premuto a mano. Per la
> riparazione definitiva, pero', non esistono alternative affidabili: un filo
> solo infilato nel foro fa contatto intermittente, e con XSMT questo significa
> audio che va e viene a caso — peggio che nessun audio, perche' ti fa credere
> che il guasto sia da un'altra parte.

---

## 3. Avvertenze — leggile prima di accendere il saldatore

* **Scollega tutto prima di saldare**: USB, batteria, alimentazione. Mai saldare
  su un circuito alimentato.
* **Non tenere il calore piu' di 2–3 secondi** su una piazzola. Le piazzole di
  questi moduli economici si staccano dal circuito stampato se le cuoci, e a
  quel punto il modulo e' da buttare.
* **Lavora in un posto ventilato.** I fumi del flussante non vanno respirati.
* La punta del saldatore sta a oltre 300 °C: **ustiona all'istante**. Appoggiala
  sempre sul suo supporto, mai sul tavolo.
* Se usi stagno al piombo, **lavati le mani** quando hai finito.
* Occhiali di protezione se ne hai: lo stagno fuso ogni tanto schizza.
* Tocca qualcosa di metallico collegato a terra prima di maneggiare il modulo
  (scarica elettrostatica). Non e' critico, ma costa zero.

---

## 4. Identificare le piazzole

Metti il modulo con il **retro verso di te** e il **connettore a 6 pin in
alto**. Sul lato sinistro c'e' una fila di 9 fori metallizzati, con le
etichette serigrafate accanto.

Contandoli **dall'alto verso il basso**:

| # | Etichetta | Nota |
|---|-----------|------|
| 1 | `FLT`   | |
| 2 | `DEMP`  | |
| 3 | **`XSMT`** | ← **da collegare** |
| 4 | `FMT`   | **NON toccare** — vedi sotto |
| 5 | **`A3V3`** | ← **a questo** (3,3 V del regolatore a bordo) |
| 6 | `AGND`  | **NON toccare** — vedi sotto |
| 7 | `ROUT`  | |
| 8 | `AGND`  | |
| 9 | `LROUT` | |

### Le due trappole

* **`FMT` (4) sta in mezzo fra XSMT e A3V3.** Il filo deve scavalcarlo senza
  toccarlo. Se FMT va alto, il DAC passa dal formato I2S a left-justified: il
  suono **esce sbagliato** invece che assente, il che e' un sintomo molto piu'
  confuso da diagnosticare.
* **`AGND` (6) sta subito dopo A3V3.** Un ponte di stagno fra 5 e 6 e' un
  **cortocircuito sull'alimentazione** del DAC.

Per entrambi i motivi: usa **filo isolato**, non un filo nudo, e cura la
quantita' di stagno.

---

## 5. Prima prova senza saldare — falla, ti fa risparmiare tempo

Il firmware ha un **modo test audio** apposta: manda al DAC un tono continuo a
440 Hz senza toccare la microSD e senza decodificare niente. Siccome il tono e'
continuo, basta tenere un filo premuto a mano per sentire subito l'effetto.

1. Compila e flasha il firmware aggiornato (`./tools/build.sh`, poi BOOTSEL +
   copia `build/picoplayer.uf2`).
2. **Accendi tenendo premuto MENU.** Sul display deve comparire `TEST AUDIO`.
3. Collega le cuffie al jack del modulo.
4. Prendi un filo rigido — va benissimo una **zampa di resistenza tagliata** —
   e **tienilo premuto a mano** fra le piazzole `XSMT` (3) e `A3V3` (5),
   scavalcando `FMT` (4) senza toccarlo.
5. Osserva:

| Cosa succede | Cosa significa |
|---|---|
| **Il tono parte quando premi e sparisce quando molli** | Diagnosi confermata. Vai al capitolo 6. |
| Il tono c'e' anche senza filo | XSMT non era il problema: il guasto e' altrove, vai al capitolo 9. |
| Non si sente niente in nessun caso | Prova prima con il **3V3 del Pico** al posto di A3V3 (potrebbe non essere alimentato come previsto). Se ancora niente, capitolo 9. |

> Con il filo tenuto a mano e' normale sentire fruscii e interruzioni: il
> contatto e' pessimo per definizione. Quello che conta e' che il tono **ci
> sia**, non che sia pulito.

---

## 6. Scegliere dove prendere i 3,3 V

Due possibilita', elettricamente equivalenti. **Mai 5 V**: solo 3,3 V.

### Opzione A — `XSMT` → `A3V3` (tutto sul modulo)

Filo di circa 1 cm, resta tutto sulla schedina, nessun cavo in piu' nel
cablaggio. **Due saldature, entrambe nella zona stretta.**

Prima di saldare, con il modulo alimentato, **misura `A3V3` verso GND**: deve
dare circa 3,3 V. E' il rail del regolatore a bordo del modulo. Se leggi un
valore strano, usa l'opzione B.

### Opzione B — `XSMT` → 3V3 del Pico (pin 36)

Un filo in piu' nel cablaggio, ma **una sola saldatura sul modulo**: l'altro
capo va dove gia' arrivano gli altri fili (breadboard, morsettiera, o il pin 36
del Pico).

**Se non sei sicuro con il saldatore, scegli la B**: dimezza il rischio nella
zona dove le piazzole sono vicine, e ti toglie del tutto il problema di
scavalcare FMT.

---

## 7. Saldatura, passo passo

1. **Scollega tutto** e lascia raffreddare eventuali parti calde.
2. Taglia il filo della lunghezza giusta (2 cm bastano per l'opzione A) e
   **spela 2 mm** da ogni capo. Non di piu': il rame nudo in eccesso e' proprio
   quello che poi tocca FMT.
3. **Stagna i capi del filo** ("pre-tin"): appoggia la punta sul rame nudo,
   tocca lo stagno, un secondo, via. Il rame deve diventare lucido e argentato.
4. **Stagna la piazzola `XSMT`**: appoggia la punta sul bordo del foro, tocca
   lo stagno con l'altra mano, conta **due secondi**, togli lo stagno, togli la
   punta. Deve restare una piccola cupola lucida, non una palla.
5. **Infila il capo del filo nel foro** e risalda: punta sul foro e sul filo
   insieme, un tocco di stagno, due secondi, via. Tieni il filo fermo finche' lo
   stagno non e' solidificato (2–3 secondi), altrimenti viene una saldatura
   "fredda", opaca e granulosa, che sembra fatta ma non conduce.
6. **Ripeti sull'altro capo** (`A3V3` per l'opzione A, oppure il 3V3 del Pico
   per l'opzione B).
7. **Curva il filo ad arco** in modo che passi sopra `FMT` senza appoggiarcisi.
8. **Tira delicatamente il filo** da entrambi i capi: non deve muoversi.

> Se sbagli e fai un ponte: non insistere col saldatore. Lascia raffreddare,
> togli lo stagno in eccesso con la treccia dissaldante e rifai. Riscaldare piu'
> volte di seguito la stessa piazzola e' il modo migliore per staccarla.

---

## 8. Verifica

### Con il multimetro, a modulo scollegato

Prova di continuita' (cicalino):

| Fra | Deve |
|---|---|
| `XSMT` e `A3V3` (o il 3V3 del Pico) | **suonare** |
| `XSMT` e `FMT` | **NON suonare** |
| `XSMT` e `AGND` | **NON suonare** |
| `A3V3` e `AGND` | **NON suonare** ← il piu' importante: e' il corto di alimentazione |

Se `A3V3` e `AGND` suonano, **non alimentare**: c'e' un ponte da rimuovere.

### Con il modulo alimentato

Misura `XSMT` verso GND: deve dare **~3,3 V**.

### Prova finale

1. Accendi tenendo premuto MENU → deve uscire il tono a 440 Hz, stabile e
   pulito, senza tenere niente a mano.
2. Riavvia normalmente e premi OK → deve partire la musica.

---

## 9. Se ancora non si sente niente

Nell'ordine:

1. **SCK del DAC (connettore in alto) deve andare a GND**, non a un GPIO. E'
   quello che seleziona il clock interno: lasciato flottante il DAC non
   aggancia.
2. **Controlla che BCK e LCK non siano invertiti.** Devono essere
   **BCK = GP14** e **LCK = GP15**, in quest'ordine, e **DIN = GP16**. Se sulla
   tua scheda sono al contrario, invece di rifare il cablaggio si puo'
   ricompilare con `-DPICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1`.
3. **VIN alimentato e GND in comune con il Pico.** Senza massa comune l'I2S non
   ha riferimento e non funziona niente.
4. **Prova altre cuffie** e verifica che il jack sia inserito a fondo.
5. **Leggi il log.** Collega il Pico al PC con il cavo USB e apri PuTTY sulla
   porta COM che compare (*Connection type: Serial*, `115200`). Il firmware
   stampa esito dell'init audio, mount della SD, ogni brano aperto con
   frequenza, canali e bitrate, e ogni errore.
   Perche' la porta COM compaia serve il submodule tinyusb del Pico SDK, una
   volta sola:
   ```bash
   sudo git -C "$PICO_SDK_PATH" submodule update --init lib/tinyusb
   ./tools/build.sh
   ```

---

## 10. Riepilogo delle cose da non fare

* Non saldare con il circuito alimentato.
* Non tenere il saldatore piu' di 2–3 secondi sulla stessa piazzola.
* Non collegare XSMT a 5 V. Solo 3,3 V.
* Non lasciare rame nudo in eccesso vicino a `FMT` (4).
* Non fare ponti di stagno fra `A3V3` (5) e `AGND` (6).
* Non mettere resistenze in serie: XSMT e' un ingresso CMOS ad alta impedenza,
  assorbe nanoampere.
* Non fidarti di un filo solo infilato nel foro: va saldato.

---

Riferimenti nel resto della documentazione: [README.md](README.md) sezione 1
(piedinatura e XSMT) e sezione 7 (diagnostica, modo test audio, recupero della
microSD).
