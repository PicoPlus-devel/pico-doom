#!/usr/bin/env python3
#
# Copyright(C) 2026 Frank Hoedemakers
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# DESCRIPTION:
#	Builds the Options menu labels this port adds of its own ("Nes Pad:" and
#	"Bootsel Mode") and writes them out as src/vpatch_builtin.c, vpatches the
#	firmware carries itself.
#
#	The other custom menu graphics in this repo (M_GAME, M_NETWK, ...) are
#	injected into the WHD by whd_gen. These deliberately are not: adding a
#	vpatch to the WHD changes NUM_VPATCHES, and r_data_whd.c refuses a WHD
#	whose table does not match, which would force every _full user to
#	regenerate /roms/doom/doom.whd. A built-in patch keeps the data format
#	untouched.
#
#	The lettering is not drawn: it is cut out of the stock menu graphics in
#	DOOM.WAD, so it is Doom's own small-caps menu font, kerned the way the
#	font kerns. Letters in those lumps touch, but only through their darkest
#	outline colour, so a column whose opaque pixels are all outline is a seam
#	between two letters. Each letter is cropped together with the outline
#	column to its left, which makes concatenation reproduce the original
#	spacing exactly -- verified below by rebuilding "Messages:" from its own
#	letters and comparing against the lump it came from.
#
#	One glyph is an exception. The menu font has no B anywhere -- the stock
#	lumps between them supply C D E F G H I K L M N O P Q R S T U V W and no
#	lowercase b either -- so "Bootsel" cannot be cut outright. splice() builds
#	one from the top of P and the bottom of D, which share the same stem and
#	bowl weight; see B_SPLICE_ROW.
#
#	Usage:  python3 tools/make_menu_vpatches.py [DOOM.WAD]
#	Any IWAD works; the menu graphics are identical in all of them.
#
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WAD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "DOOM.WAD")
C_OUT = os.path.join(ROOT, "src", "vpatch_builtin.c")

VP4_RUNS = 1        # enum vpatch_type in src/whddata.h
OUTLINE_RGB = (67, 0, 0)

# Row where the spliced B changes over from P to D. 7 leaves the waist too high
# and 9 doubles it up; 8 lands exactly on the stroke that closes P's bowl.
B_SPLICE_ROW = 8


# ---------------------------------------------------------------- WAD reading

def read_wad(path):
    data = open(path, "rb").read()
    _, numlumps, tab = struct.unpack_from("<4sii", data, 0)
    lumps = {}
    for i in range(numlumps):
        off, size, name = struct.unpack_from("<ii8s", data, tab + i * 16)
        lumps[name.rstrip(b"\0").decode("ascii", "replace")] = data[off:off + size]
    return lumps


def playpal(lumps):
    p = lumps["PLAYPAL"]
    return [(p[i * 3], p[i * 3 + 1], p[i * 3 + 2]) for i in range(256)]


def playpal_pages(lumps):
    """Every PLAYPAL page, i.e. the base palette plus all 13 flash variants."""
    p = lumps["PLAYPAL"]
    return [p[i:i + 768] for i in range(0, len(p), 768)]


def fold_duplicates(pix, pages):
    """Merge palette indices that hold the same colour on *every* PLAYPAL page.

    The red ramp appears twice in PLAYPAL and the menu lumps do not all pick
    the same copy: "Bootsel Mode" pulls letters from six of them and lands on
    18 distinct indices, where vp4 has room for 16. Two indices that are
    byte-identical on all 14 pages render the same under every palette flash
    the game can apply, so folding them is lossless -- not a quantization.
    Here it takes 47 -> 191 and 45 -> 190, which is exactly the 2 needed.
    """
    def same(a, b):
        return all(pg[a * 3:a * 3 + 3] == pg[b * 3:b * 3 + 3] for pg in pages)

    canon, reps = {}, []
    for idx in sorted({c for row in pix for c in row if c is not None}):
        canon[idx] = next((r for r in reps if same(idx, r)), idx)
        if canon[idx] == idx:
            reps.append(idx)
    folded = [(k, v) for k, v in canon.items() if k != v]
    return [[None if c is None else canon[c] for c in row] for row in pix], folded


def patch_to_idx(b):
    """Classic Doom patch -> rows of palette index (or None where transparent)."""
    w, h, _lo, _to = struct.unpack_from("<hhhh", b, 0)
    offs = struct.unpack_from("<%di" % w, b, 8)
    pix = [[None] * w for _ in range(h)]
    for x in range(w):
        p = offs[x]
        while b[p] != 0xFF:
            topdelta, length = b[p], b[p + 1]
            p += 3                      # topdelta, length, unused pad byte
            for i in range(length):
                y = topdelta + i
                if 0 <= y < h:
                    pix[y][x] = b[p + i]
            p += length + 1             # data, unused pad byte
    return pix


# ------------------------------------------------------------ letter cutting

def split_letters(pix, pal):
    """Column spans of the individual letters, cutting at outline-only columns.

    Matched on colour, not palette index: rgb(67,0,0) sits at two places in
    PLAYPAL and the menu font uses the one in the red ramp.
    """
    h, w = len(pix), len(pix[0])
    seam = {x for x in range(w)
            if all(pix[y][x] is None or pal[pix[y][x]] == OUTLINE_RGB
                   for y in range(h))}
    spans, x = [], 0
    while x < w:
        if x in seam:
            x += 1
            continue
        x0 = x
        while x < w and x not in seam:
            x += 1
        spans.append((x0, x))
    return spans


def crop(pix, x0, x1):
    return [row[x0:x1] for row in pix]


def cat(blocks):
    return [sum((b[y] for b in blocks), []) for y in range(len(blocks[0]))]


def gap(n, h=15):
    return [[None] * n for _ in range(h)]


def splice(top, bottom, row):
    """Rows above `row` from `top`, the rest from `bottom`.

    Both blocks must already be the same width, which is why the B below takes
    P and D: they are the two 15-wide capitals with a full-height left stem.
    """
    assert len(top[0]) == len(bottom[0]), "splice needs equal widths"
    return [(top if y < row else bottom)[y][:] for y in range(len(top))]


# --------------------------------------------------------------- vp4_runs

def encode_vp4_runs(pix):
    """Encode to the vp4_runs body decoded by V_DrawPatchList in v_video.c.

    `pix` holds 4-bit palette indices, or None where the patch is transparent.
    """
    h, w = len(pix), len(pix[0])
    out = bytearray()
    for y in range(h):
        row, x, end = pix[y], 0, 0
        while x < w:
            while x < w and row[x] is None:
                x += 1
            if x >= w:
                break
            x0 = x
            while x < w and row[x] is not None:
                x += 1
            run = row[x0:x]
            assert x0 - end < 0xFF, "gap too wide for a byte"
            out.append(x0 - end)
            out.append(len(run))
            for i in range(1, len(run), 2):
                out.append(run[i - 1] | (run[i] << 4))
            if len(run) & 1:
                out.append(run[-1])
            end = x
        if end != w:
            out.append(0xFF)            # the decoder stops early on a full row
    return bytes(out)


def decode_vp4_runs(data, w, h):
    """Mirror of the vp4_runs case in V_DrawPatchList, used to check the encoder.

    Returns 4-bit indices rather than screen bytes, so it compares directly
    against what was handed to encode_vp4_runs().
    """
    pix = [[None] * w for _ in range(h)]
    p = 0
    for y in range(h):
        x = 0
        while True:
            g = data[p]; p += 1
            if g == 0xFF:
                break
            x += g
            ln = data[p]; p += 1
            i = 1
            while i < ln:
                v = data[p]; p += 1
                pix[y][x] = v & 0xF; x += 1
                pix[y][x] = v >> 4; x += 1
                i += 2
            if ln & 1:
                v = data[p]; p += 1
                pix[y][x] = v & 0xF; x += 1
            assert x <= w
            if x == w:
                break
    assert p == len(data), "encoder/decoder disagree on length"
    return pix


def build_vpatch(pix, palette):
    """`pix` holds 4-bit indices into `palette`, which holds PLAYPAL indices."""
    assert len(palette) <= 16, "vp4 has only 4 bits of palette index"
    h, w = len(pix), len(pix[0])
    assert h < 256 and w < 512
    flags = (VP4_RUNS << 2) | ((w >> 8) << 1)   # bit 0 = shared palette: no
    hdr = bytes([w & 0xFF, h, len(palette), flags, 0, 0])
    return hdr + bytes(palette) + encode_vp4_runs(pix)


# ------------------------------------------------------------------- output

def write_png(path, pix, pal):
    h, w = len(pix), len(pix[0])
    raw = b""
    for row in pix:
        raw += b"\0" + b"".join(
            bytes(pal[c] + (255,)) if c is not None else b"\0\0\0\0" for c in row)

    def chunk(t, b):
        return (struct.pack(">I", len(b)) + t + b
                + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF))

    open(path, "wb").write(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">iiBBBBB", w, h, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b""))


C_TEMPLATE = '''//
// Copyright(C) 2026 Frank Hoedemakers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//\tvpatches the firmware carries itself, for menu graphics that have no lump
//\tin the WHD. Handles from VPATCH_BUILTIN_FIRST up resolve to this table
//\tinstead of to a WHD lump -- see resolve_vpatch_handle() in doom/r_data.h.
//
//\tPutting them here rather than in whd_gen's extra_patches.h is deliberate:
//\ta new WHD vpatch changes NUM_VPATCHES, and r_data_whd.c rejects a WHD
//\twhose table does not match, so it would force every _full user to
//\tregenerate /roms/doom/doom.whd from their own WAD.
//
//\tThe lettering is cut out of Doom's own menu graphics, so each label is in
//\tthe menu font and kerned the way that font kerns. All of them are
//\tvp4_runs (see enum vpatch_type in whddata.h): 4-bit palette indices, two
//\tpixels per byte, transparency carried by the gaps between runs.
//
// GENERATED by tools/make_menu_vpatches.py -- do not edit by hand.
//

#include "doomtype.h"
#include "v_patch.h"

#if USE_WHD

{blocks}const uint8_t *const builtin_vpatches[] = {{
{table}}};

static_assert(count_of(builtin_vpatches) == NUM_BUILTIN_VPATCHES, "");

#endif
'''

BLOCK_TEMPLATE = '''// "{text}" for the Options menu, {w}x{h}:
{sources}static const uint8_t {ident}[] = {{
{body}}};

'''


def c_array(data, indent="        ", per_line=12):
    out = ""
    for i in range(0, len(data), per_line):
        out += indent + " ".join("0x%02x," % b for b in data[i:i + per_line]) + "\n"
    return out


# --------------------------------------------------------------------- main

def main():
    lumps = read_wad(WAD)
    pal = playpal(lumps)
    src = {}

    def letters(name):
        if name not in src:
            pix = patch_to_idx(lumps[name])
            src[name] = (pix, split_letters(pix, pal))
        return src[name]

    def glyph(name, i, span=None, tail=False):
        """Letter i of `name`, carrying the outline column on its left."""
        pix, spans = letters(name)
        x0, x1 = span if span else spans[i]
        return crop(pix, max(0, x0 - 1), min(len(pix[0]), x1 + 1) if tail else x1)

    # Self-test: cut a stock word into letters and put it straight back
    # together. Every seam column is kept, so it must come out byte-identical.
    pix, spans = letters("M_MESSG")
    blocks, prev = [], 0
    for (a, b) in spans:
        blocks.append(crop(pix, prev, b))
        prev = b
    blocks.append(crop(pix, prev, len(pix[0])))
    if cat(blocks) != pix:
        sys.exit("self-test failed: M_MESSG does not survive a cut and rejoin")

    # Each source is (character, note for the generated comment, thunk). The
    # note is what documents where the pixels came from, so keep it accurate.
    #
    #   N  OPTIONS[5]      P  OPTIONS[1]
    #   e  Messages:[1]    a  Messages:[4]
    #   s  Messages:[2]    d  right half of the "ad" in "Load game"
    #   :  Messages:[8]
    NESPAD = [
        ("N", "M_OPTTTL letter 5", lambda: glyph("M_OPTTTL", 5)),
        ("e", "M_MESSG letter 1",  lambda: glyph("M_MESSG", 1)),
        ("s", "M_MESSG letter 2",  lambda: glyph("M_MESSG", 2)),
        (" ", None,                lambda: gap(10)),
        ("P", "M_OPTTTL letter 1", lambda: glyph("M_OPTTTL", 1)),
        ("a", "M_MESSG letter 4",  lambda: glyph("M_MESSG", 4)),
        ("d", "M_LOADG letter (45, 59)",
                                   lambda: glyph("M_LOADG", None, (45, 59))),
        ("",  None,                lambda: gap(2)),
        (":", "M_MESSG letter 8",  lambda: glyph("M_MESSG", 8, tail=True)),
    ]

    #   B  spliced, see B_SPLICE_ROW    s  Options[6]
    #   o  Options[4]                   e  Messages:[1]
    #   t  Options[2]                   l  Display[4]
    #   M  Mouse Sensitivity[0]         d  the "ad" in "Load game" again
    BOOTSEL = [
        ("B", "M_OPTTTL letter 1 (P) rows 0-%d + M_DISP letter 0 (D) below"
              % (B_SPLICE_ROW - 1),
                                   lambda: splice(glyph("M_OPTTTL", 1),
                                                  glyph("M_DISP", 0),
                                                  B_SPLICE_ROW)),
        ("o", "M_OPTION letter 4", lambda: glyph("M_OPTION", 4)),
        ("o", "M_OPTION letter 4", lambda: glyph("M_OPTION", 4)),
        ("t", "M_OPTION letter 2", lambda: glyph("M_OPTION", 2)),
        ("s", "M_OPTION letter 6", lambda: glyph("M_OPTION", 6)),
        ("e", "M_MESSG letter 1",  lambda: glyph("M_MESSG", 1)),
        ("l", "M_DISP letter 4",   lambda: glyph("M_DISP", 4)),
        (" ", None,                lambda: gap(10)),
        ("M", "M_MSENS letter 0",  lambda: glyph("M_MSENS", 0)),
        ("o", "M_OPTION letter 4", lambda: glyph("M_OPTION", 4)),
        # One column narrower than the d in "Nes Pad:" above. The "ad" in
        # "Load game" has no outline-only seam -- the two letters interleave
        # diagonally -- so a cut at 45 carries the a's bright right edge along
        # with it. After an a that reads as part of the pair; after this o it
        # would be a stray lit column, and 46 leaves a proper outline seam.
        ("d", "M_LOADG letter (46, 59)",
                                   lambda: glyph("M_LOADG", None, (46, 59))),
        ("e", "M_MESSG letter 1",  lambda: glyph("M_MESSG", 1, tail=True)),
    ]

    LABELS = [
        ("vpatch_m_nespad",  "VPATCH_M_NESPAD",  "Nes Pad:",     "m_nespad.png",
         NESPAD),
        ("vpatch_m_bootsel", "VPATCH_M_BOOTSEL", "Bootsel Mode", "m_bootsel.png",
         BOOTSEL),
    ]

    pages = playpal_pages(lumps)
    blocks, table = "", ""
    for (ident, enum, text, png, sources) in LABELS:
        label = cat([thunk() for (_c, _note, thunk) in sources])
        label, folded = fold_duplicates(label, pages)

        # The patch carries its own palette of PLAYPAL indices; the pixels
        # become 4-bit indices into that.
        palette = sorted({c for row in label for c in row if c is not None})
        if len(palette) > 16:
            sys.exit("%s needs %d colours, vp4 holds 16" % (text, len(palette)))
        remap = [[None if c is None else palette.index(c) for c in row]
                 for row in label]
        blob = build_vpatch(remap, palette)

        # Decode the encoded body back the way the firmware will and compare.
        h, w = len(label), len(label[0])
        if decode_vp4_runs(blob[6 + len(palette):], w, h) != remap:
            sys.exit("self-test failed: %s does not survive a vp4_runs round trip"
                     % text)

        write_png(os.path.join(ROOT, png), label, pal)
        srclines = "".join("//   %-2s %s\n" % (c, note)
                           for (c, note, _t) in sources if note)
        blocks += BLOCK_TEMPLATE.format(text=text, w=w, h=h, sources=srclines,
                                        ident=ident, body=c_array(blob))
        table += "        %-22s // %s\n" % (ident + ",", enum)

        print("%s  %dx%d, %d colours%s" % (png, w, h, len(palette),
              "" if not folded else "  (folded %s)" %
              ", ".join("%d->%d" % kv for kv in folded)))
        print("%s  %d bytes (%d header + %d palette + %d data)"
              % (ident, len(blob), 6, len(palette), len(blob) - 6 - len(palette)))
        for y in range(h):
            print("  " + "".join("." if c is None else
                                 ("-" if pal[c] == OUTLINE_RGB else "#")
                                 for c in label[y]))

    open(C_OUT, "w").write(C_TEMPLATE.format(blocks=blocks, table=table))


if __name__ == "__main__":
    main()
