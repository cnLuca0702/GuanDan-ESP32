[简体中文](guandan.zh_CN.md) · **English**

# Guandan Scoreboard

An offline, two-team level scoreboard for AI Passport. The red team occupies
the upper half of the portrait screen and the blue team occupies the lower
half. Each side shows its current level in large type, with smaller button
instructions. No phone, network, account, or Mac bridge is needed during play.

## Levels and controls

Both teams start at `2`. The 15 levels, in order, are:

```text
2  3  4  5  6  7  8  9  10  J  Q  K  A1  A2  A3
```

This is a manual scoreboard: players decide the result of each round and
adjust the corresponding team. It does not adjudicate game rules or advance
levels automatically.

| Button | Short press | Hold for at least 650 ms |
| --- | --- | --- |
| Up | Increase the red team's level by one | Decrease the red team's level by one |
| Down | Increase the blue team's level by one | Decrease the blue team's level by one |
| OK | Open reset confirmation; a second short press confirms | Does not confirm a reset |

A short press takes effect on release. A long press decreases exactly once,
does not increase first, and does not repeat while held. Releasing a long press
does not add another action. Levels stop at `2` and `A3`; they never wrap.

## Reset confirmation

Press and release OK once to open confirmation, then press and release OK
again to reset **both teams to `2`**. While confirmation is open:

- Up or Down cancels confirmation without changing either team's level.
- Waiting 8 seconds cancels confirmation without changing either level.
- Only a second short OK press confirms the reset.

## Saved state and screen brightness

The application attempts to save locally after approximately 500 ms without
another level change, or approximately every 5 seconds during continuous
changes. A failed save is retried after at least 5 seconds. A restart restores
the last successfully saved pair of levels. With no saved state, both teams
start at `2`. Storage work runs outside button callbacks; a power cut before a
pending save finishes may lose the most recent change. If storage is
unavailable, scoring remains usable for the current session, but those changes
cannot be restored after restarting.

The screen stays on. Its normal backlight is 80%; after 3 minutes without input
it drops to 30%. The next button press restores 80% brightness **and performs
its normal action**. There is no wake-only press, automatic screen-off, or deep
sleep in this application. Readability and battery runtime still require
measurement on the actual device.

## Build and delivery

From the repository root, activate the local ESP-IDF installation and run the
complete repository gate after integrating the application:

```bash
source tools/activate-esp.sh
idf.py --version  # must report ESP-IDF v5.5.3
./tools/validate.sh
```

The delivery image is `build/FoloToy-AI-Passport-full.bin`. Retain its exact
SHA-256 and the matching bundle under `build/firmware/<full-bin-sha256>/`,
including `manifest.json`, ELF, MAP, component images, and `flash_args`.
Verify that bundle without rebuilding:

```text
python3 tools/archive_firmware.py verify <archive-directory>
```

See [Build and Test](../development/engineering/build-and-test.md) for the
complete gate and [Firmware Layout](../development/engineering/firmware-layout.md)
for flashing offsets and stored-data behavior. Building does not flash a device.

## Keeping existing Muse settings

The Muse v1.0.0 community package and this checkout use the same default layout:

| Partition | Offset | Size |
| --- | ---: | ---: |
| NVS | `0x9000` | `0x6000` |
| PHY data | `0xF000` | `0x1000` |
| Factory application | `0x10000` | `0x7F0000` |

Muse keeps its provisioning record in the `muse` namespace. Guandan stores its
scores at `guandan_v1/scores` and does not erase the entire NVS partition on an
initialization error. With compatible layouts, an authorized segmented write
of verified component images can leave NVS and PHY data untouched. This is a
preservation path to verify on hardware, not a guarantee about an uninspected
device's current contents.

A merged image written from `0x0` includes padding across NVS and can erase
existing settings. Do not use that route when those settings must be kept.
The application-only image belongs at `0x10000`, never `0x0`. Installing Guandan
replaces the running Muse application; retaining Muse's settings does not run
both applications at once. No Flash readback is a prerequisite, and a full-chip
erase is not part of this workflow.

## Device acceptance

Verified on 2026-11-03 for v1.0.0, full-image SHA-256
`41b48f76bb0bd211a963b2a3de0008fab46e6836f01dce63b880a3186299e9ea`:

- **Build: PASS.** The complete firmware gate and matching archive verification passed.
- **Host tests: PASS.** Includes 27 model/input groups, font checks, 24 rendered states, and 2,000 UI transitions.
- **Device tests: PASS for the checks below.** Authorized segmented flashing, startup, and the user's display/control acceptance passed.

The user confirmed the red/blue layout and level labels, red increments through
A3 and holds at that limit, a long press lowers it to A2, blue short/long presses
and the lower limit at 2, cancellation with Up, and a two-OK reset of both sides.
After the user saved red 5 / blue 7, a USB reset restored exactly those levels
in the startup log without a panic or assertion.

The actual installed partition table matched the verified archive. All three
writes passed their data-hash checks. NVS and PHY bytes were hashed in memory
before and after flashing, before the first application boot; the hashes were
identical. No raw credential-bearing region was saved. Runtime score saves may
change NVS's physical bytes while keeping other namespaces logically intact.

**Unverified:** physical 8-second timeout and Down-button cancellation, exact
long-press timing, 3-minute dimming and first-key brightness behavior, battery
runtime, and a later restoration of Muse. Host tests cover the logical timeout,
Down cancellation, and long-press threshold. Do not treat the checks above as
hardware acceptance of a different build.

Local evidence is retained under `build/guandan-device-test-20261103/`; the
matching firmware/debug archive is under `build/firmware/<full-bin-sha256>/`.
