<p align="right">
  <a href="muyu.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Muyu: a pocket wooden fish

This guide and the Muyu source are retained from the previous application. The
current build target is the [Guandan scoreboard](guandan.md). Its build output
must not be used as Muyu firmware; the previously verified Muyu delivery remains
in the local `build/delivery/` directory. The build instructions below apply to
the Muyu source configuration, not the current Guandan `main/CMakeLists.txt`.

Muyu 1.0.0 turns AI Passport into an offline wooden-fish percussion counter.
Press OK to strike, hear a short synthesized wood tone, and see the fish move
with a floating count animation. The application uses its own warm paper-colored
screen, rather than the baseline hardware-test menu.

The target is the FoloToy AI Passport with an ESP32-C3, 8 MB Flash, no PSRAM,
a 240 × 320 portrait display, and three physical function buttons. No Wi-Fi,
Bluetooth connection, account, cloud service, or additional wiring is required.

## Start and controls

On each boot, the session count starts at zero and automatic striking is off.
The application restores the last saved cumulative count, volume, and tempo.
With no previous record, volume is 50% and tempo is 60 strikes per minute.

| Button action | Behavior |
| --- | --- |
| Press OK | Strike immediately; increment the session and cumulative counts. Holding OK does not repeat. |
| Short press UP | Start or pause automatic striking. The first automatic strike occurs after one beat. |
| Hold UP for at least 500 ms, then release | Cycle the automatic tempo through 60, 90, and 120 BPM. |
| Short press DOWN | Cycle volume through 0%, 25%, 50%, and 75%; from the default 50%, the next choices are 75%, mute, 25%, and 50%. |
| Hold DOWN for at least 500 ms, then release | Open the session-reset confirmation. Automatic striking pauses immediately. |
| Press OK in the confirmation | Clear only the session count; retain the cumulative count. |
| Press and release UP or DOWN in the confirmation | Cancel without clearing. Waiting 10 seconds also cancels. |

Automatic striking stays paused after confirming, cancelling, or timing out.
Muting affects sound only; counting and animation continue. A manual OK press
also counts during automatic striking. Counts saturate at 4,294,967,295 rather
than wrapping to zero. If processing is delayed, automatic mode emits at most
one overdue strike; it does not replay a backlog.

The top-right indicator shows battery percentage, or `--%` when a reading is
unavailable. After 30 seconds without a function-button event in manual mode,
backlight brightness drops from 70% to 10%. Interaction restores 70%; automatic
mode keeps the display bright. This is display dimming, not deep sleep.

## Saving and power-off

Only the cumulative count, volume, and tempo are stored in local NVS. The session
count and whether automatic mode is running are intentionally not restored.

The save indicator distinguishes saved data from pending changes. After manual
striking or a setting change stops, the worker attempts to save after roughly
1.5 seconds of inactivity. During continuous changes, it attempts a batch save
after roughly 30 seconds from the first pending change. Failed writes are
retried after at least 5 seconds. These are scheduling targets, not a guarantee
against data loss during power failure or storage faults.

Before switching the device off, pause automatic striking, stop pressing keys,
and wait for the saved indicator. Cutting power earlier can lose the latest
unsaved cumulative count or settings. There is no claim of lossless recording
of every strike across abrupt power removal.

If NVS cannot initialize, or an existing record has an unknown schema, invalid
length, or invalid settings, the application preserves that storage and runs
with an in-memory counter. It does not erase NVS as a recovery step. A storage
warning means persistence is unavailable. Sound and button warnings remain
separate from save feedback; a sound failure does not disable counting.

## Develop and build

The application is developed on `feature/muyu`. The local macOS ARM64 setup uses
ESP-IDF v5.5.3 under `~/esp/esp-idf-v5.5.3`, managed tools under `~/.espressif`,
and Python 3.12. Activate the environment from the repository root:

```bash
source ./tools/activate-esp.sh
idf.py --version
./tools/validate.sh --static
./tools/validate.sh
```

The version must be `ESP-IDF v5.5.3`. The complete gate combines repository
checks, host tests, a firmware build, and merged-image verification. A successful
command must be recorded separately from physical-device acceptance; this guide
does not certify a particular build as passing.

Pure model and input checks can also run without ESP-IDF:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_muyu_model.c main/muyu_model.c -o /tmp/test_muyu_model
/tmp/test_muyu_model
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_muyu_input.c main/muyu_input.c -o /tmp/test_muyu_input
/tmp/test_muyu_input
python3 tools/generate_muyu_assets.py --check
```

The tests cover counting, saturation, beat scheduling, pause/reset behavior,
short/long press boundaries, duplicate edges, missing releases, and wake guards.
The asset check verifies the required glyph inventory and image size. Any host
LVGL renders are previews of the application layout and fonts; they do not
establish panel rendering, button timing, speaker output, or actual device RAM
headroom.

After the first firmware build, run `python3 tools/render_muyu_preview.py` to
reuse the downloaded LVGL 9.5.0 and render ten host UI states with glyph, layout,
and repeated-animation memory checks. Screenshots are saved under
`build/muyu-preview/screenshots/`, with results in
`build/muyu-preview/validation.log`.

| Source | Responsibility |
| --- | --- |
| `main/muyu_model.c` | Pure counting, automatic tempo, settings, and confirmation state. |
| `main/muyu_input.c` | Pure PRESS/RELEASE interpretation without waiting for click or double-click classification. |
| `main/muyu_ui.c` | Dedicated LVGL screen, status messages, confirmation, and strike animation. |
| `main/main.c` | Startup, queues, UI ownership, synthesized audio, battery reads, and NVS worker. |
| `assets/fonts/` and `assets/images/` | Licensed font subset and original wooden-fish artwork; see the [asset index](../../assets/README.md). |

Button callbacks only capture timestamps and queue edges. Slow audio and storage
operations run in the I/O worker; UI calls hold the BSP LVGL lock. Queue overflow
discards the incomplete gesture and resynchronizes input instead of interpreting
a missing release as a long press.

## Firmware installation

The complete build produces the merged image:

```text
build/FoloToy-AI-Passport-full.bin
```

This is the image for offset `0x0`. Do not substitute the application-only binary.
The gate also retains matching ELF/MAP and image files under
`build/firmware/<full-bin-sha256>/`; verify the chosen archive with:

```bash
python3 tools/archive_firmware.py verify build/firmware/<full-bin-sha256>
```

After confirming the target device and approving installation, use the
[local firmware installer](https://ai-passport.folotoy.cn/tools/web-flasher/)
with the verified merged image, or follow the repository's
[flashing and data policy](../development/engineering/firmware-layout.md#flashing-and-stored-data).
A merged flash replaces the installed application and can reset NVS, including
saved counts and settings. No full-chip erase or original-firmware readback is
required. Save any data you need before choosing a complete refresh.

## Device acceptance and limits

Physical-device tests have not been performed for this application. Record the
exact firmware hash and verify these behaviors after approved flashing:

1. Boot into the Muyu screen; inspect all Chinese labels, the full count range,
   and the confirmation without clipping or missing glyphs.
2. Check individual and rapid OK presses, UP/DOWN short presses, and long
   presses around the 500 ms boundary. Hold a key while powering on and release
   it before normal use.
3. Check the speaker at each volume, mute, automatic start/pause, all tempos,
   and confirmation cancellation/timeout.
4. Wait for saved status, restart, and check the cumulative count and settings;
   confirm the session is zero and automatic mode is off.
5. Check dimming and wake interaction, unavailable battery feedback, runtime
   heap margin, and sustained operation. Measure power consumption and battery
   life on the actual device if those matter for the intended use.

The sound is synthesized percussion, not a recording of a physical wooden fish.
Sound quality, timing, display output, persistence on real Flash, power use, and
battery life remain subject to device testing. Network services, cloud sync,
recording, and export are outside this version.
