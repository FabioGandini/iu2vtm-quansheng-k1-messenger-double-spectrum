# IU2VTM firmware for the UV-K1 and UV-K5 V3

##########################################

COMPATIBLE WITH LABS APPS (OPEN WITH F+7)

MESSENGER (LONG PRESS MENU)

SPECTRUM F4HWN ( F+5)

ALTERNATIVE SPECTRUM (F+8)

###########################################


This is the **IU2VTM** fork of the [armel/muzkr UV-K1/K5V3 firmware](https://github.com/armel/uv-k1-k5v3-firmware-custom)
(F4HWN, Labs edition as a base), built around two things stock F4HWN doesn't
have: a **text messenger** between two radios -- type with the keypad,
callsign auto-ID, delivery confirmation, a one-button range-check ping, and
optional ChaCha20 encryption for off-band experiments -- and a **second
spectrum analyzer** (F+8) that scans your saved channels instead of a
frequency range, so you can see which ones are actually active at a glance.

Everything else comes straight from F4HWN Labs: Multiboot, overlay apps,
AirCopy, Broadcast FM, K5Viewer, RF Log and the rest -- see
[armel's own repository](https://github.com/armel/uv-k1-k5v3-firmware-custom)
for the full documentation of that base.

**Download:** grab the latest `iu2vtm.bin` from the
[Releases](https://github.com/FabioGandini/iu2vtm-uv-k1-k5v3-messenger-double-spectrum/releases)
page of this fork.

### IU2VTM-specific credits

Parts of the BK4829 Messenger RX/re-arm strategy and channel-busy handling were
informed by [GOGUFW](https://github.com/Gogu-Qs/GOGUFW-UV-K1-Messenger).
GOGUFW's earlier Messenger and multi-radio Range Check implementation also
served as a reference during development. The messenger, crypto and
channel-scan spectrum build on work by
[@joaquimorg](https://github.com/joaquimorg) and
[@kamilsss655](https://github.com/kamilsss655); see armel's own Credits
for everyone else.

## License

Copyright 2023 Dual Tachyon
https://github.com/DualTachyon

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
