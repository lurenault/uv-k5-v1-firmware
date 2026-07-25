# UV-K5 v1 firmware — release notes (Cursor-maintained)

## Download

- **`bg7nzl-k5v1-<short-sha>.bin`** (attached to this release) — **packed** firmware from `fw-pack.py` / `firmware.packed.bin`, ready for normal flashing tools.
- GitHub may also list default **Source code** archives; those are normal and separate from the firmware asset.

## What's new since the previous auto-release

### APRS (side-key screen)

- **Enter via side key** (default F2 long = APRS). RF uses the **currently selected VFO** frequency and power; temporarily forces **FM / wide / CTCSS-DCS off / compander off**, restores the VFO snapshot on exit.
- **First entry** locks `TX_VFO` and forces FM so Dual Watch / cross-band does not leave RX on the wrong VFO.
- On the APRS screen: **Dual Watch off**, **PTT / VOX blocked** for voice TX; **squelch end does not tear down** the Bell202/FSK modem. Global menu **Sql** is kept (not forced to 1).
- **Digipeater (DgPeat)**: OFF / n-N / echo; **WIDE** 1 / 2 / 1+2; DigiCall EEPROM **`0x0F20`**. Modem path from **ta1js**; decode UI shows call / Maidenhead / comment.
- User-facing manual: English [`docs/USER_MANUAL_BG7NZL.md`](docs/USER_MANUAL_BG7NZL.md), Chinese [`docs/USER_MANUAL_BG7NZL_ZH.md`](docs/USER_MANUAL_BG7NZL_ZH.md).

### Morse Roger (MyCall)

- Roger menu adds **MORSE**: after FM PTT release, keys **MyCall** as audio-simulated CW (**Tone1 + TxMute**, **750 Hz** — not true CW / not `Demodu=CW`).
- MyCall ≤6 chars, EEPROM **`0x0E30`**, independent of DigiCall; empty MyCall → silent.

### Beacon / flashlight

- Menu **Beacon**: obstruction-light style slow blink on the flashlight LED (~120 ms on, ~5 s period) when the light is otherwise off.

### Build & packaging / flash budget

- Linker flash region is **60K** (`firmware.ld`); keep features within that budget (current default build text is near the limit).
- **`build-packed.sh`** — one-step ELF build plus versioned packed `.bin` output.
- **`run_webui.sh`** — local venv setup and CAT Web UI launcher.
- Size helpers: `make` emits `firmware.map`; optional `make size-report`.

### CAT / RF (carried forward)

- **CAT** on programming UART (38400), PC tools under `tools/cat_control/`; status-bar **CAT** label (no dedicated CAT LCD page).
- **AM / USB TX** supported; experimental **DSB-SC not included**.

---

*This file is the **source of truth** for auto-release descriptions. Before each `commit` + `push` to `main`, update it in English (narrative summary, not a raw commit list). GitHub Actions reads it verbatim when creating the `auto-*` release.*
