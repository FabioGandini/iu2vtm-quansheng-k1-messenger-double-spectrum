# iu2vtm (messenger + double-spectrum) → allineamento a F4HWN v7.0.0

Base: `armel/uv-k1-k5v3-firmware-custom` **v7.0.0** (18 nov 2026).
Fork rifuso sopra: `FabioGandini/iu2vtm-...-messenger-double-spectrum` (era su v6.3.0).
Metodo: merge a 3 vie (base = armel v6.3.0, ours = armel v7.0.0, theirs = fork).

## Esito
- Compila pulito su v7.0.0 con toolchain arm-none-eabi-gcc 13.2.1.
- Preset `Custom` (= edizione IU2VTM): RAM 83.9%, FLASH 89.6% (108 KB/118 KB),
  asset pack 52.5%. Nessun overflow.
- `_Static_assert` di coerenza tabelle menu: superato.

## File del fork reintrodotti (drop-in, invariati)
- `App/app/messenger.c/.h`, `App/ui/messenger.c/.h`
- `App/app/spectrum_k5.c/.h`
- `App/helper/crypto.c/.h`, `App/external/chacha/chacha.c/.h`

## Merge automatico pulito (nessun conflitto testuale)
17 dei 18 file sorgente modificati dal fork si sono fusi senza conflitti:
action.c, app.c/.h, main.c, menu.c (app), bk4819-regs.h, bk4819.h, bk4829.c,
settings.c/.h, ui/helper.c/.h, ui/main.c, ui/menu.h, ui/ui.c/.h, App/CMakeLists.txt.

## Risoluzioni manuali (3 punti)
1. **`App/ui/menu.c`** — unico conflitto testuale. v7 ha sostituito la tabella
   menu inline con `#include "menu_list.def"` (X-macro). Tenuto il lato v7 e
   spostate le voci del fork nel `.def` (punto 2).
2. **`App/ui/menu_list.def`** — inserite le tre voci del fork subito prima delle
   "hidden menu items":
   ```
   #ifdef ENABLE_MESSENGER
       MENU_ITEM("MsgRX",  MENU_MSG_RX)
       MENU_ITEM("MsgACK", MENU_MSG_ACK)
   #ifdef ENABLE_ENCRYPTION
       MENU_ITEM("MsgEnc", MENU_MSG_ENC)
   #endif
   #endif
   ```
   (Gli enum `MENU_MSG_*` erano già entrati puliti in `menu.h`.)
3. **`CMakePresets.json`** — il preset `Custom` del fork ereditava da `Labs`,
   che in v7 non esiste più (l'overlay è confluito in Fusion). Cambiato
   `"inherits": "Labs"` → `"inherits": "Fusion"`. Tutte le feature del fork
   (MESSENGER, ENCRYPTION, SPECTRUM_K5, notification/FSK-mute) restano attive.

## Da verificare su hardware (il build non lo garantisce)
- Messaggistica e doppio spettro a runtime sulla pipeline display/menu riveduta di v7.
- Migrazione settings EEPROM: il fork aggiunge campi; dopo il flash fai un
  **reset di fabbrica** per sicurezza.
- Firma/packaging: qui ho usato il build cmake nativo; per la release ufficiale
  usa `./compile-firmware.sh Custom` (Docker) se vuoi l'immagine firmata uguale a monte.

## Overlay apps
API overlay v7 = livello 4, ma **ABI major 1 invariato e append-only**: le app di
livello 3 restano compatibili. RF Learn/RF Send ricompilate su v7 risultano
**byte-identiche** a quelle v6.3.0 → girano su v7 senza modifiche.
Opportunità futura: v7 aggiunge un servizio livello-4 di "potenza TX per-app"
che non tocca la potenza salvata del VFO — sostituirebbe l'hack manuale su REG_0x36
in RF Send.
