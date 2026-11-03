#!/usr/bin/env python3
"""Generate Muyu font subsets or verify emitted LVGL glyph maps without dependencies.

  python tools/generate_muyu_assets.py --subset /path/to/NotoSansSC-Regular.otf
  node tools/generate_muyu_assets.mjs
  python tools/generate_muyu_assets.py --check

Only --subset requires fonttools==4.60.1. Generated fonts use lv_font_conv==1.5.3.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "assets/fonts"
SUBSET = FONT_DIR / "muyu-noto-sans-sc-subset.otf"
INVENTORY = FONT_DIR / "muyu-characters.txt"


def expected_characters() -> set[int]:
    return set(range(32, 127)) | {ord(c) for c in INVENTORY.read_text() if not c.isspace()}


def make_subset(source: Path) -> None:
    from fontTools import subset
    from fontTools.ttLib import TTFont
    import fontTools
    if fontTools.__version__ != "4.60.1":
        raise SystemExit("Use fonttools==4.60.1 for reproducible subsetting")
    source_font = TTFont(source)
    missing = expected_characters() - set(source_font.getBestCmap())
    if missing:
        raise SystemExit(f"Source font missing: {sorted(missing)}")
    options = subset.Options()
    options.recalc_timestamp = False
    options.canonical_order = True
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
    options.name_legacy = True
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=expected_characters())
    subsetter.subset(source_font)
    source_font.save(SUBSET)
    actual = set(TTFont(SUBSET).getBestCmap())
    assert expected_characters() <= actual
    metadata = {
        "family": "Noto Sans SC Regular (application subset)",
        "source_url": "https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf",
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "subset_sha256": hashlib.sha256(SUBSET.read_bytes()).hexdigest(),
        "license": "SIL Open Font License 1.1; see muyu-noto-OFL.txt",
        "fonttools_version": fontTools.__version__,
        "lv_font_conv_version": "1.5.3",
        "sizes_px": [16, 20],
        "bpp": 4,
        "compressed": False,
        "codepoints": [f"U+{cp:04X}" for cp in sorted(actual)],
    }
    (FONT_DIR / "muyu-font-manifest.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
    print(f"Subset: {len(actual)} code points, {SUBSET.stat().st_size} bytes")


def numeric_array(source: str, name: str) -> list[int]:
    match = re.search(rf"\b{re.escape(name)}\[\]\s*=\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise ValueError(f"Missing generated array {name}")
    body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.S)
    return [int(n, 0) for n in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", body)]


def mapped_glyphs(path: Path) -> dict[int, int]:
    source = path.read_text()
    match = re.search(r"\bcmaps\[\]\s*=\s*\{(.*?)\n\};", source, re.S)
    if not match:
        raise ValueError(f"Missing cmaps in {path}")
    result: dict[int, int] = {}
    for block in re.findall(r"\{([^{}]+)\}", match.group(1)):
        fields = dict(re.findall(r"\.(\w+)\s*=\s*([^,}]+)", block))
        start = int(fields["range_start"].strip(), 0)
        length = int(fields["range_length"].strip(), 0)
        glyph_start = int(fields["glyph_id_start"].strip(), 0)
        map_type = fields["type"].strip()
        if map_type == "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY":
            pairs = ((start + i, glyph_start + i) for i in range(length))
        elif map_type == "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY":
            offsets = numeric_array(source, fields["unicode_list"].strip())
            assert len(offsets) == int(fields["list_length"].strip(), 0)
            assert all(0 <= offset < length for offset in offsets)
            pairs = ((start + offset, glyph_start + i) for i, offset in enumerate(offsets))
        else:
            raise ValueError(f"Unsupported cmap {map_type}; update checker explicitly")
        for codepoint, glyph in pairs:
            assert glyph > 0 and codepoint not in result
            result[codepoint] = glyph
    glyph_block = re.search(r"\bglyph_dsc\[\]\s*=\s*\{(.*?)\n\};", source, re.S)
    assert glyph_block
    glyph_count = len(re.findall(r"\.bitmap_index\s*=", glyph_block.group(1)))
    assert max(result.values()) < glyph_count
    return result


def check_assets() -> None:
    expected = expected_characters()
    ui_path = ROOT / "main/muyu_ui.c"
    if ui_path.exists():
        strings = re.findall(r'"(?:\\.|[^"\\])*"', ui_path.read_text())
        used = {ord(c) for string in strings for c in string if ord(c) > 127}
        missing_inventory = used - expected
        if missing_inventory:
            raise SystemExit("UI text missing from inventory: " + " ".join(f"U+{c:04X} {chr(c)}" for c in sorted(missing_inventory)))
    for size in (16, 20):
        path = FONT_DIR / f"muyu_font_{size}.c"
        actual = mapped_glyphs(path)
        missing = expected - set(actual)
        if missing:
            raise SystemExit(f"{path.name}: missing glyphs {sorted(missing)}")
        assert 0x9F98 not in actual, "negative coverage control (龘) unexpectedly present"
        print(f"{path.name}: PASS, {len(expected)} required glyphs, U+9F98 correctly absent")
    meta = json.loads((FONT_DIR / "muyu-font-manifest.json").read_text())
    assert hashlib.sha256(SUBSET.read_bytes()).hexdigest() == meta["subset_sha256"]
    image = (ROOT / "assets/images/muyu_woodfish.c").read_text()
    assert len(numeric_array(image, "muyu_woodfish_pixels")) == 154 * 115 * 2
    print("Muyu image: PASS, 154x115 RGB565 / 35420 bytes")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--subset", type=Path)
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.subset:
        make_subset(args.subset)
    else:
        check_assets()


if __name__ == "__main__":
    main()
