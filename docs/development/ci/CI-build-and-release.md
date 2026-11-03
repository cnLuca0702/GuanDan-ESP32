[简体中文](CI-build-and-release.zh_CN.md) · **English**

# Build and Release

`.github/workflows/build-firmware.yml` builds firmware on tag pushes and manual dispatch.
It restores ccache, runs `./tools/validate.sh --firmware` with ESP-IDF 5.5.3 for ESP32-C3,
verifies the generated image layout and 8 MB Flash settings, and uploads
`FoloToy-AI-Passport-full.bin` as an Actions artifact. Ordinary branch pushes use the
separate static and firmware check workflows.

All Actions are pinned to full commit SHAs. The build workflow has `contents: read`.
It does not create or update GitHub Releases: release assets are published explicitly
from the exact firmware accepted during device testing. CI rebuilds may contain different
build metadata and must not silently replace that accepted binary.

## Preparing a release

1. Verify the archive containing the accepted firmware, and retain the full-image SHA-256.
2. Update both [changelog](../../CHANGELOG.md) languages with the current product's verified user-visible changes. Keep a fresh `Unreleased` section; do not treat inherited upstream notes as this product's release history.
3. Commit the matching source and documentation. Use an app-specific tag such as `v1.0.0-guandan` and a title such as `v1.0.0 Guandan Scoreboard`.
4. Publish the exact full image, SHA-256 checksums, and a clean installation package. Include the verified component images for segmented flashing. Exclude credentials, personal paths, raw device logs, and local debug manifests.
5. Write English and Simplified Chinese release notes covering features, controls, installation, stored-data effects, build commands, and the actual test scope. Confirm the published title and download hashes.

## Installation and stored data

The [official web flasher](https://ai-passport.folotoy.cn/tools/web-flasher/) writes
the merged `FoloToy-AI-Passport-full.bin` at `0x0`. This replaces the current application.
The merged image includes padding across storage partitions and can erase saved settings.
When preserving data is required, verify the actual partition layout first and follow the
release package's segmented-flashing instructions. Never put the application-only binary
at `0x0`; this project's application offset is `0x10000`. A full-chip erase is not required.

See [Firmware Layout](../engineering/firmware-layout.md) and the
[Guandan application guide](../../applications/guandan.md).

## Rebuilding

Activate ESP-IDF 5.5.3, then run `./tools/validate.sh` for the complete gate or
`./tools/validate.sh --firmware` for build, merge, verification, and archive creation.
`idf.py build` alone does not produce the verified merged delivery image.
A successful build is not evidence that the rebuilt bytes were tested on hardware.

Community submission is separate from a GitHub Release; see
[publish-to-community.md](../release/publish-to-community.md).
