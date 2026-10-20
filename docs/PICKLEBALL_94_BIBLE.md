# Pickleball '94 — Canon Bible

Status: **Draft canon v0.3.6** (2026-10-19)
Owner: project lead
Change rule: no artifact (ad, photo, manual page, review, site copy) may state a fact that contradicts this file. To change canon, edit this file first, then the artifact. The same rule the court geometry follows: one source of truth.

v0.3.6: release 0.28.0 is out with story mode; everything in §5 is in a tagged release.
v0.3.5: story mode (ADR-077) is in §5.2, with its two new names, Pepper and Grimsby (§8, Q12).
v0.3.4: release 0.27.0 is out with the push; everything in §5 is in a tagged release.
v0.3.3: the push (ADR-076), on `main` after 0.26.0, is in §5.3.
v0.3.2: release 0.26.0 is out, with four players, pick your partner, the hitting poses, and the new tutorial (ADR-075); everything in §5 is in a tagged release.
v0.3.1: the hitting poses are in the game (ADR-074): §5.3 gains them, and §5.7's unused-frames legend becomes "the smash that isn't".
v0.3: §5 rewritten against the ROM on `main` (release 0.25.0 plus four players and pick-your-partner, merged after it). Everything the cartridge does is listed, with the ADR behind it. Answers added to §8 where the repo settles the question; the rest stay for the project lead.
v0.2: game facts now come from the instruction manual (A-02), which supersedes the early POC docs. The v0.1 "cut content" story was retired because the ROM already ships most of it.

---

## 0. Ground rules

1. **This is fiction, and we say so somewhere.** Every public release (site, store page, press kit) links an "About this project" note explaining the 1993 history is invented. The bit is the fun; fooling people is not the goal.
2. **No real brands, outlets, or people.** Use the fictional stand-ins in §6. No Toys "R" Us, Nintendo Power, real pros, or real reviewers. Platform names (SNES/Super Famicom, Super Multitap) may be used descriptively. No Nintendo Official Seal or "licensed by Nintendo" wording anywhere (matches the manual's trademark note).
3. **Run a quick trademark/name search** before any fictional name goes public. Rename if it collides with a real company or person.
4. **Real-world facts may anchor fiction but never be altered.** Pickleball was invented in 1965 on Bainbridge Island, WA. The Super Multitap came out in 1993. Madden/NHL "'94" shipped fall 1993. Keep these accurate.
5. **The game tells the truth.** In-lore claims about the cartridge must match the actual SNES ROM. If the box says it, the ROM does it. Source of truth for features: the instruction manual (A-02), which must itself match the ROM. Behind the manual: [MANUAL.md](MANUAL.md), [PLAYER_GUIDE.md](PLAYER_GUIDE.md), and the ADRs in [decisions/](decisions/).
6. Mark every new entry **[CANON]**, **[PROPOSED]**, or **[OPEN]**. Only the project lead promotes PROPOSED to CANON.
7. **Period-true words only.** In-universe text uses what a 1993 player would know. The ROM already follows this: the park's courts were renamed from DUPR-style ratings (`3.5-4.5`) to ROOKIE / CLUB / CHAMP because DUPR didn't exist in 1994 (ADR-070). No "online," "DLC," "patch," or "ratings app" in any 1993–95 artifact.

---

## 1. The pitch, in-universe

*Pickleball '94* was a small studio's swing at the big yearly sports franchises: a "'94" title on a sport most kids had never heard of, released into the most crowded holiday season of the era. It was far more game than anyone expected (eight teams with their own personalities, a career mode for three players, a park full of open-play courts, a ball machine, four-player doubles on the Multitap, and a battery save) and it still flopped. It built a cult following, and its promised sequel, *Pickleball '95*, was never released. Thirty-three years later, it is "faithfully remastered."

The core joke: **a yearly franchise that only ever had one year.**

The second joke, which the ROM now earns: **it is absurdly over-built for a flop.** Unlockable hats with a museum to display them, a ponytail toggle, a close-call line replay, teams that trash-talk after lost points, a pickle mascot behind the ball machine. Reviews and fan lore should lean on this: nobody bought it, and the few who did found everything in it.

---

## 2. Companies

| Entity | Role | Status |
|---|---|---|
| **Ferry Dock Software** | Developer. Five-person team, Bainbridge Island area, named for the commute everyone shared. | [PROPOSED] |
| **Sunvale Entertainment** | Publisher. Mid-size US publisher chasing the sports-license boom. Folded in 1995. | [PROPOSED] |
| **Tiny Chair** | Real studio; developer/publisher of the Godot remaster. Real studio, invented history. Name search required before public use. | [PROPOSED] |

### Tiny Chair origin story [PROPOSED]
On Christmas morning 1993, a kid sits on a tiny chair far too close to the TV, holding a *Pickleball '94* box (artifact A-05). That kid grows up, loves this flop enough to rebuild it, and names the studio after the chair. The same photo opens the remaster's "making of" page (A-09).

### Tiny Chair logo [CANON direction, final art pending]
Based on the original Tiny Chair mark: "TINY" stacked tightly over "CHAIR" in heavy uppercase. The **I** in TINY runs straight down into a lowercase, square-cornered **h** in CHAIR, so the I becomes the chair back and the h's shoulder and right leg become the seat and front leg. The single lowercase letter in an all-caps word is what makes the chair read.
- Primary: cream letters `#F2EBDC` on navy `#1C2340`, with the I and h (the chair) in coral `#E8734A`. No drop shadow.
- Secondary: one-color version for stamps, cartridge labels, and favicons.
- The old tagline ("UI/UX development & consulting") is retired.
- Modern and clean, in deliberate contrast with the loud early-90s Ferry Dock and Sunvale marks on the 1993 box.

### Ferry Dock team (1993)
Lore hooks are tied to things the ROM really does, so a credits page or interview can point at them.

| Name | Role | Lore hook | Status |
|---|---|---|---|
| — | Lead programmer | Wrote the "two-bounce rule enforced" engine and the kitchen-volley call; famously refused to cut either. Squeezed four-player doubles into the frame budget. | [OPEN] |
| — | Artist | Drew one shared player body for everyone, recoloured per player "to save the cart": four skin tones, ten hair colours, thirteen hats, and a ponytail. Also drew the pickle behind the ball machine. | [OPEN] |
| — | Designer | Wrote all eight ladder teams, every story-card line, and the quips teams mutter after losing a point. Put the Snowbirds' "since 1962" in twice (§8, Q8). | [OPEN] |
| — | Producer | Fought for the battery save so families could share Career slots (three on the cart), and for the park so kids had somewhere to play after the ladder. | [OPEN] |
| — | Composer | Wrote the theme that ended up in the TV spot; recorded the six venue ambiences (park birds, lake gulls, sunset crickets, dinosaur jungle, space blips, undersea bubbles). | [OPEN] |

---

## 3. Cover athlete

**Dale "Dink" Dawson** [PROPOSED; name search required]
- Fictional 1990s pickleball "legend," name and signature on the box ("Dale Dawson Presents" optional).
- In-joke: the man on the box is not in the game. The ladder ends with the Champs (Ace and Jade), and Dawson never appears: no opponent, quip, story card, or park player in the ROM names him (checked 2026-10-18). The remaster finally adds him as an unlockable.
- Keep it that way: Dawson must never be added to the SNES ROM, or the joke breaks.

---

## 4. Timeline

| Date | Event | Status |
|---|---|---|
| 1965 | Pickleball invented on Bainbridge Island, WA (real) | [CANON] |
| Spring 1992 | Ferry Dock pitches "a pickleball game" to Sunvale; Sunvale insists on a '94 title to sit beside the yearly franchises | [PROPOSED] |
| Nov 1993 | **Pickleball '94 ships for SNES. $59.99.** | [PROPOSED] |
| Holiday 1993 | Toy Kingdom circular, 30-second TV spot, mall demo kiosks (running the title screen's CPU demo match) | [PROPOSED] |
| Dec 25, 1993 | The Christmas-morning photos (see §7) | [PROPOSED] |
| Early 1994 | Reviews land: praised for feel and depth, confused by the rules | [PROPOSED] |
| Summer 1994 | "Riverdale Park Open" mall tournament; flyer survives | [PROPOSED] |
| Early 1995 | *Pickleball '95* preview in Game Front Monthly: one screenshot teasing features the original didn't have (see §5) | [PROPOSED] |
| Mid 1995 | Sunvale folds; '95 cancelled | [PROPOSED] |
| 2000s | Emulation-era rediscovery; fans fixate on the '95 screenshot, and notice the '94 overhead swing does nothing special (§5.7) | [PROPOSED] |
| 2026 | *Pickleball '94* remaster announced: "faithfully recreated," with the '95 features finally delivered | [PROPOSED] |

---

## 5. The game (in-universe facts tied to the real ROM)

Source: instruction manual (A-02), checked against the ROM on `main` at `41f12f5` (release 0.28.0, 2026-10-19). If the ROM changes, update the manual, then this section. ADR numbers point at the design record behind each fact.

### 5.1 The cartridge

| Fact | Status |
|---|---|
| SNES / Super Famicom, NTSC. Internal name `PICKLEBALL 94`. | [CANON] |
| **8 Mbit** (1 MiB) LoROM FastROM, no enhancement chip (ADR-017). Box bullet: "8 MEG". | [CANON] while the ROM stays 8 Mbit. It is nearly full; if it grows to 12 or 16 Mbit, change the box (§8, Q9). |
| **Battery back-up**: 16 Kbit (2 KiB) SRAM, saved automatically at match end and when leaving a menu; no save button (ADR-021, ADR-032). | [CANON] |
| 1–4 players. 2P on controller 2; 3P and 4P need a Super Multitap in port 2 (ADR-073). | [CANON] |
| The title screen plays a CPU-vs-CPU demo match behind the menu. The Court and Options pages show changes live on that demo. | [CANON] |

### 5.2 Modes

Eleven title-screen entries, top to bottom (ADR-065, ADR-075, ADR-077): **STORY, SINGLES, DOUBLES, OPEN PLAY, PRACTICE, TUTORIAL, CAREER, RECORDS, COURT, OPTIONS, MODIFIERS.** [CANON]

| Mode | What the ROM does | Status |
|---|---|---|
| Story | **Save the park.** Mr. Grimsby of Grimsby Parking Co. will pave the park for a 400-space lot unless someone beats the Champs. You and your partner **Pepper** (a park regular: backwards cap, red hair, lime shirt) beat the other seven teams first, and each signs the petition. The Gym Bros are on Grimsby's payroll; the Twins are his niece and nephew; the Champs play with the park as the bet. Doubles, one chapter per ladder team, a scene before each match, an ending, progress saved (ADR-077). Grimsby is only ever text, never a sprite. | [CANON] |
| Singles | You against a CPU captain, climbing the ladder; or 1P vs 2P. | [CANON] |
| Doubles | You and a CPU partner against a CPU team. Up to four humans: 1P and 3P on the left, 2P and 4P on the right, CPUs fill empty spots (ADR-018, ADR-073). | [CANON] |
| Open Play | A park with three courts, each a CPU skill level: **ROOKIE** (teal, Easy), **CLUB** (gold, Medium), **CHAMP** (red, Hard). Walk up, put your paddle in the rack, wait your turn. Winners stay on, up to two wins in a row (ADR-034, ADR-071). A **clubhouse** off the path holds the records board and a ball machine. | [CANON] |
| — Park points | Career players start with 10; +2 for a win at your level, −1 for a loss. CLUB opens at 15, CHAMP at 25, and stays open (ADR-070). | [CANON] |
| — Pick your partner | A career player can press Y at the rack to choose any captain they've beaten on the ladder (ADR-072). | [CANON] |
| Practice | A ball machine (with a pickle mascot behind it) feeds DRIVE / LOB / DINK / MIX to six targets at three rates; you chase a streak of returns into a chosen zone. Best streak per zone is saved (ADR-028). | [CANON] |
| Tutorial | The ball machine teaches six steps (SWING, AIM, DRIVE, LOB, DINK, SLICE), three good returns each, then WELL DONE! Nothing is saved (ADR-075). | [CANON] |
| Career | Three save slots. Name (8 letters), body, skin, hair, shirt, hat, paddle. Lifetime stats, unlocks, park points. NO PLAYER plays without recording (ADR-047). | [CANON] |
| — Hat Museum | X on a player's card: twelve display cases, 4 × 3; locked hats are silhouettes with a hint (ADR-061). | [CANON] |
| Records | Win/loss in singles and doubles, current and best streak, shutouts, longest rally, ladder places, ladder clears, with arcade-style three-letter initials (ADR-040). | [CANON] |
| Court | Venue, time of day, and four court colours (surface, ground, lines, kitchen) (ADR-022, ADR-027). | [CANON] |
| Options | POINTS 3–21, SCORING (FIRST TO / WIN BY 2), DIFFICULTY (KID / EASY / MEDIUM / HARD), SOUND (ALL / NO CROWD / OFF), CAMERA with AMOUNT (ADR-025, ADR-050). | [CANON] |
| Modifiers | Eight toggles that combine: BIG ball, FAST ball, LAVA kitchen, GHOST ball, POPCORN rally, ICE footing, GLOW lights, INVERSE colours (ADR-033, ADR-053, ADR-054). | [CANON] |

### 5.3 Play and rules

| Fact | Status |
|---|---|
| Side-on court view; you always play the left side. A ring at each player's feet: 1P red, 2P blue, 3P yellow, 4P green, CPU grey (ADR-066). | [CANON] |
| Moves: **B** standard swing (hold to charge a serve), **A** drive, **Y** lob, **Y + away from net** dink, **X** slice, **hold L or R** to hit short (L+A fast drop, L+Y dink). The D-pad aims at contact. | [CANON] |
| **The push:** hold toward the net longer and a standard shot or drive goes deeper; hold it about a second and it sails out. Lobs always land deep (ADR-076). | [CANON] |
| Charged serve: a power bar fills and drains; let go on the last white block and it dumps into the net (ADR-044). | [CANON] |
| **ARCADE RULES** on the scoreboard. Rally scoring only; first to 5 by default (3–21 in OPTIONS), optional win by 2. | [CANON] |
| Serve diagonally into the service box; two-bounce rule enforced; lines are in. | [CANON] |
| **Kitchen volleys are a fault** in the standard rules: volley with a foot in the kitchen or on its line and the banner reads **KITCHEN!** (ADR-052). Merely stepping in is only a fault with the LAVA modifier (**LAVA!**). | [CANON] (v0.2 said kitchen faults were LAVA-only; that was wrong) |
| Result banners: OUT!, NET!, KITCHEN!, FAULT!, 2 BOUNCES, VOLLEY!, LAVA!, then who won the point. MATCH POINT on the scoreboard. | [CANON] |
| Close-call replay: after a long rally, a ball that lands just out gets a line close-up showing it past the paint, and by how many inches (ADR-041). | [CANON] |
| Doubles: each player covers a lane; partners never swap sides; the serving side alternates who serves as its score changes (ADR-018). | [CANON] |
| KID difficulty swings and serves for you; you just run into the ball's path. | [CANON] |
| The swing shows the contact: a ball over the head gets an overhead wind-up and swing, a stretched contact (the one that sprays) a lunge, a ball near the ground a scoop. Looks only: the shot itself is the same (ADR-074). | [CANON] |

### 5.4 The ladder and its teams

Eight teams, captain first. Singles plays the captain; doubles plays both. Singles and doubles keep separate ladder places (ADR-055). Beat the Champs and the ladder clears and starts over: **CHAMPION!** with a trophy. Quitting a ladder match from the pause menu resets that ladder; losing keeps your place. [CANON]

| # | Team | Players | Home court | Intro line | Plays like | Status |
|---|---|---|---|---|---|---|
| 1 | The Hendersons | Dad and Kiddo | Park | "DAD JOKES INCOMING, SPORT!" | Stays back. Celebrates with a thumbs-up. | [CANON] |
| 2 | The Snowbirds | Marge and Walt | Lakeside | "WE'VE PLAYED SINCE 1962." | Stays back. | [CANON] |
| 3 | The Gym Bros | Brad and Chad | Dinosaur | "DO YOU EVEN DINK, BRO?" | Rushes the net after the bounces; serves hard, sometimes into the net. Flexes. | [CANON] |
| 4 | The Pirates | Pete and Bilge | Lakeside | "ARR! PREPARE TO BE BOARDED!" | Plays the kitchen line; first team to drop and dink back. En garde pose. | [CANON] |
| 5 | The Goth Club | Raven and Vlad | Space | "THE KITCHEN IS OUR CRYPT." | Kitchen line, dinks, mixes in slices. | [CANON] |
| 6 | The Surf Crew | Kai and Lani | Undersea | "COWABUNGA, DUDE!" | Kitchen line, dinks. | [CANON] |
| 7 | The Twins | Max and Mia | Sunset | "DOUBLE TROUBLE. READY?" | Kitchen line, dinks. | [CANON] |
| 8 | The Champs | Ace and Jade | Sunset | "THE TITLE STAYS WITH US." | Everything: net play, slices, hard serves, hunting the player in back. | [CANON] |

- Each team has a story card before and after a match (intro, taunt on a win, concession on a loss) and a portrait bust (ADR-026). Now and then the side that loses a point says something under the banner: a quip (ADR-048, ADR-063). [CANON]
- In a singles ladder match, the captain's partner watches from the stands and hops when their teammate scores (ADR-042). [CANON]
- Stronger teams in doubles cover the middle, hit between you and your partner, and pick on whichever of you hangs back (ADR-036, ADR-043). [CANON]
- The ladder is tuned so every rung is measurably harder than the last, in singles and both doubles bots (ADR-062). [CANON]

### 5.5 Players, paddles, and hats

| Fact | Status |
|---|---|
| Shirt colours **Blue / Red / Green / Gold**; male or female (the female player wears a ponytail). A colour already taken shows as a dark silhouette. | [CANON] |
| Career looks: 4 skin tones (always open), **10 hair colours** (5 to unlock), **13 hats** counting the bare head (11 to unlock) (ADR-059, ADR-060). | [CANON] |
| Paddles **Wood / Blue / Red / Gold**, same 9-point stat budget across Power / Control / Speed: Wood 2/5/2, Blue 3/3/3, Red 5/2/2, Gold 2/2/5 (ADR-015, ADR-020). | [CANON] |
| Four hats trade one paddle stat point for another: VISOR (+Control −Power), HEADBAND (+Speed −Control), TRICORN (+Power −Control), BANDANA (+Power −Speed). The rest are looks only. | [CANON] |

Unlocks, earned against the CPU (the card shows the next one as **NEXT:**): [CANON]

| Unlock | How |
|---|---|
| VISOR | Win a match |
| BEANIE | Win 3 matches |
| HEADBAND | Win 10 matches |
| BACKCAP | Win 25 matches |
| BANDANA | A 20-shot rally |
| WHIRLY (propeller beanie) | A 30-shot rally |
| TRICORN | Beat the Pirates |
| VIKING | Beat the Gym Bros |
| CHEF | A 10-win streak |
| CROWN (silver, no gold left in the palette) | 3 ladder clears |
| BUCKET | 10 open play games |
| Blue hair | 5 aces |
| Pink hair | A shutout |
| Green hair | 5 open play games |
| Purple hair | A 5-win streak |
| Gold hair | Clear the ladder |

### 5.6 Venues, sound, and show

| Fact | Status |
|---|---|
| Six venues: **Park, Lakeside, Sunset, Dinosaur, Space, Undersea**; AUTO plays each team on its home court (ADR-024, ADR-046). | [CANON] |
| Day / Dusk / Night; at night the court plays under lights (ADR-045). | [CANON] |
| Each venue has its own ambience: birds, gulls, crickets, jungle calls, space blips, bubbles (ADR-031). Critters cross the sky now and then: a gull at Lakeside, a satellite in Space, a fish Undersea (ADR-064). | [CANON] |
| CAMERA leans the court toward the ball, with the scenery in parallax layers (ADR-050, ADR-051). | [CANON] |
| Celebrations, slumps, diving saves, emotes (`!`, `?`, sweat, anger, a star after a long rally, Z's while a server dawdles), bounce dust, net shiver, squash-and-stretch on drives (ADR-064). | [CANON] |
| The ROM never names the park. The venue is just PARK, and Open Play's park has no name on screen. | [CANON] (see Riverdale row) |
| "Riverdale Park" | [OPEN] — not in the ROM. It may name the park in non-cartridge artifacts (the A-07 flyer, the mall), but no box, manual, or review may say the game calls it that. Adding the name to the ROM is a separate decision (§8, Q10). |

### 5.7 The smash that isn't [PROPOSED]
True of the real ROM and good for the rediscovery story: a player under a high ball winds up and swings overhead, paddle straight up (ADR-074), but the ball comes off exactly as a normal swing would. There is no smash shot. In lore, Ferry Dock drew the overhead for a smash they cut for time and kept the animation because it looked too good to lose; '95 was going to make it real. Stays true until the overhead becomes its own shot (IDEAS.md, "Hitting: reach, height, and overheads").

v0.3 had an "unused frames on the cartridge" legend here; the game draws those frames now, so it is retired.

### 5.8 Pickleball '95 promised features [OPEN]
Must be things the ROM does **not** do, so the remaster can "deliver" them. Every candidate below was checked against `main` on 2026-10-18; if the ROM later adds one, it leaves this list. Retired since v0.2: Super Multitap four-player (shipped, ADR-073) and kitchen faults as a standard rule (the kitchen-volley fault is standard, ADR-052).

Strongest candidates:
- **Traditional side-out scoring.** The ROM is rally scoring only (OPTIONS has FIRST TO and WIN BY 2).
- **Dale Dawson as a playable opponent** (§3).
- **The overhead smash** (§5.7): the '95 screenshot shows a smash landing at an opponent's feet, the payoff for the '94 overhead swing that never hit any harder.
- **The momentum rule:** a volley that carries you into the kitchen is a fault. The ROM only calls the kitchen at contact (IDEAS.md).

Also usable (all unbuilt ideas in IDEAS.md / GOOFY_IDEAS.md):
- A **fifth player as line judge** on the Multitap.
- New modifiers: MOON (low gravity), BOUNCY, WIND, DINK OR DIE, SUDDEN DEATH, MULTI BALL.
- A **club manager** mode: run the park behind Open Play.

---

## 6. Fictional stand-ins for real outlets

| Real-world type | Our stand-in | Status |
|---|---|---|
| Toy-store chain | **Toy Kingdom** | [PROPOSED] |
| Official-style magazine | **Game Front Monthly** | [PROPOSED] |
| Tips/strategy magazine | **Power Player Tips** | [PROPOSED] |
| Rental chain | **Reel Time Video** | [PROPOSED] |
| Mall | **Riverdale Mall** (shares a name with the park in-universe) | [PROPOSED] |

---

## 7. Artifact registry

Every artifact gets a row. Record its in-universe date and the canon facts it shows, so later pieces stay consistent.

| ID | Artifact | In-universe date | Shows | Status |
|---|---|---|---|---|
| A-01 | Box art (front/back) | Nov 1993 | Title, cover athlete, box bullets ("8 MEG", "BATTERY BACK-UP", "1–4 PLAYERS" with the Super Multitap named in plain text), screenshots | [OPEN] |
| A-02 | Instruction manual | Nov 1993 | Full feature reference. Claude Doc: https://claude.ai/code/artifact/2080a5f3-9ad7-4050-9eb5-d65d0d2f3988; repo snapshot [MANUAL.md](MANUAL.md) (2026-10-16, already covers four players and pick your partner). Needs a Ferry Dock/Sunvale credits page and screenshots. | Draft |
| A-03 | Toy Kingdom holiday circular | Dec 1993 | $59.99 | [OPEN] |
| A-04 | 30-second TV spot | Holiday 1993 | Theme song, gameplay | [OPEN] |
| A-05 | Christmas-morning photos | Dec 25, 1993 | Faded film, date stamp, box under tree; the Tiny Chair kid photo | [OPEN] |
| A-06 | Game Front Monthly review | Feb 1994 | Score, rules-confusion quote | [OPEN] |
| A-07 | Riverdale Park Open flyer | Jun 1994 | Mall tournament | [OPEN] |
| A-08 | Pickleball '95 preview page | Early 1995 | The '95 screenshot (§5.8) | [OPEN] |
| A-09 | Remaster "making of" page | 2026 | Recovered-source story | [OPEN] |
| A-10 | Remaster patch notes | 2026 | Original bugs preserved as toggles | [OPEN] |
| A-11 | Tiny Chair logo | 2026 | Navy/cream/coral mark (§2) | Direction chosen |

Printable artwork already made: list it here with file paths.

---

## 8. Open questions

Questions the repo can settle have an answer; the project lead still decides anything marked as a recommendation.

1. Is the Christmas photo a real childhood photo or staged? — *Project lead's call.*
2. Ferry Dock team names and one personality line each (they go on the manual's credits page). — *Open. §2 now ties each role to real ROM features to write from.*
3. Review score: how good was it, really? — *Suggest 7.5/10: "great feel, baffling rules." The ROM supports both halves: the rules really are strict (two-bounce, kitchen volleys, serve boxes), and a reviewer would have been the first to meet KITCHEN! and VOLLEY! banners without reading the manual.*
4. Did a Japanese version exist? — *Possible later lore. The ROM is an NTSC cart with all-English text, so a "lost" Super Famicom release would be a separate, never-released build.*
5. How did the remaster "recover" the source: an attic box of floppies, or reverse-engineered from a cart? — *Open. Note the real code is C and 65816 assembly, which a 1993 team could have written; the attic-floppies story fits that.*
6. Does Dale Dawson have a real-sounding backstory, or stay a cardboard cutout? — *Open. Either way he stays out of the SNES ROM (§3).*
7. What did the *Pickleball '95* screenshot show? — *Recommend the overhead smash (§5.7, §5.8): it ties the '95 tease to an animation every '94 player has seen do nothing.*
8. **Snowbirds line conflict:** "WE'VE PLAYED SINCE 1962" predates pickleball's 1965 invention. — *The ROM commits to it twice: the intro card, and a Snowbirds quip, "IN 1962 THAT WAS IN." Recommend keeping it as a deliberate joke (they're fibbing, or claim to have invented it), and having the A-06 review or a fan wiki "catch" it.*
9. *(new)* Cart size on the box. — *8 Mbit today, and close to full. The project is happy to grow the cart; if it goes to 12 or 16 Mbit before the box is final, the box says that instead. Decide at A-01.*
10. *(new)* Should the ROM name its park "Riverdale Park"? — *Today it doesn't (§5.6). Adding it is a small text change but must follow the period-true rule and the BG3 HUD limits. Until then, Riverdale stays off the box and manual.*
11. *(new)* The working title. — *The README still calls "Pickleball '94" a working title. The whole bible assumes it's final; confirm.*
12. *(new)* **Pepper** and **Mr. Grimsby / Grimsby Parking Co.** are story mode's names (ADR-077), already in the ROM. — *Run the name search (§0, rule 3) before they go public; renaming them is a one-line change in `assets/source/story.json`.*

---

## 9. How to contribute (humans and Claude)

- Read this file before writing any lore artifact.
- New ideas go in as **[PROPOSED]** with a one-line rationale.
- Never silently change a [CANON] entry. Propose the change in §8 instead.
- When an artifact is finished, add or update its row in §7 in the same commit.
- When a game feature ships (a new ADR), add it to §5 in the same branch, and check it against §5.7 and §5.8: anything the ROM now does can't be a '95 promise or an unused-content legend.
- Commit prefix: `LORE-xx: <summary>`, matching the `M2-BALL-03` task style.
