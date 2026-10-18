#!/usr/bin/env python3
"""The gameplay tuning as one platform-neutral file (ADR-068).

docs/tuning/tuning.json lists every simulation constant (each enum member
and object-like #define in the headers below, with its comment as a note)
and the tables the game builds from them, with the values the game
compiles. Ports read it instead of the C; its git history is the tuning
changelog; TUNING_VERSION (include/tuning.h) says when values changed.

    make tuning-export   # rewrite docs/tuning/tuning.json
    make tuning-check    # part of make check: fail if it is stale

Steps (the Makefile runs them):

    tuning_export.py --inc build/host/tuning_constants.inc
        writes the X-macro list tools/tuning_dump.c prints
    tuning_export.py --write RAW | --check RAW
        RAW is tuning_dump's output; merges the notes and writes or checks

--write refuses to change a value without a TUNING_VERSION bump, and both
want a row for the current version in docs/TUNING.md's versions table.
"""
import argparse
import hashlib
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "tuning" / "tuning.json"
DOC = ROOT / "docs" / "TUNING.md"

# The simulation headers, in reading order, with what each holds.
SOURCES = [
    ("include/court.h", "Court geometry and player bounds, in ground pixels"),
    ("include/ball.h", "Ball flight and bounce"),
    ("include/collision.h", "Hit box, shot targets, paces, flight times"),
    ("include/player.h", "Movement, swing timing, fixed point"),
    ("include/serve.h", "Serve placement, toss, charge, duds"),
    ("include/game.h", "Match phases, faults, timers"),
    ("include/ai.h", "CPU defaults (the default AiStyle is in the tables)"),
    ("src/ai.c", "CPU positioning, private to ai.c"),
    ("include/options.h", "Difficulty, points, ball cycle"),
    ("include/modifiers.h", "Modifiers"),
    ("include/match.h", "Match setup"),
    ("include/practice.h", "Practice mode"),
    ("include/replay.h", "Close-call replay"),
    ("assets/generated/paddle_stats.h", "Paddle profiles (from paddles.json)"),
    ("assets/generated/opponents.h", "Opponents (from opponents.json)"),
]

# Names that are layout, include guards, or menu plumbing, not tuning.
EXCLUDE = re.compile(
    r"(_H$|_ROWS?(_|$)|_EDIT_|_COLS?$|^REPLAY_LINE_|^OPPONENT_LINE_|"
    r"^QUIPS_|^OPPONENT_QUIPS$|^PLAYER_SPRITE_|^COURT_PROJ_SCALE_|"
    r"^PAL_SLOT_)")

# Each table tools/tuning_dump.c prints, and what it is.
TABLE_NOTES = {
    "player_diagonal_limit": "Per-axis speed limit on a diagonal for each "
        "straight limit 0-31 (12.4): limit * 181 / 256, as a table.",
    "ai_ice_coast": "ICE: px a player moving at v (12.4, 0-31) coasts to a "
        "stop, v (v - 1) / 32.",
    "options_points": "The POINTS choices.",
    "ball_pace_KID": "Ball steps on each frame from a 60-frame cycle's start "
        "(GAME_BALL_PACE_TICKS entries; the 12 past the cycle repeat its "
        "start). 0 = the ball skips the frame, 2 = it steps twice. This is "
        "how difficulty and FAST BALL change ball speed: the step pattern, "
        "not the velocity.",
    "ball_pace_KID_fast_ball": "As ball_pace_KID, with FAST BALL.",
    "ball_pace_EASY": "As ball_pace_KID, on EASY.",
    "ball_pace_EASY_fast_ball": "As ball_pace_EASY, with FAST BALL.",
    "ball_pace_MEDIUM": "As ball_pace_KID, on MEDIUM (one step a frame).",
    "ball_pace_MEDIUM_fast_ball": "As ball_pace_MEDIUM, with FAST BALL.",
    "ball_pace_HARD": "As ball_pace_KID, on HARD.",
    "ball_pace_HARD_fast_ball": "As ball_pace_HARD, with FAST BALL.",
    "popcorn_steps": "POPCORN: the extra steps each level (hit 1, 2, ...) "
        "adds to the pace table, per tick.",
    "paddle_stats": "Bars [power, control, speed] per paddle profile; a "
        "profile is hat * PADDLE_COUNT + paddle (hat 0 is the plain paddle).",
    "paddle_power_mul": "Shot pace x n / 64, per profile.",
    "paddle_control_mul": "Stretch spray x n / 64, per profile.",
    "paddle_swing_limit": "Movement limit while swinging (12.4), per profile.",
    "paddle_recovery": "Recovery frames after a swing, per profile.",
    "ai_default_style": "The default CPU (opponents.json default_style).",
    "ai_partner_style": "A human's CPU partner in doubles.",
    "opponent_styles": "[captain, partner] per ladder rung, as authored "
        "(singles plays only the captain).",
    "opponent_styles_KID": "opponent_styles after KID's adjustment "
        "(options_apply_difficulty).",
    "opponent_styles_EASY": "opponent_styles after EASY's adjustment.",
    "opponent_styles_MEDIUM": "opponent_styles on MEDIUM (unchanged).",
    "opponent_styles_HARD": "opponent_styles after HARD's adjustment.",
    "opponent_paddle": "[captain, partner] paddle per rung.",
    "opponent_hat": "[captain, partner] hat per rung (hats change the "
        "paddle profile).",
    "practice_zone_rects": "Practice target zones [x0, y0, x1, y1].",
}

UNITS = {
    "frame": "One simulation step. 60 a second (NTSC); the game has no "
        "PAL timing.",
    "fixed_point": "12.4. A name ending _FP is in 1/16 px: a position, a "
        "speed in 1/16 px per frame, or gravity in 1/16 px per frame "
        "squared. FP_SHIFT is 4.",
    "ground": "x runs along the court (baseline 16, net 128, baseline "
        "240); y across it (sidelines 90 and 170). Both are ground pixels "
        "of the flat court the rules, physics and AI use; the 3/4 view is "
        "a display projection (ADR-011).",
    "scale": "x: 224 px for 44 ft, 5.09 px/ft. y: 80 px for 20 ft, 4 px/ft. "
        "The court is squashed across, so a y speed covers about 27% more "
        "feet than the same x speed. Keep these units in a port; don't "
        "rescale to a true court.",
    "frames": "A name ending _FRAMES, and AiStyle.reaction, count frames.",
    "fractions": "_256 is n/256; paddle multipliers are n/64.",
}

DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)(\(?)(.*)$")
IDENT = re.compile(r"[A-Za-z_]\w*")


def clean_comment(text):
    text = re.sub(r"^/\*+|\*+/$", "", text.strip())
    lines = [re.sub(r"^\s*\*\s?", "", l).strip() for l in text.splitlines()]
    return " ".join(l for l in lines if l)


def split_enum(body):
    """(name, notes) per member of an enum body, comments kept."""
    items = []          # [name, [notes]]
    chunk_code = []
    chunk_notes = []    # (comment, attaches_to_previous)
    seen_newline = False
    depth = 0
    i = 0

    def finish():
        code = "".join(chunk_code)
        m = IDENT.search(code)
        for note, to_previous in chunk_notes:
            if (to_previous or not m) and items:
                items[-1][1].append(note)
        if m:
            items.append([m.group(0),
                          [n for n, p in chunk_notes if not p]])

    while i < len(body):
        if body.startswith("/*", i):
            end = body.index("*/", i) + 2
            has_code = "".join(chunk_code).strip() != ""
            chunk_notes.append((clean_comment(body[i:end]),
                                not has_code and not seen_newline
                                and bool(items)))
            i = end
            continue
        c = body[i]
        if c == "\n":
            seen_newline = True
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        if c == "," and depth == 0:
            finish()
            chunk_code, chunk_notes, seen_newline = [], [], False
        else:
            chunk_code.append(c)
        i += 1
    finish()
    return [(name, " ".join(n for n in notes if n)) for name, notes in items]


def parse(path):
    """{name: note} for a file's enum members and object-like #defines."""
    text = (ROOT / path).read_text()
    found = {}
    lines = text.replace("\\\n", " ").splitlines()
    for n, line in enumerate(lines):
        m = DEFINE.match(line)
        if not m or m.group(2) or not m.group(3).strip():
            continue
        value = m.group(3)
        note = ""
        cm = re.search(r"/\*(.*?)\*/", value)
        if cm:
            note = clean_comment(cm.group(0))
            value = value[:cm.start()]
        if '"' in value or "{" in value or not value.strip():
            continue
        if not note and n and lines[n - 1].strip().endswith("*/"):
            k = n - 1
            while k > 0 and "/*" not in lines[k]:
                k -= 1
            note = clean_comment("\n".join(lines[k:n]))
        found[m.group(1)] = (note, value.strip())
    # A .c file's enums aren't visible to tuning_dump.c; its #defines are
    # copied into the generated list.
    for m in re.finditer(r"\benum\b\s*\w*\s*\{(.*?)\}",
                         "" if path.startswith("src/") else text, re.S):
        for name, note in split_enum(m.group(1)):
            found[name] = (note, None)
    return {k: v for k, v in found.items() if not EXCLUDE.search(k)}


def write_inc(path):
    out = ["/* Generated by tools/tuning_export.py; do not edit. */"]
    for source, _ in SOURCES:
        if source.startswith("src/"):
            for name, (_, value) in parse(source).items():
                if value is not None:
                    out.append(f"#define {name} {value}    /* {source} */")
        else:
            out.append(f'#include "{pathlib.Path(source).name}"')
    out.append("#define TUNING_CONSTANTS \\")
    names = []
    for source, _ in SOURCES:
        names.extend(parse(source))
    out.extend(f"    TUNING_INT({name}) \\" for name in names)
    out.append("")
    pathlib.Path(path).write_text("\n".join(out) + "\n")


def build(raw_path):
    raw = json.loads(pathlib.Path(raw_path).read_text())
    constants = {}
    for source, title in SOURCES:
        group = {}
        for name, (note, _) in parse(source).items():
            entry = {"value": raw["constants"][name]}
            if note:
                entry["note"] = note
            group[name] = entry
        constants[source] = {"about": title, "values": group}
    missing = sorted(set(raw["tables"]) - set(TABLE_NOTES))
    if missing:
        sys.exit("tuning_export: tables without a note in TABLE_NOTES: "
                 + ", ".join(missing))
    tables = {name: {"note": TABLE_NOTES[name], "value": value}
              for name, value in raw["tables"].items()}
    sources = {}
    for name in ("opponents.json", "paddles.json", "career.json"):
        data = (ROOT / "assets" / "source" / name).read_bytes()
        sources["assets/source/" + name] = hashlib.sha256(data).hexdigest()[:16]
    return {
        "tuning_version": raw["tuning_version"],
        "generated_by": "make tuning-export (tools/tuning_export.py) from "
            "the game's headers and tables; do not edit. "
            "See docs/TUNING.md.",
        "units": UNITS,
        "constants": constants,
        "tables": tables,
        "sources": sources,
    }


def values(doc):
    """What a version bump covers: the numbers, not the notes."""
    return ({name: e["value"] for g in doc["constants"].values()
             for name, e in g["values"].items()},
            {name: t["value"] for name, t in doc["tables"].items()})


def moved(old, new):
    """Values that changed or went away. A new name for a value the code
    already had (a literal given a name) needs a re-export, not a bump."""
    out = []
    for o, n in zip(values(old), values(new)):
        out += [k for k in o if k not in n or o[k] != n[k]]
    return out


def scalar_dict(v):
    return isinstance(v, dict) and all(
        not isinstance(x, (dict, list)) for x in v.values())


def flat(v):
    return json.dumps(v, separators=(", ", ": "))


def render(v, indent=0, in_list=False):
    pad = "  " * indent
    if isinstance(v, dict) and not (in_list and scalar_dict(v)):
        items = [f'{pad}  {json.dumps(k)}: {render(x, indent + 1)}'
                 for k, x in v.items()]
        return "{\n" + ",\n".join(items) + "\n" + pad + "}"
    if isinstance(v, list) and any(isinstance(x, dict) or
            (isinstance(x, list) and any(isinstance(y, dict) for y in x))
            for x in v):
        items = [pad + "  " + render(x, indent + 1, True) for x in v]
        return "[\n" + ",\n".join(items) + "\n" + pad + "]"
    return flat(v)


def check_doc(version):
    if not re.search(rf"^\|\s*{version}\s*\|", DOC.read_text(), re.M):
        sys.exit(f"tuning_export: docs/TUNING.md has no row for tuning "
                 f"version {version} in its versions table")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inc")
    ap.add_argument("--write")
    ap.add_argument("--check")
    args = ap.parse_args()
    if args.inc:
        write_inc(args.inc)
        return
    doc = build(args.write or args.check)
    text = render(doc) + "\n"
    old = json.loads(OUT.read_text()) if OUT.exists() else None
    changed = old is not None and bool(moved(old, doc))
    same_version = old is not None and \
        old["tuning_version"] == doc["tuning_version"]
    if changed and same_version:
        sys.exit("tuning_export: tuning values changed under tuning version "
                 f"{doc['tuning_version']} ({', '.join(moved(old, doc)[:6])}): "
                 "bump TUNING_VERSION in include/tuning.h and add its row to "
                 "docs/TUNING.md")
    check_doc(doc["tuning_version"])
    if args.check:
        if not OUT.exists() or OUT.read_text() != text:
            sys.exit("tuning_export: docs/tuning/tuning.json is stale: run "
                     "make tuning-export (ADR-068)")
        print(f"tuning export: version {doc['tuning_version']}, up to date")
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text)
    n = sum(len(g["values"]) for g in doc["constants"].values())
    print(f"wrote {OUT.relative_to(ROOT)}: version {doc['tuning_version']}, "
          f"{n} constants, {len(doc['tables'])} tables")


if __name__ == "__main__":
    main()
