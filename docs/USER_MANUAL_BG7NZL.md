# UV-K5 (k5-v6 / bg7nzl) User Manual

Operator-facing guide for features added on top of the egzumer-family baseline: how to enter them, where the menus are, defaults, and caveats.

Chinese edition: [`docs/USER_MANUAL_BG7NZL_ZH.md`](USER_MANUAL_BG7NZL_ZH.md)

---

## Baseline and version notes

| Item | Value |
|------|--------|
| Firmware tree for this manual | `k5-v6` current HEAD |
| Baseline (stock egzumer-family) | `a6eda00ab1f2915d0da49b5aa65ce3adaa0532df` (2024-03-26, `Fix typo in README.md`) |
| HEAD when this manual was written | `a70a9fbefa2440ee513ab1c319822d9eb30a688a` (`fix(roger): key Morse with TxMute; use 750 Hz Tone1`) |
| Target radios | Quansheng UV-K5 / K6 and similar DP32G030 + BK4819 sets (this tree’s build target) |

The baseline SHA was verified locally with `git cat-file -t`. “Relative to stock” below means relative to that commit.

**Legal reminder:** Obey local amateur-radio rules (frequency, power, identification, digipeater/APRS authorization, etc.). This manual describes firmware operation only; it is not a transmit authorization.

---

## 1. Overview of differences vs stock

On the egzumer custom firmware base, this build enables a set of digital-comms and practical extras by default, and disables some space-heavy features.

### 1.1 New / enhanced (user-visible)

| Feature | One-liner |
|---------|-----------|
| **Beacon** | Menu toggle: flashlight LED slow-blinks in an obstruction-light pattern (~one flash every 5 s) |
| **APRS RX/TX + Digipeater** | Side-key screen; RF uses the **currently selected VFO** frequency and power; temporarily FM / wide / CTCSS-DCS off / compander off, restored on exit; modem from ta1js; shows call / grid / comment; DgPeat: OFF / n-N / echo |
| **MyCall + Morse Roger** | Menu MyCall (≤6, EEPROM `0x0E30`, independent of DigiCall); with Roger=MORSE, FM PTT release sends ~**750 Hz** audio-simulated CW via **Tone1 + TxMute** (not true CW); empty callsign → silent |
| **CW modulation** | `Demodu` adds CW; CW TX is carrier + local 650 Hz sidetone |
| **AM / USB voice TX** | PTT transmit allowed in AM and USB (stock often blocks non-FM TX) |
| **Digimode (UART digital mode)** | PC precise frequency/control TX over the programming port (e.g. FT8); radio shows DIG UI |
| **CAT control** | PC read/write of the current VFO over the programming port; status bar shows `CAT` |
| **Slim broadcast FM** | Enter only with `F+0`; 64–108 MHz; UP/DOWN step; no memories / no scan |

### 1.2 Defaults off or behavior changes (vs baseline)

| Item | Baseline default | This firmware | User impact |
|------|------------------|---------------|-------------|
| **DTMF** | On (incl. calling-related) | **Off** (`ENABLE_DTMF=0`) | No DTMF menus / DTMF-entry behavior |
| **Spectrum** | On | **Off** (`ENABLE_SPECTRUM=0`) | No `F+5` spectrum; no SPECTRUM in side-key list |
| **Side-key → FM** | Side key can bind FM RADIO | **FM bind ineffective** | Broadcast FM only via `F+0` |
| **Lower side-key long** | Often none / other | With APRS enabled, default **APRS** | Long lower side key opens APRS screen |
| **Roger** | OFF / ROGER / MDC | Adds **MORSE** | Needs MyCall |

### 1.3 Compile flags (default build)

`Makefile` defaults — “factory” capability of the published build (custom builds may differ):

| Flag | Default | Meaning |
|------|---------|---------|
| `ENABLE_UART` | 1 | Programming UART / Digimode / CAT |
| `ENABLE_DIGMODE` | 1 | Digimode |
| `ENABLE_CATMODE` | 1 | CAT |
| `ENABLE_APRS` | 1 | APRS |
| `ENABLE_FLASHLIGHT` | 1 | Flashlight + Beacon menu |
| `ENABLE_FMRADIO` | 1 | Slim FM |
| `ENABLE_DTMF` | **0** | DTMF off |
| `ENABLE_DTMF_CALLING` | **0** | DTMF calling off |
| `ENABLE_SPECTRUM` | **0** | Spectrum off |

---

## 2. Flashing and multi-firmware notes (brief)

1. **Flash file**  
   Use the **packed** firmware from the release (e.g. `*.packed.bin` / `bg7nzl-k5v1-<shortSHA>.bin` in the release notes) with a normal UV-K5 flasher. Unpacked bare `.bin` files may be rejected by some tools.

2. **EEPROM / channels**  
   This firmware adds MyCall, APRS Digipeater, Beacon, etc. in existing config “holes.” Channels and most settings are usually kept; if menus show garbage values, use menu **Reset → VFO** or **ALL** (ALL clears more — use with care).

3. **Returning to stock / other firmware**  
   You can reflash egzumer or another image anytime. New menu items (MyCall / DgPeat, etc.) disappear; their EEPROM data is usually harmless. Different firmwares use holes differently — Reset if something looks wrong.

4. **Programming cable conflicts**  
   Digimode, CAT, and normal CPS flashing share the same UART. Do not run CPS while Digimode/CAT is active. For APRS timing tests (especially digipeat), **unplug the programming cable / close serial software** so UART activity does not disturb RX or your timing impression (see FAQ).

5. **Baud rate**  
   Digimode / CAT default to **38400** on the programming port (USB-CDC adapters at their device rate).

---

## 3. Side keys and shortcuts (this firmware)

### 3.1 Configurable side keys (menu)

Menu items: **F1Shrt / F1Long / F2Shrt / F2Long / M Long**  
(upper short/long, lower short/long, MENU long-press)

Side-key options in the default build include:

| Label | Action |
|-------|--------|
| NONE | None |
| FLASH LIGHT | Flashlight: off → solid → fast blink → SOS → off |
| POWER | Power cycle |
| MONITOR | Monitor (open squelch) |
| SCAN | Scan |
| VOX | Toggle VOX |
| LOCK KEYPAD | Keypad lock |
| SWITCH VFO | A/B |
| VFO/MR | Frequency / channel |
| SWITCH DEMODUL | Cycle demod (FM→AM→USB→CW→…) |
| APRS | Enter / exit APRS screen |

Notes:

- **FM RADIO no longer appears** in the side-key list; even if EEPROM still holds an old FM action, it becomes a no-op.
- **SPECTRUM is not built by default**, so it is absent from the list; `F+5` does not open spectrum.
- **With APRS enabled, F2Long (lower long) factory default = APRS** (falls back to this when EEPROM is unset or invalid).

### 3.2 Fixed shortcuts

| Action | Effect |
|--------|--------|
| `F` + `0` | Enter / exit broadcast FM |
| Main-screen side key APRS (or your bound key) | Enter / exit APRS (current VFO freq/power) |
| Digimode / APRS screen: `EXIT` | Leave that screen to the main UI |
| Main screen long side key for demod | If bound to SWITCH DEMODUL |

---

## 4. Beacon (obstruction-light slow blink)

### 4.1 What it is

With the **flashlight off**, the body LED blinks in an obstruction-light style rhythm: about **120 ms on**, about **5 s period**, for night location of the radio.

### 4.2 How to enable

1. Press `MENU`.  
2. Find **Beacon** (near Beep).  
3. Set **OFF** or **ON**, confirm to save.

### 4.3 Defaults and notes

| Item | Value |
|------|--------|
| Default | **OFF** (fresh EEPROM `0xFF` treated as off) |
| vs flashlight | While flashlight is solid / fast blink / SOS, Beacon **does not take the LED**; slow blink resumes after flashlight is off |
| Side-key FLASH LIGHT | Still cycles flashlight modes manually; Beacon is a separate menu item |

---

## 5. Roger and MyCall (Morse end-of-TX)

After releasing PTT at the end of an **FM** voice transmission, if Roger is **MORSE**, the radio sends **MyCall** once in Morse as an end-of-TX marker.

Implementation is **audio-simulated CW** (not true CW keying of the carrier): still in the FM end-of-TX path, BK4819 **Tone1** generates ~**750 Hz** single tone, keyed on/off with **TxMute** (same class of path as classic dual-tone Roger). On air it sounds like a short CW callsign, but modulation remains FM-path tone, not `Demodu=CW` true CW.

### 5.1 Roger menu

Menu: **Roger** (followed by **MyCall**)

| Option | Behavior |
|--------|----------|
| OFF | No end tone |
| ROGER | Classic Roger dual tone |
| MDC | MDC-style end tone |
| **MORSE** | After PTT release, send **MyCall** in Morse |

Trigger: Roger is called from the normal end-of-TX path (`RADIO_SendEndOfTransmission`).  
**CW / AM / USB** TX ends return early and **do not** play any Roger (including MORSE). Morse Roger therefore applies **only to FM voice** PTT release.

### 5.2 MyCall

Menu: **MyCall**

| Item | Detail |
|------|--------|
| Purpose | Only for Roger=**MORSE** end-of-TX Morse; nothing else |
| Length | Max **6** characters |
| Characters | `A`–`Z`, `0`–`9` (lowercase saved as uppercase) |
| Blanks | `_` / spaces stripped on save; unset shows `-` in the menu |
| Empty callsign | If MyCall is empty, Roger=MORSE still **stays silent** on PTT release |
| Storage | EEPROM **`0x0E30`–`0x0E37`** (8-byte region, first 6 = callsign; independent of DigiCall `0x0F20`) |
| vs DgCall | **Fully independent**: DgCall is only for APRS Digipeater; changing MyCall does not affect DgCall and vice versa (similar edit UI, different buffers) |

Built-in Morse parameters: about **100 WPM** (~12 ms element), Tone1 **750 Hz**, TxMute keying. Very fast — short ID only, not slow practice CW.

### 5.3 Recommended setup

1. Menu → **MyCall** → enter your callsign (e.g. `BG7NZL`, max 6 chars).  
2. Menu → **Roger** → **MORSE**.  
3. Switch to **FM**, talk on PTT, release, listen for your callsign (~750 Hz).  
4. To disable: set Roger back to OFF / ROGER / MDC, or clear MyCall.

---

## 6. Modulation: CW / AM / USB

### 6.1 How to switch

- Menu **Demodu**: FM / AM / USB / **CW** (and other compiled options).  
- Or bind a side key to **SWITCH DEMODUL** to cycle.

### 6.2 CW

| Item | Detail |
|------|--------|
| RX | USB-class demod with ~**650 Hz** pitch offset (sounds like sidetone CW) |
| TX | PTT keys carrier; speaker has **650 Hz** sidetone for timing |
| Use | Hand-key practice; Digimode also uses the CW TX path (PC precise control; sidetone may be taken over) |

### 6.3 AM / USB voice TX

Unlike stock “non-FM often TX-blocked,” this firmware allows PTT voice TX in **AM** and **USB**.  
Audio quality and duty cycle depend on hardware and use case; daily QSO should still use **FM**. Experimental DSB-SC TX is **not** included in the current firmware.

### 6.4 AM Fix

Menu **AM Fix** (if present): known AM RX improvement, same idea as egzumer; keep it on for AM listening when your menu offers ON/OFF.

---

## 7. APRS and Digipeater (DgPeat)

### 7.1 What it can do

- After entering the APRS screen via side key, RF uses the **currently selected VFO** (`TX_VFO`) frequency and power; **temporarily** switches to **FM + wide + CTCSS/DCS off + compander off** (does not keep prior demod/bandwidth/tones), arms the on-radio Bell202/FSK modem (implementation from **ta1js**) for APRS RX.  
- Digipeat / echo TX also uses that current VFO frequency and power.  
- Before use, tune the **VFO you want to listen on** to the local APRS channel and select it (common suggestion e.g. **144.640 MHz** — follow local rules).  
- On successful decode the screen shows:  
  - Line 1: source callsign (with SSID)  
  - Line 2: Maidenhead grid (from position packets; else `--------`)  
  - Lines 3–6: comment / message text  
- Digipeat (data digipeater) behavior is menu-configurable.

### 7.2 Enter / exit

**Enter:**

- Side-key action **APRS**, or default **lower side-key long (F2Long)**.  
- Same key again exits.  
- If Digimode is active, entering APRS exits Digimode first.  
- On entry, the current VFO’s frequency/demod/bandwidth/tones/compander etc. are snapshotted, then temporary modem RF is applied (frequency and power stay on the selected VFO).  
- **First APRS entry**: locks to selected `TX_VFO` and forces FM (plus wide / tones-off modem RF). Avoids Dual Watch / cross-band leaving RX on the other VFO with no RX on first entry.

**Exit:**

- `EXIT`, or the APRS side key again.  
- Stops the modem, **restores the pre-APRS VFO snapshot**, returns to the main UI.

### 7.3 UI and keys

| Key | Effect |
|-----|--------|
| `EXIT` | Leave APRS |
| `PTT` | **Ignored** (voice TX blocked; digipeat is automatic) |
| Other keys | Generally ignored (dual-beep) |

No manual TX button on this screen: focus is **listen + optional auto digipeat**, not a full APRS handheld (no menu to set position and beacon, etc.).

**On-screen behavior (vs main UI):**

| Item | Behavior |
|------|----------|
| Dual Watch | **Disabled** (no A/B hopping; stays on selected VFO) |
| PTT / VOX | **Voice TX blocked** (VOX path returns immediately on APRS screen) |
| Squelch end | **Does not tear down** the FSK modem (squelch-lost does not run a full `RADIO_SetupRegisters` that clears REG_3F / modem) |
| Sql | **Keeps global menu Sql**; APRS entry **does not force Sql=1**; raise/lower Sql in the main menu if packets are hard to hear |

### 7.4 Digipeater menus

| Menu | Role | Values / range | Fresh default |
|------|------|----------------|---------------|
| **DgPeat** | Digipeat mode | OFF / **n-N** / **echo** | **OFF** |
| **DgCall** | Digi callsign (DigiCall) | Max 6 chars A–Z/0–9 | empty |
| **DgSSID** | Digi SSID | 0–15 | 0 |
| **WIDE** | Which WIDE to answer | **1** / **2** / **1+2** | **1** (WIDE1-1 only) |

DigiCall / DgSSID / DgPeat / WIDE live in EEPROM **`0x0F20`** (independent of MyCall `0x0E30`).

#### DgPeat modes

| Mode | Meaning |
|------|---------|
| **OFF** | RX/display only, **no digipeat** |
| **n-N** | Proper digipeater: rewrite path (insert/mark your DgCall, handle WIDE1/WIDE2, etc.), ~**30 s** duplicate suppression |
| **echo** | On valid frame, **retransmit as-is** (debug/experiment; loop risk — use sparingly) |

#### n-N notes

1. **Set DgCall first**; empty callsign cannot enter valid n-N (saving with empty callsign forces OFF).  
2. **WIDE** selects WIDE1, WIDE2, or both.  
3. Does not digipeat packets already from you; does not re-insert if your call is already processed in the path.  
4. Retries when the channel is busy; short cool-down around digipeat reduces self-trigger.

### 7.5 Recommended steps (listen only)

1. Confirm **DgPeat = OFF**; set global **Sql** as needed (no need to force 1).  
2. Select the VFO to monitor; set local APRS frequency and desired power.  
3. Long-press lower side key (or your APRS binding) to enter.  
4. Wait for nearby APRS; watch call / grid / comment.  
5. `EXIT` (demod/bandwidth/tones restore to the pre-entry snapshot).

### 7.6 Recommended steps (proper digipeater)

1. Set **DgCall** to your callsign (authorized and legal for your locale).  
2. Set **DgSSID** as needed (e.g. 7, 10 — your preference).  
3. Set **WIDE** to `1`, `2`, or `1+2`.  
4. Set **DgPeat** to **n-N**.  
5. Stay on the APRS screen; matching packets digipeat automatically.  
6. When finished, set **DgPeat** back to **OFF** to avoid unattended digipeating.

### 7.7 Limits and cautions

- While in APRS, uses **selected VFO frequency and power**, with temporary FM/wide/tones-off/compander-off (snapshot restored on exit); this build does not retune inside the APRS UI.  
- Dual Watch can be on when entering: entry locks to the selected VFO; if first entry still has no RX, select the APRS VFO and re-enter.  
- Relies on the on-radio FSK modem; weak signal / interference may fail decode; Sql can help, but too open increases noise interruptions.  
- Common position packets, Mic-E, and some message text are shown; not a full APRS client.  
- **echo** is for short tests only.  
- Unplug the programming UART when measuring digipeat timing (see FAQ).

---

## 8. Slim broadcast FM

### 8.1 Enter / exit

- **Enter / exit:** `F` + `0`  
- **Also exit:** `EXIT` on the FM screen

### 8.2 Controls

| Key | Effect |
|-----|--------|
| `UP` / `DOWN` short or long | Frequency ±0.1 MHz, wraps at ends |
| `EXIT` | Leave FM |
| Digits / `PTT` / side keys / `MENU` etc. | **Ignored** (slim design) |

### 8.3 Range and defaults

| Item | Value |
|------|--------|
| Range | **64.0 – 108.0 MHz** |
| Step | 0.1 MHz |
| Memories / auto-scan / store | **Removed** |
| Side-key → FM | **Unavailable** |

Frequency is written to FM EEPROM; next `F+0` resumes near the last frequency (still clamped to 64–108).

---

## 9. Digimode (PC-controlled digital TX)

### 9.1 What it is

The radio takes PC commands over the programming UART for **sub-Hz** stepped constant-envelope FSK TX (typical use: VHF/UHF **FT8**, etc.). Symbols come from the PC; the radio handles timing and RF.

### 9.2 How to enter

- **No** standalone menu item “enter Digimode.”  
- When the PC tool sends Digimode protocol frames (sync `0xAB`, commands `0x01`–`0x0A`), the radio enters automatically and shows the DIG UI.  
- Shares the UART with the stock flash protocol (`0xAB 0xCD`); distinguished by the second byte.

### 9.3 Screen fields

| Display | Meaning |
|---------|---------|
| **DIG RX** | In mode, not transmitting |
| **DIG TX** | Transmitting |
| **DIG WAIT** | Scheduled, waiting for start (countdown) |
| Large frequency | Base frequency |
| AF: … Hz | Current audio offset |
| RF: … | Actual RF |
| FIFO / CRC | Queue depth and recent CRC fail count (link debug) |

### 9.4 Exit

- Press **`EXIT`** for a clean Digimode exit and normal RX restore.  
- Link heartbeat: ~**1 s** without valid traffic stops TX (prevents a dead PC holding the channel).  
- Entering APRS also exits Digimode first.

### 9.5 PC tools (in-tree)

| Path | Purpose |
|------|---------|
| `tools/digimode/` or `tools/gui/` | Windows GUI (v1 audio bridge, v2 manual FT8, v3 WSJT-X UDP) |
| `tools/digimode/ft8_send_*.py` etc. | CLI / batch send |
| `tools/DIGITAL_MODE.md` | Protocol and advanced notes (technical) |

Typical flow (conceptual):

1. Power on the radio; connect programming cable or USB serial adapter.  
2. Open the GUI/script; select port, **38400**.  
3. Sync time in the tool → schedule or stream symbols.  
4. Radio shows DIG; when done, `EXIT` or stop TX in the tool.

### 9.6 Notes

- Digimode and **CAT at once** conflict: CAT entry fails if Digimode is already active.  
- Confirm legal frequency and power before TX.  
- Do not use CPS read/write on a busy link.

---

## 10. CAT control

### 10.1 What it is

The PC reads/writes the **current VFO** (frequency, tones, power, squelch, VOX, etc.) over the same programming UART for remote control.  
**No dedicated full-screen CAT UI**; you stay on the main screen with a status-bar **`CAT`** label.

### 10.2 Enter / exit

- PC tool sends CAT commands (sync `0xAB`, commands ~`0x10`–`0x1B`).  
- On enter: if in channel mode, the radio **forces VFO (frequency) mode**.  
- On exit: VFO changes **remain** (no full restore from backup).  
- Heartbeat timeout ~**5 s** without traffic is handled on the firmware side per protocol (keep heartbeat with the intended tools).

### 10.3 PC tools

| Path | Purpose |
|------|---------|
| `tools/cat_control/cat_cli.py` | CLI: `python cat_cli.py -p /dev/ttyUSB0` (default 38400) |
| `tools/cat_control/webui/` | Browser UI (channel presets, status, RSSI, etc.) |
| `tools/cat_control/run_webui.sh` | Helper to start the Web UI |

Common CLI ideas: `freq`, `txfreq`, `offset`, `power`, `squelch`, `vox`, `status`, `quit` (see `help` in the script).

### 10.4 Controllable parameters (summary)

RX/TX frequency, offset direction/size, CTCSS/DCS, demod (FM/AM/USB), power, bandwidth, squelch, VOX, mic gain, speaker-related gain, compander, scrambler, busy lock, step; read-only includes RSSI, mic level, status (RX/TX, battery mV, etc.).

Power can be passed as multi-level codes in the protocol; firmware maps to onboard LOW / MID / HIGH.

### 10.5 Notes

- Cannot enter CAT while Digimode is active.  
- CAT changes the current VFO and keeps them after exit — check frequency before PTT.

---

## 11. DTMF and Spectrum (off by default)

### 11.1 DTMF

Default firmware **has no DTMF** (no UPCode / PTT ID / D Live menus, no DTMF dial state on the main screen).  
If you need DTMF, rebuild with `ENABLE_DTMF=1` (and optionally `ENABLE_DTMF_CALLING=1`). This manual describes the default release.

### 11.2 Spectrum

Default **has no spectrum analyzer**:

- `F+5` does not open spectrum (and without NOAA that key differs from the old spectrum shortcut).  
- No SPECTRUM in the side-key list.  

Rebuild with `ENABLE_SPECTRUM=1` if needed.

---

## 12. Other practical differences vs stock

| Item | Notes |
|------|--------|
| No FM in side-key list | See §8 |
| AM/USB/CW TX allowed | See §6 |
| Pack script | Repo provides `build-packed.sh` for flashable packed images (for builders, not an on-radio menu) |
| Status-bar CAT | See §10 |

Unmentioned daily ops (channels, scan, Dual Watch, CTCSS, etc.) largely match egzumer; see the upstream Wiki. This manual covers increments and default differences vs the baseline.

---

## Appendix A. Firmware-related menu items

(Default build: DTMF off, Spectrum off, APRS / Flashlight on)

| Menu label | Notes |
|------------|--------|
| Beacon | Obstruction-light slow blink OFF/ON |
| DgPeat | APRS digipeat OFF / n-N / echo |
| DgCall | Digipeater callsign |
| DgSSID | Digipeater SSID 0–15 |
| WIDE | 1 / 2 / 1+2 |
| Roger | OFF / ROGER / MDC / **MORSE** (MORSE uses MyCall; 750 Hz Tone1+TxMute) |
| MyCall | Morse Roger callsign (≤6; independent of DgCall; EEPROM `0x0E30`) |
| Sql | Global squelch; APRS screen keeps it (not forced) |
| Demodu | Includes **CW** |
| F1Shrt / F1Long / F2Shrt / F2Long / M Long | Side keys + MENU long; **APRS** available |
| AM Fix | AM RX enhancement |

Hidden menus (traditional power-on PTT + upper side key, etc.) still include F Lock, TX band unlock, Reset, etc., similar to stock — use carefully.

---

## Appendix B. User-configurable EEPROM items (callsigns, etc.)

Addresses for backup/reference; day-to-day use the menus — no need to hand-edit EEPROM.

| Content | EEPROM region | Notes |
|---------|---------------|--------|
| **MyCall** | `0x0E30`–`0x0E37` | Max 6 chars A–Z/0–9; Morse Roger only; unrelated to DgCall |
| **APRS Digi** | `0x0F20`–`0x0F2F` | flags, SSID, DgCall; **do not casually alter other bits around `0x0F40`** |
| **Beacon** | `0x0F40` byte bit0 (inverted storage) | Fresh `0xFF` → Beacon OFF |
| Side-key actions | `0x0E90` region | Includes F2 long; invalid values fall back to APRS in APRS builds |
| Roger mode | Original Roger byte (shared misc area) | Values 0–3: OFF / ROGER / MDC / **MORSE** |

**Reset:**

- **VFO**: keeps MyCall (`0x0E30`) and Digi (`0x0F20`) “hole” data.  
- **ALL**: broad erase; MyCall / Digi return to empty / OFF.

---

## Appendix C. FAQ

**Q1: Long lower side key does not open APRS?**  
Check menu **F2Long** is **APRS**. If changed, set it back.

**Q2: No audio after entering APRS?**  
Normal. APRS listen closes the speaker audio path and uses the modem; call/grid on screen means it is working. PTT / VOX are inert on the APRS screen — you cannot “open squelch with PTT.”

**Q3: n-N does not digipeat?**  
Confirm DgPeat=n-N, DgCall non-empty, WIDE matches the path, not a duplicate within 30 s, and the channel is free. Use OFF for listen-only.

**Q4: No Morse Roger sound?**  
Confirm Roger=**MORSE**, **MyCall non-empty** (empty is intentionally silent), and mode is **FM** (CW/AM/USB skip Roger). Morse is fast (~100 WPM, Tone1 **750 Hz**) — listen right at PTT release. MyCall is unrelated to DgCall.

**Q5: Beacon does not blink?**  
Beacon=ON and flashlight off (not solid/SOS). Rhythm ~120 ms on, ~5 s period.

**Q6: FM side key dead / cannot store stations?**  
Slim FM: `F+0` only; no memories or scan.

**Q7: No Spectrum / DTMF?**  
Default build disables them. See §11.

**Q8: Digimode / CAT will not connect?**  
Check port, **38400**, CPS not holding the port; do not stack both; `EXIT` Digimode before starting CAT.

**Q9: Odd APRS digipeat timing / unstable delay?**  
**Unplug the programming cable / AIOC serial**, close PC serial apps, then retest. UART / combo USB-audio lines can disturb RX and skew “hear → digipeat” timing.

**Q10: Menus garbled or odd after flash?**  
Do a menu **Reset** (try VFO first, then ALL), then re-set MyCall / DgCall / side keys.

**Q11: Can this act as a full APRS handheld beacon?**  
No. Current VFO listen + optional digipeater + simple display only; no menu to edit lat/lon/path and send your own position beacons.

**Q12: Dual Watch / cross-band: enter APRS with no RX?**  
APRS locks to the **currently selected (TX) VFO** and forces FM. Select the APRS VFO first; Dual Watch does not hop inside the screen. If still wrong, `EXIT` and re-enter.

**Q13: Must APRS Sql be set to 1?**  
No. Global menu **Sql** is kept; firmware **does not** force Sql on APRS entry.

---

## Appendix D. Quick reference card

```
F+0          Broadcast FM (64–108, UP/DOWN step, EXIT to leave)
F2 long*     APRS (selected VFO freq/power; temp FM/wide/tones off; no DW/PTT/VOX; *default)
EXIT         Leave APRS / Digimode / FM (APRS exit restores snapshot)
Menu Beacon  Obstruction-light blink (~every 5 s)
Menu DgPeat  OFF | n-N | echo; WIDE 1/2/1+2; DgCall@0x0F20
MyCall(≤6@0x0E30) + Roger=MORSE   FM PTT release: 750Hz Tone1+TxMute (empty=silent)
Demodu=CW    True CW RX/TX (650 Hz sidetone; not Morse Roger)
PC serial    Digimode (FT8 etc.) / CAT (status-bar CAT)
```

---

*Document file: `docs/USER_MANUAL_BG7NZL.md` (English); Chinese: `docs/USER_MANUAL_BG7NZL_ZH.md`. Features follow the current tree source and default `Makefile`; custom compile flags may change menus and side keys.*
