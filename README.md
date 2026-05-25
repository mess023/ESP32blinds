# ESP32 Blinds Firmware

Firmware for the motorised window blinds. One ESP32 board (**WT32-ETH01**, with
wired Ethernet) controls **one window = two stepper-driven blinds** (a *bottom*
screen and a *top* screen). It accepts position commands over **Art-Net** and
**OSC**, exposes a **web UI** for setup/calibration, and supports **OTA**
firmware updates.

Four boards (window1–window4) make up the full installation. They all share one
Art-Net universe; each reads its own block of DMX channels.

For the complete end-to-end setup (this firmware **+** the desktop
[Blinds Controller](../blinds-controller) **+** wiring, network and lighting-
console integration) see **[../blinds-controller/SYSTEM_SETUP.md](../blinds-controller/SYSTEM_SETUP.md)**.

---

## Hardware

Per window:

| Item | Notes |
|------|-------|
| **WT32-ETH01** ESP32 board | ESP32 + LAN8720 Ethernet PHY (RJ45). Networking is **wired Ethernet**, not Wi-Fi. |
| **2 × stepper motors** | one for the bottom screen, one for the top screen |
| **2 × stepper drivers** | step/dir type (e.g. DM542, TB6600, TMC). |
| **4 × limit switches** | start/end endstops, one pair per screen |
| **Motor power supply** | sized for your drivers/motors (e.g. 24–48 V) |
| **5 V supply** for the board | the WT32-ETH01 is powered at its 5V pin |
| **USB-to-TTL serial adapter (3.3 V)** | needed **once** for the first flash (CP2102 / FT232 / CH340) |

Shared:
- An **Ethernet switch** and cables to link all four boards (and the controlling
  computer or lighting console) on one LAN.

### Default GPIO pin map (from `include/stepper_interface.h` and `src/main.cpp`)

| Function | GPIO |
|----------|------|
| Stepper 0 (bottom) STEP | 32 |
| Stepper 0 (bottom) DIR  | 33 |
| Stepper 1 (top) STEP    | 15 |
| Stepper 1 (top) DIR     | 14 |
| Limit: bottom start     | 2  |
| Limit: bottom end       | 4  |
| Limit: top start        | 17 |
| Limit: top end          | 12 |

---

## Toolchain — PlatformIO

This is a PlatformIO project (`platformio.ini`).

1. Install **VS Code**: <https://code.visualstudio.com/>
2. Install the **PlatformIO IDE** extension (Extensions → search “PlatformIO”).
   It downloads the ESP32 toolchain and all `lib_deps` automatically on first
   build.
   - CLI alternative: `pip install platformio` then use `pio` commands.
3. Open the `ESP32blinds` folder in VS Code.

Build artifacts and downloaded libraries go in `.pio/` (git-ignored).

---

## Per-device configuration — `data/config.json`

Each board stores a JSON config on its SPIFFS filesystem:

```json
{
  "mdnsName": "window1",
  "ipadress": "10.0.0.101",
  "gateway":  "10.0.0.1",
  "subnet":   "255.255.255.0",
  "dmxAddress": 1,
  "reverseStepper0": false,
  "reverseStepper1": true
}
```

| Key | Meaning |
|-----|---------|
| `mdnsName` | mDNS hostname (`window1.local`) **and** the source of the auto DMX address |
| `ipadress` | static IP (note: spelled `ipadress`) |
| `gateway` / `subnet` | network settings |
| `dmxAddress` | **optional** DMX start channel. If omitted, it is derived from the number in `mdnsName`: `window1`→1, `window2`→5, `window3`→9, `window4`→13 |
| `reverseStepper0/1` | flip motor direction per screen |

**DMX channel layout (universe 1):**

| Window | mdnsName | DMX start | Bottom blind | Top blind |
|--------|----------|-----------|--------------|-----------|
| 1 | window1 | 1  | ch 1–2  | ch 3–4  |
| 2 | window2 | 5  | ch 5–6  | ch 7–8  |
| 3 | window3 | 9  | ch 9–10 | ch 11–12 |
| 4 | window4 | 13 | ch 13–14 | ch 15–16 |

Because the DMX address is derived from `mdnsName`, **you do not need to edit
config per device for channel assignment** — just give each board the right
`mdnsName` (and IP) once.

---

## First flash (over USB) + filesystem

The WT32-ETH01 has no native USB. Use a 3.3V/5V USB-to-TTL adapter:

| Adapter | WT32-ETH01 |
|---------|------------|
| 3.3V or 5V | 3.3V or 5V pin |
| GND | GND |
| TX  | RX0 |
| RX  | TX0 |

To enter the bootloader: connect **GPIO0 to GND**, then power on. After flashing has succeeded you diconnect GPIO0 from GND and reboot the device.

1. Put the correct `config.json` for this board in `data/` (set its `mdnsName`
   and `ipadress`).
2. Edit the `[env:wt32-eth01-COM3]` section in `platformio.ini` so `upload_port`
   matches your serial port (`COMx` on Windows, `/dev/ttyUSB0` on Linux,
   `/dev/cu.usbserial-*` on macOS).
3. **Upload the filesystem** (writes `config.json` to the board):
   ```bash
   pio run -e wt32-eth01-COM3 -t uploadfs
   ```
4. **Upload the firmware**:
   ```bash
   pio run -e wt32-eth01-COM3 -t upload
   ```
5. Open the serial monitor to confirm it boots, gets its IP, and prints its DMX
   address:
   ```bash
   pio device monitor -b 115200
   ```

Repeat for each board with its own `config.json` (`mdnsName`/IP per window).

---

## Updating later — OTA (no USB, no cables)

Once a board is on the network it can be reflashed via ethernet. The four
`frameN` environments in `platformio.ini` target the window IPs
(`10.0.0.101–104`), and `default_envs` lists all four.

Flash **all four** windows in one go:
```bash
pio run -t upload
```
Flash a single window:
```bash
pio run -e wt32-eth01-frame1 -t upload
```

> ⚠️ **Use `-t upload` (firmware only), NOT `-t uploadfs`, for OTA.**
> The filesystem holds each board’s unique `config.json`. A firmware-only OTA
> preserves it; `uploadfs` would overwrite every board with the sample config
> and break the per-window IP/DMX assignment.

If the IPs differ in your install, edit each `frameN` env’s `upload_port`.

---

## Web UI, calibration & homing (required before use)

Each board serves a web UI at its IP (e.g. `http://10.0.0.101/`) or
`http://window1.local/`.

- **Control tab** – position sliders, **Calibrate stepper0/1**, **Home
  stepper0/1**, **forward**, **stop**, live position.
- **Settings tab** – motor **Speed**, **Acceleration**, **Safety Margin**.

> **Important:** the firmware ignores Art-Net/OSC for a screen until that screen
> has been **calibrated** (it learns its travel range from the limit switches).
> After power-up also **home** each screen. Do this once per screen, per board,
> from the web UI before driving them from the controller or a console.

Other tools:
- **OTA**: handled automatically once on the network (ArduinoOTA).
- **WebSerial**: live debug log served by the device’s web server.

---

## Control protocols

Both run simultaneously — use whichever you like.

**Art-Net** (UDP 6454, universe 1): 16-bit positions in the channel layout
above. Driven by the desktop controller or any lighting console (broadcast or
unicast to the board IPs).

**OSC** (UDP 7000), addressed per board by sending to that board’s IP:
| Address | Argument | Meaning |
|---------|----------|---------|
| `/btm/pos` | float `0.0–1.0` | bottom screen position (normalised) |
| `/top/pos` | float `0.0–1.0` | top screen position (normalised) |

---

## Repo contents

```
platformio.ini      build envs (USB + per-window OTA)
src/                 firmware sources (main, steppers, web UI, OTA, OSC, config)
include/             headers
data/                config.json (uploaded to device SPIFFS)
README.md            this file
```
