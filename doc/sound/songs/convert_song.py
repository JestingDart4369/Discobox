#!/usr/bin/env python3
"""
convert_song.py — turn a VisiPiano/MIDI-JSON export into a SongNote[] C++ header
for Discobox's SongPlayer (include/Buzzer/SongsPlayer/SongPlayer.h).

USAGE
    # single file
    python3 convert_song.py <input.json> <ArrayName> <output.h> [--track N]

    # whole folder — converts every *.json in <input_dir> into <output_dir>/
    python3 convert_song.py <input_dir> <output_dir>

EXAMPLES
    python3 convert_song.py VisiPiano-10.json VisiPiano10_Notes VisiPiano10_song.h
    python3 convert_song.py songs/json/ songs/headers/
"""

import json
import re
import sys
from pathlib import Path

ONSET_BUCKET_MS = 15
MERGE_GAP_MS    = 25
MIN_DURATION_MS = 20   # never emit a note too short to hear


def load_notes(json_path: str, track_index):
    with open(json_path) as f:
        data = json.load(f)
    tracks = data["tracks"]
    if track_index is None:
        track_index = max(range(len(tracks)), key=lambda i: len(tracks[i].get("notes", [])))
        print(f"  auto-picked track {track_index}: {len(tracks[track_index].get('notes', []))} notes")
    notes = tracks[track_index]["notes"]
    if not notes:
        raise ValueError(f"track {track_index} has no notes")
    return notes


def extract_melody(raw_notes):
    """Polyphonic -> monophonic: highest note per onset bucket."""
    flat = sorted((round(n["time"]*1000), n["midi"], round(n["duration"]*1000)) for n in raw_notes)
    buckets = []
    for onset_ms, midi, dur_ms in flat:
        if buckets and onset_ms - buckets[-1][0] <= ONSET_BUCKET_MS:
            if midi > buckets[-1][1]:
                buckets[-1][1] = midi; buckets[-1][2] = dur_ms
        else:
            buckets.append([onset_ms, midi, dur_ms])
    merged = []
    for onset_ms, midi, dur_ms in buckets:
        if merged and onset_ms - merged[-1][0] < MERGE_GAP_MS:
            continue
        merged.append([onset_ms, midi, dur_ms])
    for i in range(len(merged)):
        onset_ms, midi, dur_ms = merged[i]
        if i + 1 < len(merged):
            gap = merged[i+1][0] - onset_ms
            dur_ms = min(dur_ms, gap) if gap > 0 else dur_ms
        merged[i][2] = max(MIN_DURATION_MS, dur_ms)
    return [(m[1], m[2], m[0]) for m in merged]


def convert(raw_notes):
    """Direct 1:1 conversion — all notes, no filtering. Returns [(midi, dur_ms, time_ms)]."""
    return [
        (n["midi"], max(MIN_DURATION_MS, round(n["duration"] * 1000)), round(n["time"] * 1000))
        for n in raw_notes
    ]


def cpp_identifier(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def write_header(notes, array_name: str, output_path: str, source_name: str):
    guard = re.sub(r"[^A-Za-z0-9]", "_", Path(output_path).stem).upper() + "_H"
    count_name = re.sub(r"_?[Nn]otes$", "", array_name) + "_notes_count"
    total_ms = notes[-1][2] + notes[-1][1] if notes else 0

    lines = [
        f"// Auto-generated from {source_name} by convert_song.py — do not edit by hand.",
        f"#ifndef {guard}",
        f"#define {guard}",
        '#include "Buzzer/SongsPlayer/SongPlayer.h"',
        "",
        f"// {len(notes)} notes, total duration ~{total_ms / 1000:.1f}s",
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
    print(f"  -> {output_path}  ({len(notes)} notes, ~{total_ms / 1000:.1f}s)")


def process_file(input_json: str, array_name: str, output_h: str, track_index=None, raw_mode=False):
    print(f"reading {input_json} ...")
    raw = load_notes(input_json, track_index)
    if raw_mode:
        print("  mode: 1:1 (no filtering)")
        notes = convert(raw)
    else:
        print("  mode: melody extraction")
        notes = extract_melody(raw)
    write_header(notes, array_name, output_h, Path(input_json).name)


def process_folder(input_dir: str, output_dir: str, raw_mode=False):
    in_path = Path(input_dir)
    out_path = Path(output_dir)
    out_path.mkdir(parents=True, exist_ok=True)
    json_files = sorted(in_path.glob("*.json"))
    if not json_files:
        print(f"No *.json files found in {input_dir}")
        return
    print(f"Found {len(json_files)} JSON files in {input_dir}")
    for jf in json_files:
        array_name = cpp_identifier(jf.stem) + "_Notes"
        output_h = out_path / (jf.stem + "_song.h")
        try:
            process_file(str(jf), array_name, str(output_h), raw_mode=raw_mode)
        except Exception as e:
            print(f"  ERROR: {e}")


def main():
    args = sys.argv[1:]
    track_index = None
    if "--track" in args:
        i = args.index("--track")
        track_index = int(args[i + 1])
        del args[i:i + 2]

    raw_mode = "--raw" in args
    if raw_mode:
        args.remove("--raw")

    # folder mode: 2 args, both are directories (or second doesn't exist yet)
    if len(args) == 2 and (Path(args[0]).is_dir() or not args[1].endswith(".h")):
        process_folder(args[0], args[1], raw_mode)
    elif len(args) == 3:
        array_name = cpp_identifier(args[1])
        process_file(args[0], array_name, args[2], track_index, raw_mode)
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
