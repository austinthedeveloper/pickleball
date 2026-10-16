"""Court colour presets (ADR-022): validated tables in court_colors.h."""
import json

from .common import AssetError, paths, read_png, rgb8_to_15

GROUND_PALETTE = 2              # BG1 palette 2: the court and its surround
PARTS = ("surface", "kitchen", "surround", "lines")
SAME = "SAME"                   # kitchen preset 1 follows the surface
MAX_ENTRIES = 5


def luminance(color):
    """WCAG relative luminance of an 8-bit RGB colour."""
    def channel(c):
        c /= 255
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (channel(c) for c in color)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = sorted((luminance(a), luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def brightness(color):
    r, g, b = color
    return 0.299 * r + 0.587 * g + 0.114 * b


def derive(base, painted_base, painted_entry):
    """The entry for a new base: the base scaled by the painted entry's
    brightness relative to the painted base (keeps the base's hue)."""
    scale = brightness(painted_entry) / max(1, brightness(painted_base))
    return tuple(min(255, round(c * scale)) for c in base)


def time_table(scale):
    """A 5-bit channel's table for a TIME scale (ADR-045)."""
    return [min(31, round(c * scale)) for c in range(32)]


def five_to_eight(c):
    return (c << 3) | (c >> 2)


def apply_time(color, scales):
    """An 8-bit RGB colour as the game shows it at a TIME: to 5 bits, each
    channel through its table, and back (the contrast check's view)."""
    r, g, b = (round(c * 31 / 255) for c in color)
    if scales is None:
        return tuple(five_to_eight(c) for c in (r, g, b))
    tables = [time_table(k) for k in scales]
    return tuple(five_to_eight(t[c]) for t, c in zip(tables, (r, g, b)))


def load_times(spec):
    """TIME choices (ADR-045): names, the lit ground entries, tables."""
    info = spec["times"]
    lit = info["lit_entries"]
    if any(not 1 <= e <= 15 for e in lit):
        raise AssetError("court times: lit entries are 1..15")
    times = info["list"]
    if not times or times[0]["name"] != "DAY" or "dark" in times[0]:
        raise AssetError("court times: the first is DAY, as painted")
    venues = json.loads((paths.src / "venues.json").read_text("utf-8"))
    sounds = [v["ambience"] for v in venues["venues"]]
    for t in times:
        if len(t["name"]) > 8:
            raise AssetError(f"court time {t['name']}: names fit 8 cells")
        for kind in ("dark", "lit"):
            if (kind in t) != ("dark" in t) or (
                    kind in t and (len(t[kind]) != 3 or
                                   not all(0 < k <= 1 for k in t[kind]))):
                raise AssetError(f"court time {t['name']}: {kind} is three "
                                 "scales in (0, 1]")
        if "ambience" in t and t["ambience"] not in sounds:
            raise AssetError(f"court time {t['name']}: no venue has the "
                             f"{t['ambience']} ambience")
    return lit, times, sounds


def load_court_colors():
    spec = json.loads((paths.src / "court_colors.json").read_text("utf-8"))
    _, _, _, palette = read_png(paths.src / "court_scene.png")
    first = GROUND_PALETTE * 16

    def painted(entry):
        return tuple(palette[first + entry])

    parts = []
    for part in PARTS:
        info = spec["parts"][part]
        entries = info["entries"]
        if not 1 <= len(entries) <= MAX_ENTRIES:
            raise AssetError(f"court colours {part}: 1..{MAX_ENTRIES} entries")
        if any(not 1 <= e <= 15 for e in entries):
            raise AssetError(f"court colours {part}: entries are 1..15")
        presets = [("DEFAULT", [painted(e) for e in entries])]
        for name, base in info["presets"]:
            if len(name) > 8:
                raise AssetError(f"court colour {name}: names fit 8 cells")
            if base is None:
                if part != "kitchen" or name != SAME or len(presets) != 1:
                    raise AssetError(f"court colours {part}: only the "
                                     f"kitchen's first preset ({SAME}) has "
                                     "no colour")
                # Resolved at runtime; the painted colours stand in.
                presets.append((name, [painted(e) for e in entries]))
                continue
            presets.append((name, [derive(tuple(base), painted(entries[0]),
                                          painted(e)) for e in entries]))
        parts.append((part, entries, presets))
    used = [e for _, entries, _ in parts for e in entries]
    if len(used) != len(set(used)):
        raise AssetError("court colours: an entry belongs to two parts")
    # SAME maps surface preset k to kitchen preset k + 1 (ADR-027).
    surface_names = [n for n, _ in parts[0][2]][1:]
    kitchen_names = [n for n, _ in parts[1][2]][2:2 + len(surface_names)]
    if kitchen_names != surface_names:
        raise AssetError("court colours: kitchen presets after SAME must "
                         "mirror the surface presets in order")

    # Readability (ADR-022): every line colour on every surface, and the
    # ball on every surface.
    # The kitchen is court surface too: check both parts.
    surfaces = parts[0][2] + parts[1][2][2:]
    # The LAVA kitchen (ADR-033, the KITCHEN: LAVA modifier) is checked too.
    kitchen = parts[1][1]
    lava = [derive(tuple(spec["lava"]), painted(kitchen[0]), painted(e))
            for e in kitchen]
    surfaces = surfaces + [("LAVA", lava)]
    lines = parts[3][2]
    ball = tuple(spec["ball_color"])
    # ...at every TIME (ADR-045): the surface and lines under the lights;
    # the ball (an object) keeps its colour.
    lit, times, _ = load_times(spec)
    for entry in (parts[0][1][0], parts[1][1][0], parts[3][1][0]):
        if entry not in lit:
            raise AssetError(f"court times: entry {entry} (a surface or "
                             "line base) must be lit")
    for t in times:
        scales = t.get("lit")
        when = f" at {t['name']}" if scales else ""
        for sname, scolors in surfaces:
            surface = apply_time(scolors[0], scales)
            for lname, lcolors in lines:
                ratio = contrast(surface, apply_time(lcolors[0], scales))
                if ratio < spec["min_line_contrast"]:
                    raise AssetError(f"court colours: {lname} lines on "
                                     f"{sname}{when} contrast {ratio:.2f} < "
                                     f"{spec['min_line_contrast']}")
            ratio = contrast(surface, ball)
            if ratio < spec["min_ball_contrast"]:
                raise AssetError(f"court colours: ball on {sname}{when} "
                                 f"contrast {ratio:.2f} < "
                                 f"{spec['min_ball_contrast']}")
    return parts, lava


def court_time_lines():
    """C tables for the TIME choices (ADR-045)."""
    spec = json.loads((paths.src / "court_colors.json").read_text("utf-8"))
    lit, times, sounds = load_times(spec)
    mask = sum(1 << e for e in lit)
    out = ["/* TIME (ADR-045): DAY as painted; each later time's channel",
           " * tables, [time - 1][0 dark, 1 lit][5-bit value], red as is,",
           " * green shifted by 5, blue by 10. */",
           "const char *const g_court_time_names[COURT_TIMES] = { " +
           ", ".join(f'"{t["name"]}"' for t in times) + " };",
           "/* Ground (palette 2) entries lit at night: bit n is entry n. */",
           f"const u16 g_court_lit_mask = 0x{mask:04x};"]
    for channel, shift in (("r", 0), ("g", 5), ("b", 10)):
        rows = []
        for t in times[1:]:
            for kind in ("dark", "lit"):
                k = t[kind][("r", "g", "b").index(channel)]
                rows.append("    { " + ", ".join(
                    f"0x{v << shift:04x}" for v in time_table(k)) + " },")
        out.append(f"const u16 g_court_time_{channel}"
                   f"[(COURT_TIMES - 1) * 2][32] = {{")
        out += rows
        out.append("};")
    out.append("/* The ambience module a time plays instead of the venue's "
               "(0xff: the venue's). */")
    out.append("const u8 g_court_time_ambience[COURT_TIMES] = { " + ", ".join(
        str(sounds.index(t["ambience"])) if "ambience" in t else "0xff"
        for t in times) + " };")
    return len(times), out


def court_colors_header():
    parts, lava = load_court_colors()
    time_count, time_lines = court_time_lines()
    total = sum(len(presets) for _, _, presets in parts)
    lines = ["/* Generated by tools/generate_assets.py from "
             "assets/source/court_colors.json; do not edit. */",
             "#ifndef COURT_COLORS_H", "#define COURT_COLORS_H", "",
             "#include \"game_types.h\"", "",
             "/* Parts of BG1 palette 2 (ADR-022). Preset 0 of each part is "
             "DEFAULT. */"]
    for i, (part, _, presets) in enumerate(parts):
        lines.append(f"#define COURT_PART_{part.upper()} {i}")
    lines.append(f"#define COURT_PARTS {len(parts)}")
    lines.append("/* Kitchen preset that follows the surface (ADR-027). */")
    lines.append("#define COURT_KITCHEN_SAME 1")
    lines_presets = [name for part, _, presets in parts if part == "lines"
                     for name, _ in presets]
    if "LIME" not in lines_presets:
        raise AssetError("court_colors.json: GLOW (ADR-054) needs a LIME "
                         "lines preset")
    lines.append("/* Lines preset GLOW plays on (ADR-054). */")
    lines.append(f"#define COURT_LINES_LIME {lines_presets.index('LIME')}")
    lines.append(f"#define COURT_PART_MAX_ENTRIES {MAX_ENTRIES}")
    lines.append(f"#define COURT_PRESETS {total}")
    lines.append(f"#define COURT_TIMES {time_count}")
    lines.append("#define COURT_TIME_DAY 0")
    lines += ["", "#ifdef COURT_COLORS_DEFINE"]
    first = 0
    firsts, counts, sizes, entries, names, colors = [], [], [], [], [], []
    for part, part_entries, presets in parts:
        firsts.append(first)
        counts.append(len(presets))
        sizes.append(len(part_entries))
        entries.append(part_entries + [0] * (MAX_ENTRIES - len(part_entries)))
        for name, part_colors in presets:
            names.append(name)
            words = [rgb8_to_15(c) for c in part_colors]
            colors.append(words + [0] * (MAX_ENTRIES - len(words)))
        first += len(presets)
    lines.append("/* First preset of each part, and how many it has. */")
    lines.append("const u8 g_court_part_first[COURT_PARTS] = { " +
                 ", ".join(map(str, firsts)) + " };")
    lines.append("const u8 g_court_part_count[COURT_PARTS] = { " +
                 ", ".join(map(str, counts)) + " };")
    lines.append("/* Palette 2 entries each part recolours. */")
    lines.append("const u8 g_court_part_size[COURT_PARTS] = { " +
                 ", ".join(map(str, sizes)) + " };")
    lines.append("const u8 g_court_part_entries[COURT_PARTS]"
                 "[COURT_PART_MAX_ENTRIES] = { " + ", ".join(
                     "{ " + ", ".join(map(str, e)) + " }" for e in entries) +
                 " };")
    lines.append("const char *const g_court_preset_names[COURT_PRESETS] = { " +
                 ", ".join(f'"{n}"' for n in names) + " };")
    lines.append("/* BGR15 colours, in the part's entry order. */")
    lines.append("const u16 g_court_preset_colors[COURT_PRESETS]"
                 "[COURT_PART_MAX_ENTRIES] = {")
    lines += ["    { " + ", ".join(f"0x{w:04x}" for w in c) + " }," for c in colors]
    lines.append("};")
    lines.append("/* The LAVA kitchen (ADR-033), in the kitchen's entry order. */")
    words = [rgb8_to_15(c) for c in lava]
    lines.append("const u16 g_court_lava_colors[COURT_PART_MAX_ENTRIES] = { " +
                 ", ".join(f"0x{w:04x}" for w in
                           words + [0] * (MAX_ENTRIES - len(words))) + " };")
    lines += time_lines
    lines += ["#else",
              "extern const u8 g_court_part_first[COURT_PARTS];",
              "extern const u8 g_court_part_count[COURT_PARTS];",
              "extern const u8 g_court_part_size[COURT_PARTS];",
              "extern const u8 g_court_part_entries[COURT_PARTS]"
              "[COURT_PART_MAX_ENTRIES];",
              "extern const char *const g_court_preset_names[COURT_PRESETS];",
              "extern const u16 g_court_preset_colors[COURT_PRESETS]"
              "[COURT_PART_MAX_ENTRIES];",
              "extern const u16 g_court_lava_colors[COURT_PART_MAX_ENTRIES];",
              "extern const char *const g_court_time_names[COURT_TIMES];",
              "extern const u16 g_court_lit_mask;",
              "extern const u16 g_court_time_r[(COURT_TIMES - 1) * 2][32];",
              "extern const u16 g_court_time_g[(COURT_TIMES - 1) * 2][32];",
              "extern const u16 g_court_time_b[(COURT_TIMES - 1) * 2][32];",
              "extern const u8 g_court_time_ambience[COURT_TIMES];",
              "#endif", "", "#endif", ""]
    return "\n".join(lines)
