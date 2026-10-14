"""CPU personalities (ADR-023): validated tables in opponents.h."""
import json
import re

from .common import ROOT, AssetError, bgr15, paths

STYLE_KEYS = ("late_period", "reaction", "dink_period", "lob_distance",
              "putaway_z", "putaway_distance", "aim", "depth", "recover",
              "speed", "volley", "net")
AIMS = ("away", "body", "mix")
DEPTHS = ("alternate", "deep")
# Inclusive ranges for the numeric knobs.
RANGES = {
    "late_period": (0, 30),      # 0 never deliberately late
    "reaction": (0, 20),
    "dink_period": (0, 8),       # 0 never dinks
    "lob_distance": (0, 120),    # 0 never lobs
    "putaway_z": (0, 40),
    "putaway_distance": (0, 128),
    "recover": (40, 100),        # home distance behind the net
    "speed": (12, 24),           # <= PLAYER_MAX_SPEED_FP
    "volley": (0, 1),            # ADR-035: volleys once the bounces are done
    "net": (0, 100),             # ADR-035: home distance once in; 0 stays back
}
VARIANTS = ("male", "female")
NAME_CELLS = 9                   # "VS " + name in the 12-cell centre panel
LINE_CELLS = 28                  # a story card line in one HUD panel
LINE_KEYS = ("intro", "taunt", "concede")
# The HUD font draws ASCII 32-95; '@' is the B button glyph in prompts.
LINE_CHARS = re.compile(r"[A-Z0-9 .,!?'\-:]+")


def header_constant(path, name):
    text = (ROOT / path).read_text("utf-8")
    match = re.search(rf"\b{name} = (\d+)", text)
    if not match:
        raise AssetError(f"{path}: {name} not found")
    return int(match.group(1))


def check_default_style(style):
    """The default style is the shipped CPU; it must match the constants."""
    net = header_constant("include/court.h", "COURT_NET_X")
    expected = {
        "late_period": header_constant("include/ai.h", "AI_LATE_SHOT_PERIOD"),
        "reaction": 0,
        "dink_period": 4,
        "lob_distance": header_constant("include/ai.h", "AI_LOB_NET_DISTANCE"),
        "putaway_z": header_constant("include/ai.h", "AI_PUTAWAY_Z"),
        "putaway_distance": header_constant("include/ai.h",
                                            "AI_PUTAWAY_NET_DISTANCE"),
        "aim": "away",
        "depth": "alternate",
        "recover": header_constant("include/ai.h", "AI_RECOVER_X") - net,
        "speed": header_constant("include/player.h", "PLAYER_CPU_MAX_SPEED_FP"),
        "volley": 0,
        "net": 0,
    }
    for key, value in expected.items():
        if style.get(key) != value:
            raise AssetError(f"opponents.json default_style {key} "
                             f"{style.get(key)} != shipped CPU {value}")


def check_style(where, style):
    if set(style) != set(STYLE_KEYS):
        raise AssetError(f"{where}: style needs exactly {', '.join(STYLE_KEYS)}")
    for key, (low, high) in RANGES.items():
        if not low <= style[key] <= high:
            raise AssetError(f"{where}: {key} {style[key]} outside {low}..{high}")
    # ADR-035: coming in stops behind the LAVA line (kitchen + 8, ADR-033).
    if style["net"] and style["net"] < 44:
        raise AssetError(f"{where}: net {style['net']} is inside 44 (the "
                         "kitchen line and the LAVA margin)")
    if style["aim"] not in AIMS:
        raise AssetError(f"{where}: aim is one of {', '.join(AIMS)}")
    if style["depth"] not in DEPTHS:
        raise AssetError(f"{where}: depth is one of {', '.join(DEPTHS)}")


def color5(where, value):
    if len(value) != 3 or any(not 0 <= c <= 31 for c in value):
        raise AssetError(f"{where}: colours are 5-bit [r, g, b]")
    return bgr15(*value)


def compose(where, player, skins, shared, paddle_parts):
    """16 colours in the contract v2 roles (ADR-013)."""
    skin = skins.get(player["skin"]) if isinstance(player["skin"], str) \
        else player["skin"]
    if skin is None or len(skin) != 3:
        raise AssetError(f"{where}: skin is a 'skins' name or three colours")
    if len(player["shirt"]) != 3 or len(player["shorts"]) != 2:
        raise AssetError(f"{where}: shirt has 3 colours, shorts 2")
    paddle = paddle_parts[player["paddle"]]
    out = [0, color5(where, shared["outline"])]
    out += [color5(where, c) for c in skin]
    out += [color5(where, c) for c in player["shirt"]]
    out += [color5(where, c) for c in player["shorts"]]
    out.append(color5(where, player["shoe"]))
    out += [color5(where, c) for c in paddle["face"]]
    out.append(color5(where, paddle["edge"]))
    out.append(color5(where, shared["highlight"]))
    out.append(color5(where, player["hair"]))
    assert len(out) == 16
    return out


def style_row(style):
    values = [style[k] for k in STYLE_KEYS]
    values[STYLE_KEYS.index("aim")] = AIMS.index(style["aim"])
    values[STYLE_KEYS.index("depth")] = DEPTHS.index(style["depth"])
    return "{ " + ", ".join(map(str, values)) + " }"


def opponents_header():
    spec = json.loads((paths.src / "opponents.json").read_text("utf-8"))
    paddles = json.loads((paths.src / "paddles.json").read_text("utf-8"))
    paddle_names = [p["name"] for p in paddles["paddles"]]
    paddle_parts = {name: json.loads((paths.palettes / f"paddle_{name}.json")
                                     .read_text("utf-8"))
                    for name in paddle_names}
    shared = json.loads((paths.palettes / "shared.json").read_text("utf-8"))
    venue_names = [v["name"] for v in json.loads(
        (paths.src / "venues.json").read_text("utf-8"))["venues"]]
    check_style("default_style", spec["default_style"])
    check_default_style(spec["default_style"])
    check_style("partner_style", spec["partner_style"])

    opponents = spec["opponents"]
    if not 1 <= len(opponents) <= 9:
        raise AssetError("opponents.json: 1..9 opponents (the records page "
                         "shows the rung as one digit)")
    styles, palettes, variants, paddle_ids, huds, teams, captains, homes = \
        [], [], [], [], [], [], [], []
    story = []
    for opponent in opponents:
        team = opponent["team"]
        if not 1 <= len(team) <= NAME_CELLS or not re.fullmatch(r"[A-Z0-9 ']+", team):
            raise AssetError(f"opponent {team!r}: 1..{NAME_CELLS} capitals")
        if len(opponent["players"]) != 2:
            raise AssetError(f"opponent {team}: a captain and a partner")
        if len(opponent["hud"]) != 2:
            raise AssetError(f"opponent {team}: two HUD label colours")
        huds.append([color5(team, c) for c in opponent["hud"]])
        if opponent.get("venue") not in venue_names:
            raise AssetError(f"opponent {team}: venue is one of "
                             f"{', '.join(venue_names)}")
        homes.append(venue_names.index(opponent["venue"]))
        for key in LINE_KEYS:
            line = opponent.get(key, "")
            if not 1 <= len(line) <= LINE_CELLS or \
                    not LINE_CHARS.fullmatch(line):
                raise AssetError(f"opponent {team}: {key} is 1..{LINE_CELLS} "
                                 "capitals, digits, spaces, and .,!?'-:")
        story.append([opponent[k] for k in LINE_KEYS])
        teams.append(team)
        row_styles, row_pals, row_vars, row_paddles = [], [], [], []
        for player in opponent["players"]:
            where = f"opponent {team} / {player['name']}"
            if not 1 <= len(player["name"]) <= NAME_CELLS:
                raise AssetError(f"{where}: name is 1..{NAME_CELLS} cells")
            check_style(where, player["style"])
            if player["variant"] not in VARIANTS:
                raise AssetError(f"{where}: variant is male or female")
            if player["paddle"] not in paddle_names:
                raise AssetError(f"{where}: paddle is one of "
                                 f"{', '.join(paddle_names)}")
            row_styles.append(style_row(player["style"]))
            row_pals.append(compose(where, player, spec["skins"], shared,
                                    paddle_parts))
            row_vars.append(VARIANTS.index(player["variant"]))
            row_paddles.append(paddle_names.index(player["paddle"]))
        captains.append(opponent["players"][0]["name"])
        styles.append(row_styles)
        palettes.append(row_pals)
        variants.append(row_vars)
        paddle_ids.append(row_paddles)

    lines = ["/* Generated by tools/generate_assets.py from "
             "assets/source/opponents.json; do not edit. */",
             "#ifndef OPPONENTS_H", "#define OPPONENTS_H", "",
             "#include \"game_types.h\"", "",
             f"#define OPPONENT_COUNT {len(opponents)}",
             "#define OPPONENT_NONE 0xff", "",
             "/* AiStyle.aim and .depth values. */",
             "enum { AI_AIM_AWAY = 0, AI_AIM_BODY = 1, AI_AIM_MIX = 2 };",
             "enum { AI_DEPTH_ALTERNATE = 0, AI_DEPTH_DEEP = 1 };",
             "/* g_opponent_lines columns. */",
             "enum { OPPONENT_LINE_INTRO = 0, OPPONENT_LINE_TAUNT = 1, "
             "OPPONENT_LINE_CONCEDE = 2 };", "",
             "/* A CPU's play style (ADR-023); the default is the shipped CPU. */",
             "typedef struct {",
             "    u8 late_period;       /* deliberately late every Nth return; 0 never */",
             "    u8 reaction;          /* frames an incoming ball is ignored */",
             "    u8 dink_period;       /* a dink every Nth shot; 0 never */",
             "    u8 lob_distance;      /* lob when an opponent is this near the net; 0 never */",
             "    u8 putaway_z;         /* drive a ball at least this high... */",
             "    u8 putaway_distance;  /* ...within this distance of the net */",
             "    u8 aim;               /* AI_AIM_* (singles) */",
             "    u8 depth;             /* AI_DEPTH_* for standard shots */",
             "    u8 recover;           /* home x distance behind the net */",
             "    u8 speed;             /* max speed, 12.4 px/frame units */",
             "    u8 volley;            /* ADR-035: volleys once the bounces are done */",
             "    u8 net;               /* ADR-035: home distance once in; 0 stays back */",
             "} AiStyle;", "",
             "#ifdef OPPONENTS_DEFINE",
             f"const AiStyle g_ai_default_style = {style_row(spec['default_style'])};",
             "/* ADR-035: a human's CPU partner (the default look, smarter). */",
             f"const AiStyle g_ai_partner_style = {style_row(spec['partner_style'])};",
             "/* [opponent][captain, partner] */",
             "const AiStyle g_opponent_styles[OPPONENT_COUNT][2] = {"]
    lines += ["    { " + ", ".join(row) + " }," for row in styles]
    lines.append("};")
    lines.append("/* Full OBJ palettes (ADR-013 roles), paddle part included. */")
    lines.append("const u16 g_opponent_palettes[OPPONENT_COUNT][2][16] = {")
    for row in palettes:
        lines.append("    {")
        for pal in row:
            lines.append("        { " + ", ".join(f"0x{w:04x}" for w in pal) + " },")
        lines.append("    },")
    lines.append("};")
    lines.append("const u8 g_opponent_variant[OPPONENT_COUNT][2] = { " +
                 ", ".join("{ %d, %d }" % tuple(v) for v in variants) + " };")
    lines.append("const u8 g_opponent_paddle[OPPONENT_COUNT][2] = { " +
                 ", ".join("{ %d, %d }" % tuple(p) for p in paddle_ids) + " };")
    lines.append("/* BG3 label colours 1-2 for the opponents' team. */")
    lines.append("const u16 g_opponent_hud[OPPONENT_COUNT][2] = { " +
                 ", ".join("{ 0x%04x, 0x%04x }" % tuple(h) for h in huds) + " };")
    lines.append("const char *const g_opponent_team[OPPONENT_COUNT] = { " +
                 ", ".join(f'"{t}"' for t in teams) + " };")
    lines.append("const char *const g_opponent_captain[OPPONENT_COUNT] = { " +
                 ", ".join(f'"{c}"' for c in captains) + " };")
    lines.append("/* Story card lines (ADR-026): intro, taunt (they won), "
                 "concede (you won). */")
    lines.append("const char *const g_opponent_lines[OPPONENT_COUNT][3] = {")
    lines += ["    { " + ", ".join(f'"{t}"' for t in row) + " }," for row in story]
    lines.append("};")
    lines.append("/* Home court (venue index, ADR-024). */")
    lines.append("const u8 g_opponent_venue[OPPONENT_COUNT] = { " +
                 ", ".join(map(str, homes)) + " };")
    lines += ["#else",
              "extern const AiStyle g_ai_default_style;",
              "extern const AiStyle g_ai_partner_style;",
              "extern const AiStyle g_opponent_styles[OPPONENT_COUNT][2];",
              "extern const u16 g_opponent_palettes[OPPONENT_COUNT][2][16];",
              "extern const u8 g_opponent_variant[OPPONENT_COUNT][2];",
              "extern const u8 g_opponent_paddle[OPPONENT_COUNT][2];",
              "extern const u16 g_opponent_hud[OPPONENT_COUNT][2];",
              "extern const char *const g_opponent_team[OPPONENT_COUNT];",
              "extern const char *const g_opponent_captain[OPPONENT_COUNT];",
              "extern const u8 g_opponent_venue[OPPONENT_COUNT];",
              "extern const char *const g_opponent_lines[OPPONENT_COUNT][3];",
              "#endif", "", "#endif", ""]
    return "\n".join(lines)
