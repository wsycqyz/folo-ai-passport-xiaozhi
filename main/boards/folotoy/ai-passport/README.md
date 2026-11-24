# FoloToy AI Passport (ai-passport)

Board definition that lets the XiaoZhi voice assistant run on the
[FoloToy AI Passport](https://github.com/FoloToy/ai-passport) wearable.

## Hardware

- MCU: ESP32-C3, 8 MB flash, **no PSRAM**, USB Serial/JTAG console
- Audio: ES8311 codec (I2C 0x18) on the shared I2C bus, I2S full-duplex
  (amplifier enable not wired, treated as always on)
- Display: ST7789 (ST7789P3) 240x320 portrait, 4-line SPI
- Battery: CW2017 fuel gauge (I2C 0x63, optional)
- Buttons: UP / DOWN / OK share GPIO0 (ADC1_CH0) through a resistor ladder

Pin mapping follows `ai-passport/components/bsp/include/bsp_pins.h`:

| Function | Pin |
| --- | --- |
| LCD MOSI / SCLK / CS / DC | 9 / 8 / 1 / 20 |
| LCD backlight (PWM) | 21 |
| I2S MCLK / BCLK / WS / DOUT / DIN | 6 / 5 / 3 / 2 / 4 |
| I2C SDA / SCL | 10 / 7 |
| Buttons (ADC ladder) | GPIO0 |

## Build

```sh
python scripts/build.py folotoy/ai-passport --name ai-passport
```

Outputs `build/merged-binary.bin` (8 MB flash, USB Serial/JTAG console).

## Controls

The three physical keys map to XiaoZhi's voice-assistant actions:

- **OK** — single click: toggle the chat state (or enter Wi-Fi config mode
  while starting up)
- **UP** — single click: volume +10; long press: max volume
- **DOWN** — single click: volume -10; long press: mute

Because the ladder shares one ADC pin, XiaoZhi reads it as three
independent ADC buttons (the same pattern as the ESP-BOX-Lite).

## Servers that start on demand

A server that scales to zero, such as an Azure Container App without a minimum
replica, accepts a connection at once but answers only after it has started.
On 2026-11-24 such a cold start took 48 to 54 seconds, while the firmware gave
up after 10. Both variants now wait for the server, up to
`CONFIG_SERVER_START_TIMEOUT_SEC` (default 90 seconds; 0 restores the old
behaviour):

- **Starting a chat**: each time the 10-second WebSocket handshake wait runs
  out, the device connects again, and from the first retry it shows
  `服务器正在开机，请稍候...`. Other connection errors are reported at
  once. A server that never answers is reported after about 100 seconds.
- **At boot**: the version check waits up to 90 seconds for an answer, instead
  of showing an error and retrying after 30.
- **OK is ignored while connecting.** The main task is busy until the
  connection is made, and a press queued behind it would close the chat as soon
  as it opened.

## Notes / calibration

- Display orientation, color inversion and the backlight PWM polarity were
  taken from the Passport BSP; verify on real hardware and adjust the
  `DISPLAY_*` macros in `config.h` if the image is rotated/inverted or the
  backlight is reversed.
- The ADC button voltage windows assume the Passport's external 10 kOhm
  pull-up. Re-measure with the Button page if thresholds drift.
- CW2017 presence is optional; without the chip the status bar shows no
  battery level and the board keeps working.

## Kids variant (`ai-passport-kids`)

A second firmware variant for a young child. It has its own name and therefore
its own OTA identity: an OTA image for the stock `ai-passport` variant never
replaces it.

```sh
python scripts/build.py folotoy/ai-passport -c config.kids.json --name ai-passport-kids
```

What it changes (`CONFIG_AI_PASSPORT_KIDS_UI`, implemented in `kids_display.cc`):

- **Dark "starry night" screens in every state**, from the first frame and
  whatever theme is stored. The artwork is original and drawn by code
  (`scripts/kids_ui/gen_art.py`).
- **The mascot's face shows the state**: ear waves while listening, thought
  dots while the answer is on its way, a talking mouth while speaking, a sad
  face on errors.
- **A status pill** at the top: the clock when idle, `我在听` (listening),
  `请稍等` (wait), `我在说` (speaking), `出错了` (error).
- **A speech bubble shows only the child's own words**, wrapped over three or
  four lines; longer text scrolls by itself. The assistant's text is never
  shown. System messages (Wi-Fi setup instructions, the wait for a server that
  is starting, errors) use the same bubble.
- **The volume keys show a bar** instead of a number.
- **Voice tools** (MCP): the assistant gets no tool to change the screen theme
  and the device status reports no theme, because the screen is always dark.
  The brightness cannot be set below 10: the device has no brightness key to
  bring a black screen back.
- **Wi-Fi transmit power**: at boot the variant removes the stored value
  `wifi/max_tx_power = 8` (2 dBm). Version 3.3.3 of the `esp-wifi-connect`
  setup page stores that value by accident when its Advanced tab is saved on a
  device that has no Wi-Fi settings yet.

Fixed texts live in `kids_strings.h` and may use only characters of the
built-in `basic` font, because the larger `common` font in the assets partition
is loaded only after the network is up. `scripts/tests/test_kids_ui.py` checks
this, and that `kids_art.c` matches the generator.

### Flash without losing the device's settings

`build/merged-binary.bin` covers the NVS partition, so writing it at 0x0 erases
the Wi-Fi and OTA settings. To keep them, write the parts:

```sh
python -m esptool --chip esp32c3 -p PORT -b 460800 write-flash \
  --flash-mode dio --flash-freq 80m --flash-size 8MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x20000 build/xiaozhi.bin \
  0x600000 build/generated_assets.bin
```

### Maintenance console

The kids variant reads these commands from the USB Serial/JTAG console once per
second. `scripts/kids_ui/kui.py` sends them and saves captured screens as PNG
files; it needs only pyserial, so the ESP-IDF Python can run it.

| Command | `kui.py` | Effect |
| --- | --- | --- |
| `KUI VOL n` | `vol n` | Set and store the output volume (0 to 100). |
| `KUI SHOT` | `shot --out DIR` | Stream the current screen. |
| `KUI DEMO` | `demo --out DIR` | Show a scripted gallery of every screen, stream each one, then restore the real state. Needs the idle state. |
| `KUI CHAT` | `chat` | Toggle the chat, like the OK key. |

Screens are captured from the LVGL flush path, after the rounded-corner mask,
so a capture is what the panel shows.

### Change the artwork

Edit `scripts/kids_ui/gen_art.py` and run it with a Python that has Pillow and
numpy. It rewrites `kids_art.c` and `kids_art.h`; `--preview DIR` also writes
PNG previews.
