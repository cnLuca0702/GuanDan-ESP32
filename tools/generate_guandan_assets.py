#!/usr/bin/env python3
"""Rebuild and verify Guandan fonts, keeping asset tooling outside firmware.

  python tools/generate_guandan_assets.py --subset-noto SOURCE.otf --subset-rank SOURCE.ttf
  python tools/generate_guandan_assets.py --generate
  python tools/generate_guandan_assets.py --check

Subsetting requires fonttools==4.60.1. Generation requires lv_font_conv==1.5.3:
  npm install --prefix build/guandan-assets-tools --no-audit --no-fund --save-exact lv_font_conv@1.5.3
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / "assets/fonts"
INVENTORY = FONTS / "guandan_characters.txt"
RANK_CHARS = "0123456789JQKA"


def required_chars() -> set[int]:
    return set(range(32, 127)) | {ord(c) for c in INVENTORY.read_text() if not c.isspace()}


def make_subset(noto_source: Path, rank_source: Path) -> None:
    import fontTools
    from fontTools import subset
    from fontTools.ttLib import TTFont
    assert fontTools.__version__ == "4.60.1", "Use fonttools==4.60.1"
    files = {}
    for name, source, cps, output in (
        ("noto", noto_source, required_chars(), "guandan_noto_subset.otf"),
        ("rank", rank_source, {ord(c) for c in RANK_CHARS}, "guandan_montserrat_bold_subset.ttf"),
    ):
        font = TTFont(source)
        missing = cps - set(font.getBestCmap())
        if missing:
            raise SystemExit(f"Source {name} missing {sorted(missing)}")
        options = subset.Options()
        options.recalc_timestamp = False
        options.canonical_order = True
        options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
        options.name_legacy = True
        sub = subset.Subsetter(options=options)
        sub.populate(unicodes=cps)
        sub.subset(font)
        path = FONTS / output
        font.save(path)
        assert cps <= set(TTFont(path).getBestCmap())
        files[name] = {
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "subset_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "subset_file": output,
            "codepoints": [f"U+{cp:04X}" for cp in sorted(cps)],
        }
    files["noto"]["source_url"] = "https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf"
    files["rank"]["source_url"] = "https://raw.githubusercontent.com/lvgl/lvgl/v9.5.0/tests/src/test_files/fonts/Montserrat-Bold.ttf"
    manifest = {"fonts": files, "fonttools": "4.60.1", "lv_font_conv": "1.5.3", "bpp": 4, "compressed": False, "small_sizes": [14, 18], "rank_size": 84}
    (FONTS / "guandan_font_manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print("Source font subsets generated and cmap coverage checked")


def generate() -> None:
    prefix = ROOT / "build/guandan-assets-tools/node_modules"
    package = json.loads((prefix / "lv_font_conv/package.json").read_text())
    assert package["version"] == "1.5.3"
    converter = prefix / ".bin/lv_font_conv"
    text = "".join(chr(cp) for cp in sorted(required_chars()))
    for size in (14, 18, 84):
        font = "guandan_montserrat_bold_subset.ttf" if size == 84 else "guandan_noto_subset.otf"
        subprocess.run([str(converter), "--font", f"assets/fonts/{font}",
                        "--symbols", RANK_CHARS if size == 84 else text,
                        "--size", str(size), "--bpp", "4", "--format", "lvgl", "--no-compress",
                        "--lv-font-name", f"guandan_font_{size}", "--lv-include", "lvgl.h",
                        "--output", f"assets/fonts/guandan_font_{size}.c"], cwd=ROOT, check=True)
    print("Guandan LVGL fonts generated: 14, 18, 84 px")


def numeric_array(source: str, name: str) -> list[int]:
    match = re.search(rf"\b{re.escape(name)}\[\]\s*=\s*\{{(.*?)\}};", source, re.S)
    assert match, f"Missing generated array {name}"
    body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.S)
    return [int(n, 0) for n in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", body)]


def mapped_glyphs(path: Path) -> dict[int, int]:
    source = path.read_text()
    match = re.search(r"\bcmaps\[\]\s*=\s*\{(.*?)\n\};", source, re.S)
    assert match
    result = {}
    for block in re.findall(r"\{([^{}]+)\}", match.group(1)):
        fields = dict(re.findall(r"\.(\w+)\s*=\s*([^,}]+)", block))
        start, length, glyph_start = (int(fields[k].strip(), 0) for k in ("range_start", "range_length", "glyph_id_start"))
        kind = fields["type"].strip()
        if kind == "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY":
            pairs = ((start + i, glyph_start + i) for i in range(length))
        elif kind == "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY":
            offsets = numeric_array(source, fields["unicode_list"].strip())
            assert len(offsets) == int(fields["list_length"].strip(), 0)
            assert all(0 <= offset < length for offset in offsets)
            pairs = ((start + offset, glyph_start + i) for i, offset in enumerate(offsets))
        else:
            raise ValueError(f"Unsupported cmap {kind}")
        for cp, glyph in pairs:
            assert glyph > 0 and cp not in result
            result[cp] = glyph
    descriptors = re.search(r"\bglyph_dsc\[\]\s*=\s*\{(.*?)\n\};", source, re.S)
    assert descriptors and max(result.values()) < len(re.findall(r"\.bitmap_index\s*=", descriptors.group(1)))
    return result


def check() -> None:
    expected = required_chars()
    ui = ROOT / "main/guandan_ui.c"
    if ui.exists():
        strings = re.findall(r'"(?:\\.|[^"\\])*"', ui.read_text())
        used = {ord(c) for string in strings for c in string if ord(c) > 127}
        missing = used - expected
        if missing:
            raise SystemExit("UI inventory missing: " + " ".join(f"U+{cp:04X} {chr(cp)}" for cp in sorted(missing)))
    for size in (14, 18, 84):
        wanted = {ord(c) for c in RANK_CHARS} if size == 84 else expected
        actual = mapped_glyphs(FONTS / f"guandan_font_{size}.c")
        assert wanted <= actual.keys(), f"Missing glyphs in font {size}: {wanted - actual.keys()}"
        assert 0x9F98 not in actual
        print(f"guandan_font_{size}: PASS ({len(wanted)} glyphs, negative case U+9F98 absent)")
    manifest = json.loads((FONTS / "guandan_font_manifest.json").read_text())
    for font in manifest["fonts"].values():
        assert hashlib.sha256((FONTS / font["subset_file"]).read_bytes()).hexdigest() == font["subset_sha256"]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--subset-noto", type=Path)
    parser.add_argument("--subset-rank", type=Path)
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if bool(args.subset_noto) != bool(args.subset_rank):
        parser.error("Pass both source fonts together")
    if args.subset_noto:
        make_subset(args.subset_noto, args.subset_rank)
    if args.generate:
        generate()
    if args.check:
        check()
    if not any((args.subset_noto, args.generate, args.check)):
        parser.error("Choose --generate, --check, or both --subset options")


if __name__ == "__main__":
    main()
