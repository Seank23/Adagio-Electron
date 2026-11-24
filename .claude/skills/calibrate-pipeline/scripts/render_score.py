"""Render sheet music to images for transcribing: every page, then every bar on its own, upscaled, with the chord
symbols and the vocal line above it. Reading one bar at a time is far more reliable than reading a page.

usage: python render_score.py score.pdf OUT_DIR [--zoom 3] [--scale 1.6]

Writes OUT_DIR/page1.png ... and OUT_DIR/bar001.png ... numbered in reading order, and prints each system's bar count.
Bars are found from the piano's grand staff (two staves joined by barlines); a staff above it that no barline joins (the
vocal line) is included in the crop. Check the count against the bar numbers printed at each system's start: a pickup
bar, a repeat sign or a first/second ending can shift the numbering. If detection fails, read the page images instead.
"""
import argparse
import os

try:
    import pymupdf as fitz
except ImportError:
    import fitz
import numpy as np
from PIL import Image


def line_rows(dark):
    """Centres of the horizontal staff lines: rows dark across most of the page width."""
    frac = dark.mean(axis=1)
    rows = np.where(frac > 0.45 * frac.max())[0]
    centres, run = [], [rows[0]] if len(rows) else []
    for r in rows[1:]:
        if r - run[-1] <= 2:
            run.append(r)
        else:
            centres.append(sum(run) / len(run))
            run = [r]
    if run:
        centres.append(sum(run) / len(run))
    return centres


def staves(lines):
    """Groups of five evenly spaced lines -> (top, bottom, spacing)."""
    out, i = [], 0
    while i + 4 < len(lines):
        gaps = np.diff(lines[i:i + 5])
        if gaps.max() - gaps.min() <= max(2.0, 0.25 * gaps.mean()):
            out.append((lines[i], lines[i + 4], gaps.mean()))
            i += 5
        else:
            i += 1
    return out


def barlines(dark, top, bottom, min_frac=0.97):
    """x of every column dark from top to bottom, merging neighbours."""
    span = dark[int(top):int(bottom) + 1, :]
    cols = np.where(span.mean(axis=0) >= min_frac)[0]
    groups = []
    for x in cols:
        if groups and x - groups[-1][-1] <= 3:
            groups[-1].append(x)
        else:
            groups.append([x])
    return [(g[0] + g[-1]) // 2 for g in groups]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pdf')
    ap.add_argument('out')
    ap.add_argument('--zoom', type=float, default=3.0)
    ap.add_argument('--scale', type=float, default=1.6)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    doc = fitz.open(a.pdf)
    bar_no = 0
    for pno, page in enumerate(doc, 1):
        path = os.path.join(a.out, f'page{pno}.png')
        page.get_pixmap(matrix=fitz.Matrix(a.zoom, a.zoom)).save(path)
        im = Image.open(path).convert('L')
        dark = np.array(im) < 128
        sts = staves(line_rows(dark))
        # Join staves that barlines connect (the piano's grand staff); the rest stand alone (vocal, guitar).
        systems, i = [], 0
        while i < len(sts):
            group = [sts[i]]
            while i + 1 < len(sts) and len(barlines(dark, group[-1][0], sts[i + 1][1])) >= 2:
                i += 1
                group.append(sts[i])
            systems.append(group)
            i += 1
        prev_bottom = 0
        pending_above = None
        for group in systems:
            if len(group) == 1:                       # unjoined staff: belongs with the grand staff below it
                pending_above = pending_above or group[0]
                continue
            top_staff = pending_above or group[0]
            pending_above = None
            sp = group[0][2]
            top = max(prev_bottom, top_staff[0] - 12 * sp)   # chord names sit above the guitar grids
            bottom = min(im.height, group[-1][1] + 3 * sp)      # bass ledger lines, not the next chord name
            xs = barlines(dark, group[0][0], group[-1][1])
            count = 0
            for x0, x1 in zip(xs, xs[1:]):
                if x1 - x0 < 6 * sp:                  # a double barline or a repeat sign, not a bar
                    continue
                bar_no += 1
                count += 1
                crop = im.crop((max(0, x0 - 12), int(top), min(im.width, x1 + 12), int(bottom))).convert('RGB')
                crop = crop.resize((int(crop.width * a.scale), int(crop.height * a.scale)), Image.LANCZOS)
                crop.save(os.path.join(a.out, f'bar{bar_no:03d}.png'))
            print(f'page {pno}: system of {len(group)} staves{" + 1 above" if top_staff is not group[0] else ""}, '
                  f'{count} bars (bar{bar_no - count + 1:03d}-bar{bar_no:03d})')
            prev_bottom = bottom
    print(f'{bar_no} bars in {len(doc)} pages -> {a.out}')


if __name__ == '__main__':
    main()
