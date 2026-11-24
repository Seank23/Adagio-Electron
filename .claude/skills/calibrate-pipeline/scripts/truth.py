"""Ground truth for a calibration, read from truth.json (format: references/transcription.md).

Notes are MIDI numbers or names ("Bb3", "F#5", C4 = 60). Chord names follow the chart ("Ebmaj7", "Ab/Bb", "Abm6",
"Db9", "F#m7b5", "N.C."); their tones come from the name unless "chordTones" gives them.
"""
import json
import re

LETTER = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
ENGINE_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B']  # AnalysisUtils.h NoteNames

# Intervals above the root for every quality ChordPredictor names, plus the usual chart spellings.
QUALITY = {
    '': [0, 4, 7], '6/9': [0, 4, 7, 2, 9], '11': [0, 4, 7, 2, 10, 5], '9': [0, 4, 7, 2, 10], 'maj13': [0, 4, 7, 2, 11, 9],
    'maj9': [0, 4, 7, 2, 11], 'add9': [0, 4, 7, 2], '6': [0, 4, 7, 9], '7b9': [0, 4, 7, 10, 1], '7#9': [0, 4, 7, 10, 3],
    '7': [0, 4, 7, 10], 'maj7': [0, 4, 7, 11], 'm6': [0, 3, 7, 9], 'm9': [0, 3, 7, 10, 2], 'm7': [0, 3, 7, 10],
    'mM7': [0, 3, 7, 11], 'm': [0, 3, 7], '7sus2': [0, 2, 7, 10], 'sus2': [0, 2, 7], '9sus4': [0, 5, 7, 10, 2],
    '7sus4': [0, 5, 7, 10], 'sus4': [0, 5, 7], 'dim7': [0, 3, 6, 9], 'm7b5': [0, 3, 6, 10], 'dim': [0, 3, 6],
    'aug': [0, 4, 8], '5': [0, 7], '13': [0, 4, 7, 10, 2, 9], 'm11': [0, 3, 7, 10, 2, 5], '7#5': [0, 4, 8, 10],
    '7b5': [0, 4, 6, 10], 'sus': [0, 5, 7],
}
ALIASES = [('min', 'm'), ('mi', 'm'), ('-', 'm'), ('M7', 'maj7'), ('Maj7', 'maj7'), ('Δ7', 'maj7'), ('Δ', 'maj7'),
           ('ø7', 'm7b5'), ('ø', 'm7b5'), ('°7', 'dim7'), ('o7', 'dim7'), ('°', 'dim'), ('+', 'aug'), ('maj', '')]
# Extensions ChordPredictor appends in brackets, "Cm7 (b9, 11)".
EXT = {'b9': 1, '9': 2, '#9': 3, 'b11': 4, '11': 5, '+11': 6, '#11': 6, '#5': 8, 'b13': 8, '6': 9, '13': 9, 'm7': 10, 'maj7': 11}


def pitch_class(name):
    m = re.fullmatch(r'([A-G])([#b]*)', name)
    if not m:
        raise ValueError(f'not a pitch class: {name!r}')
    return (LETTER[m.group(1)] + m.group(2).count('#') - m.group(2).count('b')) % 12


def midi(note):
    if isinstance(note, (int, float)):
        return int(note)
    m = re.fullmatch(r'([A-G])([#b]*)(-?\d)', note)
    if not m:
        raise ValueError(f'not a note: {note!r} (use names like "Bb3", C4 = 60)')
    return 12 * (int(m.group(3)) + 1) + LETTER[m.group(1)] + m.group(2).count('#') - m.group(2).count('b')


def note_name(m):
    return f'{ENGINE_NAMES[m % 12]}{m // 12 - 1}'


def normalise_quality(q):
    if q in QUALITY:
        return q
    for a, b in ALIASES:
        if q.startswith(a) and b + q[len(a):] in QUALITY:
            return b + q[len(a):]
    return q


def chord_tones(name):
    """Chart chord name -> (roots, pitch classes). A slash chord's bass counts as a root too."""
    name = name.strip()
    bass = None
    if '/' in name and not name.endswith('6/9'):
        name, bass = name.rsplit('/', 1)
    m = re.fullmatch(r'([A-G][#b]?)(.*?)(?:\s*\((.*)\))?', name)
    if not m:
        raise ValueError(f'cannot read chord {name!r}: give its tones in "chordTones"')
    root = pitch_class(m.group(1))
    quality = normalise_quality(m.group(2))
    if quality not in QUALITY:
        raise ValueError(f'unknown chord quality in {name!r}: give its tones in "chordTones"')
    ivs = list(QUALITY[quality])
    if m.group(3):
        for e in m.group(3).split(','):
            e = e.strip()
            if e not in EXT:
                raise ValueError(f'unknown extension {e!r} in {name!r}: give its tones in "chordTones"')
            ivs.append(EXT[e])
    pcs = {(root + i) % 12 for i in ivs}
    roots = {root}
    if bass:
        roots.add(pitch_class(bass))
        pcs.add(pitch_class(bass))
    return roots, pcs


def chord_root(name):
    """The root a chart chord is named on (Ab for Ab/Bb)."""
    return pitch_class(re.match(r'[A-G][#b]?', name.strip()).group(0))


def engine_chord(name):
    """A chord ChordPredictor named -> (root, pitch classes); an unknown quality reads as a triad."""
    m = re.match(r'^([A-G]b?)(.*?)(?: \((.*)\))?$', name)
    if not m:
        return None, set()
    root = pitch_class(m.group(1))
    ivs = list(QUALITY.get(m.group(2), [0, 4, 7]))
    if m.group(3):
        ivs += [EXT[e.strip()] for e in m.group(3).split(',') if e.strip() in EXT]
    return root, {(root + i) % 12 for i in ivs}


class Truth:
    def __init__(self, path):
        d = json.load(open(path, encoding='utf-8'))
        self.title = d.get('title', '')
        self.beats_per_bar = d.get('beatsPerBar', 4)
        self.key_credit = d['key']                                   # {"Eb Major": 1.0, "C Minor": 0.5}
        self.form = d.get('form') or list(range(1, len(d['bars']) + 1))   # recorded bar n plays score bar form[n-1]
        self.loose_octave_bars = set(d.get('looseOctaveBars', []))   # recorded bars where any octave of the melody counts
        self.bars = []
        for i, b in enumerate(d['bars']):
            try:
                self.bars.append({
                    'chords': [(float(beat), None if n in ('N.C.', 'NC', None) else n) for beat, n in b.get('chords', [])],
                    'melody': [(float(s), float(l), midi(n)) for s, l, n in b.get('melody', [])],
                    'bass': [(float(s), float(l), midi(n)) for s, l, n in b.get('bass', [])],
                })
            except Exception as ex:
                raise ValueError(f'score bar {i + 1}: {ex}') from ex
        overrides = d.get('chordTones', {})
        self.chords = {}
        for b in self.bars:
            for _, n in b['chords']:
                if n and n not in self.chords:
                    if n in overrides:
                        o = overrides[n]
                        self.chords[n] = ({pitch_class(r) for r in o['roots']}, {pitch_class(p) for p in o['tones']})
                    else:
                        self.chords[n] = chord_tones(n)
        if max(self.form) > len(self.bars):
            raise ValueError(f'form names score bar {max(self.form)} but there are {len(self.bars)}')

    def chord_at(self, bar, beat):
        """The chart chord in force at (0-based score bar, beat); a bar without one carries the last one on."""
        name = 'unset'
        for b, n in self.bars[bar]['chords']:
            if beat >= b:
                name = n
        k = bar
        while name == 'unset' and k > 0:
            k -= 1
            if self.bars[k]['chords']:
                name = self.bars[k]['chords'][-1][1]
        return None if name == 'unset' else name

    def sounding(self, bar, beat, part):
        return [m for (b, d, m) in self.bars[bar][part] if b <= beat < b + d]
