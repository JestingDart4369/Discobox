#!/usr/bin/env python3
"""
convert_song.py — turn a MIDI-to-JSON export into a SongNote[] C++ header
for Discobox's single-voice SongPlayer (include/Buzzer/SongsPlayer/SongPlayer.h).

WHY THIS EXISTS
Discobox's speaker is monophonic: SongPlayer::step() fires one Arduino
tone() call per note, and if two notes in the array share the same
time_ms, only the LAST one in the array actually sounds — the earlier
ones get overridden in the same loop pass before they're audible for
more than an instant. Dumping a polyphonic MIDI export straight into a
SongNote[] array (chords and all) produces exactly that: garbled,
near-silent overlaps. This script picks ONE note per onset — "melody
line extraction", highest note wins — before it ever writes a C++ line,
so what comes out is already safe to play on one voice.

INPUT FORMAT
A MIDI-to-JSON export shaped like:
    { "tracks": [ { "notes": [ {"midi": 65, "time": 2.03, "duration": 0.29}, ... ] }, ... ] }
"time" and "duration" are in SECONDS (as most piano-roll / MIDI->JSON
tools export them); this script converts to the milliseconds SongNote
expects. If your tool's field names differ, adjust load_notes() below.

USAGE
    python3 convert_song.py <input.json> <ArrayName> <output.h> [--track N]

    <input.json>   the MIDI-to-JSON export
    <ArrayName>    C++ identifier for the array, e.g. BackInTime_Notes
                   (the _count variable is "<ArrayName>_count" with the
                   trailing "_Notes"/"_notes" stripped if present, to
                   match the existing song headers' naming — see below)
    <output.h>     where to write the generated header (include guard +
                   filename comment are derived from this path)
    --track N      which track index to read notes from (default: the
                   track with the most notes — usually the melody/lead
                   when the others are empty or sparse)

EXAMPLE
    cd doc/sound/songs
    python3 convert_song.py \\
        ../../../include/Buzzer/SongsPlayer/Songs/BackInTime_LeadONLY_Song.json \\
        BackInTime_Notes \\
        ../../../include/Buzzer/SongsPlayer/Songs/BackInTime_song.h

ALGORITHM (see extract_melody())
    1. Flatten every note in the chosen track to (onset_ms, midi, dur_ms).
    2. Sort by onset, then group notes into onset "buckets" ~15ms wide —
       notes meant to start together often land a few ms apart in an
       export; treat them as one chord/onset.
    3. Within a bucket, keep only the highest MIDI note (the melody is
       usually the top voice; bass/harmony notes are dropped).
    4. Merge onsets that are still closer than 25ms apart even after
       bucketing (fast grace notes/ornaments collapse into the earlier one).
    5. Cap each note's held duration at the gap to the next onset (or its
       own source duration, whichever is shorter) — the speaker is
       monophonic, a note can never ring into the next one anyway.

ADD A NEW SONG TO DISCOBOX
    1. Export/find a MIDI-to-JSON file for it.
    2. Run this script to generate the header (see EXAMPLE above).
    3. #include the new header in include/Buzzer/Buzzer_Player.h and add
       one line to SONGLIST. Done — see that file's own comments.
"""

import json
import re
import sys
from pathlib import Path

ONSET_BUCKET_MS = 15   # notes within this window of each other = one onset
MERGE_GAP_MS    = 25   # onsets still this close after bucketing get merged
MIN_DURATION_MS = 20   # never emit a note too short to hear


def load_notes(json_path: str, track_index):
    """Return the raw note list [{midi, time_s, duration_s}, ...] from one track."""
    with open(json_path) as f:
        data = json.load(f)

    tracks = data["tracks"]
    if track_index is None:
        # auto-pick: the track with the most notes (melody/lead track,
        # or the only one with content when others are empty)
        track_index = max(range(len(tracks)), key=lambda i: len(tracks[i].get("notes", [])))
        print(f"  (auto-picked track {track_index}: "
              f"{len(tracks[track_index].get('notes', []))} notes, "
              f"instrument={tracks[track_index].get('instrument')!r})")

    notes = tracks[track_index]["notes"]
    if not notes:
        raise ValueError(f"track {track_index} has no notes")
    return notes


def extract_melody(raw_notes):
    """Polyphonic note list -> monophonic [(onset_ms, midi, duration_ms), ...]."""
    # 1. flatten to milliseconds, sort by onset
    flat = sorted(
        (round(n["time"] * 1000), n["midi"], round(n["duration"] * 1000))
        for n in raw_notes
    )

    # 2+3. bucket by onset, keep the highest note per bucket
    buckets = []   # list of [onset_ms, midi, duration_ms]
    for onset_ms, midi, dur_ms in flat:
        if buckets and onset_ms - buckets[-1][0] <= ONSET_BUCKET_MS:
            if midi > buckets[-1][1]:
                buckets[-1][1] = midi
                buckets[-1][2] = dur_ms
        else:
            buckets.append([onset_ms, midi, dur_ms])

    # 4. merge onsets still closer than MERGE_GAP_MS (keep the earlier one)
    merged = []
    for onset_ms, midi, dur_ms in buckets:
        if merged and onset_ms - merged[-1][0] < MERGE_GAP_MS:
            continue   # too close to the previous note to hear separately
        merged.append([onset_ms, midi, dur_ms])

    # 5. cap duration at the gap to the next onset
    for i in range(len(merged)):
        onset_ms, midi, dur_ms = merged[i]
        if i + 1 < len(merged):
            gap = merged[i + 1][0] - onset_ms
            dur_ms = min(dur_ms, gap) if gap > 0 else dur_ms
        merged[i][2] = max(MIN_DURATION_MS, dur_ms)

    return [(m[1], m[2], m[0]) for m in merged]   # (midi, duration_ms, time_ms) — SongNote order


def cpp_identifier(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def write_header(notes, array_name: str, output_path: str, source_name: str):
    guard = re.sub(r"[^A-Za-z0-9]", "_", Path(output_path).stem).upper() + "_H"
    count_name = re.sub(r"_?[Nn]otes$", "", array_name) + "_notes_count"
    total_ms = notes[-1][2] + notes[-1][1] if notes else 0

    lines = [
        f"// Auto-generated from {source_name} by doc/sound/songs/convert_song.py — do not edit by hand.",
        f"#ifndef {guard}",
        f"#define {guard}",
        '#include "Buzzer/SongsPlayer/SongPlayer.h"',
        "",
        f"// {len(notes)} notes, total duration ~{total_ms / 1000:.1f} s",
        f"const SongNote {array_name}[] = {{",
    ]
    for midi, dur_ms, time_ms in notes:
        lines.append(f"  {{ {midi:3d}, {dur_ms:5d}, {time_ms:7d}u }},")
    lines += [
        "};",
        "",
        f"const size_t {count_name} = sizeof({array_name}) / sizeof({array_name}[0]);",
        "",
        f"#endif // {guard}",
        "",
    ]

    Path(output_path).write_text("\n".join(lines))
    print(f"  wrote {output_path}  ({len(notes)} notes, ~{total_ms / 1000:.1f}s)")


def main():
    args = sys.argv[1:]
    track_index = None
    if "--track" in args:
        i = args.index("--track")
        track_index = int(args[i + 1])
        del args[i:i + 2]

    if len(args) != 3:
        print(__doc__)
        sys.exit(1)

    input_json, array_name, output_h = args
    array_name = cpp_identifier(array_name)

    print(f"reading {input_json} ...")
    raw = load_notes(input_json, track_index)
    print(f"  {len(raw)} raw notes")

    notes = extract_melody(raw)
    print(f"  {len(notes)} notes after melody extraction "
          f"({len(raw) - len(notes)} dropped as chord/harmony/duplicate)")

    write_header(notes, array_name, output_h, Path(input_json).name)


if __name__ == "__main__":
    main()
