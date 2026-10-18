PVSNESLIB_HOME ?= ../pvsneslib
CONFIG ?= debug
VERSION_NAME := 0.25.0
BUILD_ID ?= $(shell id=$$(git rev-parse --short HEAD 2>/dev/null || echo dev); \
	if test -n "$$(git status --porcelain 2>/dev/null)"; then \
		fingerprint=$$({ git diff --binary HEAD; git status --porcelain; } | \
			shasum -a 256 | cut -c1-8); \
		echo "$$id-dirty-$$fingerprint"; \
	else echo "$$id"; fi)
ROMNAME := pickleball-$(VERSION_NAME)-$(CONFIG)-$(BUILD_ID)
RELEASE_ROM := pickleball-$(VERSION_NAME)-release-$(BUILD_ID)
ROMTITLE := PICKLEBALL 94

# FastROM (ADR-017): code runs from banks $80+ at 3.58 MHz with MEMSEL set by
# the PVSnesLib FastROM runtime. Cartridges need 120 ns (or faster) ROM.
FASTROM := 1

# 1 MiB cart (R19): 32 LoROM banks of 32 KiB; header ROM size $0A (2^10
# KiB). The soundbank stays at banks 13-15; banks 16-31 take new data.
ROMBANKS := 32
ROMSIZE := 0A

# Battery-backed save RAM (ADR-021): ROM + RAM + battery, 2 KiB SRAM.
CARTRIDGETYPE := 02
SRAMSIZE := 01

export ROMNAME ROMTITLE FASTROM

# Sound (ADR-029/031/046): tools/assetgen writes sfx.it (the effects, first)
# and one ambience module per venue; smconv turns them into a SNESMOD
# soundbank from bank 13, one bank per 32 KiB (up to 15: 96 KiB). profile
# counts its banks into soundbank_banks.h for the boot code. -f checks that
# each ambience fits sound RAM alongside the effects.
AUDIOFILES := assets/generated/sfx.it assets/generated/ambience_0.it \
	assets/generated/ambience_1.it assets/generated/ambience_2.it \
	assets/generated/ambience_3.it assets/generated/ambience_4.it \
	assets/generated/ambience_5.it
export SOUNDBANK := assets/generated/soundbank
SMCONVFLAGS := -s -o $(SOUNDBANK) -b 13 -f

CONFIG_TAG := $(CONFIG)
ifeq ($(CONFIG),debug)
CONFIG_TAG := DBG
endif
ifeq ($(CONFIG),release)
CONFIG_TAG := REL
endif
ifeq ($(CONFIG),hardware-test)
CONFIG_TAG := HW
endif

CFLAGS += -Iinclude -Iassets/generated -DBUILD_ID_STRING=\"$(VERSION_NAME)-$(CONFIG_TAG)-$(BUILD_ID)\"

ifeq ($(CONFIG),debug)
CFLAGS += -DBUILD_DEBUG=1
endif
ifeq ($(CONFIG),hardware-test)
CFLAGS += -DBUILD_HARDWARE_TEST=1
endif
ifeq ($(PROFILE),1)
CFLAGS += -DBUILD_PROFILE=1
endif

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

.PHONY: all debug release hardware-test profile assets check host-check asset-check tcc-check tuning-dump tuning-export tuning-check conformance lockstep clean dist publish smoke repro stats golden-record golden-compare perf sfx-preview

all: debug

debug:
	@$(MAKE) --no-print-directory profile CONFIG=debug PVSNESLIB_DEBUG=1

release:
	@$(MAKE) --no-print-directory profile CONFIG=release PVSNESLIB_DEBUG=0

hardware-test:
	@$(MAKE) --no-print-directory profile CONFIG=hardware-test PVSNESLIB_DEBUG=0

profile: assets
	@$(MAKE) --no-print-directory cleanBuildRes CONFIG=$(CONFIG) PVSNESLIB_DEBUG=$(PVSNESLIB_DEBUG)
	@rm -f $(ROMNAME).sfc $(ROMNAME).sym $(ROMNAME).symfull
	@$(MAKE) --no-print-directory $(SOUNDBANK).asm CONFIG=$(CONFIG) PVSNESLIB_DEBUG=$(PVSNESLIB_DEBUG)
	@echo "#define SOUNDBANK_BANKS $$(grep -c '^SOUNDBANK__' $(SOUNDBANK).asm)" \
		> $(SOUNDBANK)_banks.h
	@$(MAKE) --no-print-directory $(ROMNAME).sfc CONFIG=$(CONFIG) PVSNESLIB_DEBUG=$(PVSNESLIB_DEBUG)
	@mkdir -p build/$(CONFIG)
	@rm -f build/$(CONFIG)/pickleball-*.sfc \
		build/$(CONFIG)/pickleball-*.sym build/$(CONFIG)/pickleball-*.map
	@# The linker writes to the repo root; move (not copy) so only build/ keeps ROMs.
	@mv $(ROMNAME).sfc build/$(CONFIG)/$(ROMNAME).sfc
	@mv $(ROMNAME).sym build/$(CONFIG)/$(ROMNAME).sym
	@mv $(ROMNAME).symfull build/$(CONFIG)/$(ROMNAME).map
	@echo "Built build/$(CONFIG)/$(ROMNAME).sfc"

assets:
	@python3 tools/generate_assets.py

asset-check: assets
	@python3 tools/generate_assets.py --check
	@python3 tools/test_asset_validation.py

HOST_CORE := src/ai.c src/audio_events.c src/ball.c src/collision.c \
	src/court.c src/court_style.c src/events.c src/flair.c src/fx.c src/game.c \
	src/input_state.c src/match.c src/menu.c src/modifiers.c src/open_play.c src/options.c src/pause_menu.c src/player.c \
	src/projection.c src/practice.c src/rules.c src/save.c src/select.c \
	src/serve.c src/story.c src/text.c src/replay.c src/career.c src/career_page.c \
	src/museum.c
HOST_TESTS := player ball collision serve rules ai projection match match_flow input events doubles layout select save court_style opponents options story practice audio pause_menu modifiers open_play replay career career_page museum flair menu text

host-check: assets
	@mkdir -p build/host
	@for t in $(HOST_TESTS); do \
		$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
			-Iassets/generated test/test_$$t.c $(HOST_CORE) \
			-o build/host/test_$$t && build/host/test_$$t || exit 1; \
	done

CC_HOST ?= cc
check: host-check asset-check tcc-check tuning-check

# The gameplay tuning as one file for ports (ADR-068, docs/TUNING.md):
# tuning-export rewrites docs/tuning/tuning.json, tuning-check (in check)
# fails while it is stale or a value changed without a TUNING_VERSION bump.
tuning-dump: assets
	@mkdir -p build/host
	@python3 tools/tuning_export.py --inc build/host/tuning_constants.inc
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		-Iassets/generated -Ibuild/host tools/tuning_dump.c $(HOST_CORE) \
		-o build/host/tuning_dump
	@build/host/tuning_dump > build/host/tuning_raw.json
tuning-export: tuning-dump
	@python3 tools/tuning_export.py --write build/host/tuning_raw.json
tuning-check: tuning-dump
	@python3 tools/tuning_export.py --check build/host/tuning_raw.json

# Frame-by-frame traces of fixed match scenarios for ports to diff against
# (tools/conformance.c, ADR-068): build/conformance/<scenario>.trace.
conformance: assets
	@mkdir -p build/host build/conformance
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		-Iassets/generated -Itest tools/conformance.c $(HOST_CORE) \
		-o build/host/conformance
	@build/host/conformance build/conformance

# C that 816-tcc miscompiles (CLAUDE.md): the host tests can't see it.
tcc-check:
	@python3 tools/check_tcc_pitfalls.py

# Bot-vs-CPU balance numbers (tools/match_stats.c); args: LATE (0: three
# bot skills), MATCHES, OPPONENT (-1 the default CPU, -2 the whole ladder,
# else a ladder opponent, ADR-023), DIFFICULTY (0 KID .. 3 HARD, ADR-025),
# MODE (0 singles, 1 doubles, +2 the net bot; M4-OPP-02), SPEED (ball speed
# percent, ADR-049; 0 the game's, 100; not a setting since ADR-065).
LATE ?= 4
MATCHES ?= 20
OPPONENT ?= -1
DIFFICULTY ?= 2
MODE ?= 0
SPEED ?= 0
stats: assets
	@mkdir -p build/host
	@$(CC_HOST) -std=c99 -O2 -DHOST_TEST -Iinclude -Iassets/generated -Itest \
		tools/match_stats.c $(HOST_CORE) -o build/host/match_stats
	@build/host/match_stats $(LATE) $(MATCHES) $(OPPONENT) $(DIFFICULTY) $(MODE) $(SPEED)

# The sound effects as WAV files in build/sfx/, to listen to (ADR-029).
sfx-preview:
	@python3 tools/sfx_preview.py

# Golden emulator trace (docs/REFACTOR_PLAN.md R0): record before a
# refactor, compare after; simulation state must stay identical.
golden-record: release
	@python3 tools/mesen_probe.py golden-record
golden-compare: release
	@python3 tools/mesen_probe.py golden-compare

# The ROM against the host C (tools/lockstep.c, ADR-069): a singles and a
# doubles match replayed on the host from the ROM's intents, every
# simulation field compared on every frame. Catches the asm twins
# (player_step, ai_probe) drifting from the C, and 816-tcc miscompiles.
LOCKSTEP_RUNS := golden:4260 doubles_ladder:2400 modifiers_fun:2400
lockstep: release
	@mkdir -p build/host build/lockstep
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		-Iassets/generated tools/lockstep.c $(HOST_CORE) \
		-o build/host/lockstep
	@for run in $(LOCKSTEP_RUNS); do \
		t=$${run%%:*}; n=$${run##*:}; \
		python3 tools/mesen_probe.py lockstep-record \
			test/smoke/$$t.timeline $$n build/lockstep/$$t.txt && \
		build/host/lockstep build/lockstep/$$t.txt || exit 1; \
	done

MESEN_BIN ?= /Applications/Mesen.app/Contents/MacOS/Mesen
SMOKE ?= m1
SMOKE_FRAMES ?= 600

MESEN_SAVES ?= $(HOME)/Library/Application Support/MesenCE/Saves

# Headless MesenCE run of test/smoke/$(SMOKE).timeline against the debug ROM;
# screenshots land in build/smoke/$(SMOKE)/ for review. The run starts from a
# blank battery save (ADR-021/023) unless KEEP_SAVE=1.
smoke: debug
	@rm -rf build/smoke/$(SMOKE) && mkdir -p build/smoke/$(SMOKE)
	@test "$(KEEP_SAVE)" = 1 || rm -f \
		"$(MESEN_SAVES)/pickleball-$(VERSION_NAME)-debug-$(BUILD_ID).srm"
	@SMOKE_TIMELINE=$(CURDIR)/test/smoke/$(SMOKE).timeline \
		SMOKE_OUT=$(CURDIR)/build/smoke/$(SMOKE) SMOKE_FRAMES=$(SMOKE_FRAMES) \
		"$(MESEN_BIN)" --testRunner --doNotSaveSettings --noAudio \
		--debug.scriptWindow.allowIoOsAccess=true \
		$(CURDIR)/tools/mesen_smoke.lua \
		$(CURDIR)/build/debug/pickleball-$(VERSION_NAME)-debug-$(BUILD_ID).sfc
	@ls build/smoke/$(SMOKE) | wc -l | xargs echo "smoke screenshots:"

# Frame budget and hang check (headless MesenCE, release ROM): every run in
# test/perf.json within its overrun budget with the main loop running, and
# the SPC700 voices sounding (tools/mesen_voices.lua).
perf: release
	@MESEN_BIN="$(MESEN_BIN)" python3 tools/mesen_probe.py perf

clean: cleanBuildRes cleanRom cleanLogs
	@rm -rf build dist assets/generated
	@rm -f pickleball-*.sfc pickleball-*.sym pickleball-*.symfull pickleball-*.log

dist: release
	@mkdir -p dist
	@rm -f dist/pickleball-* dist/README.md dist/hardware-log.md
	@cp build/release/$(RELEASE_ROM).sfc dist/$(RELEASE_ROM).sfc
	@cp build/release/$(RELEASE_ROM).sym dist/$(RELEASE_ROM).sym
	@cp build/release/$(RELEASE_ROM).map dist/$(RELEASE_ROM).map
	@cd dist && shasum -a 256 $(RELEASE_ROM).sfc > $(RELEASE_ROM).sha256
	@python3 tools/build_report.py --config release --build-id $(BUILD_ID) \
		--rom dist/$(RELEASE_ROM).sfc --output dist/build-report.md
	@cp docs/PLAYER_GUIDE.md dist/README.md
	@cp test/hardware-log.md dist/hardware-log.md
	@{ echo "version: $(VERSION_NAME)"; echo "build_id: $(BUILD_ID)"; \
		echo "commit: $$(git rev-parse HEAD 2>/dev/null || echo unknown)"; \
		echo "branch: $$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"; \
		} > dist/source-revision.txt
	@echo "Distribution written to dist/"

# GitHub release of the current commit (docs/RELEASING.md): tag
# v$(VERSION_NAME), push the tag, attach the dist ROM, checksum, build report,
# and player guide. Needs a clean main that matches origin/main and the gh
# CLI logged in; NOTES=file replaces the generated notes.
TAG := v$(VERSION_NAME)
NOTES ?=
publish:
	@test -z "$$(git status --porcelain)" || \
		{ echo "publish: commit or stash your changes first"; exit 1; }
	@test "$$(git rev-parse --abbrev-ref HEAD)" = main || \
		{ echo "publish: run from main"; exit 1; }
	@git fetch -q origin
	@test "$$(git rev-parse HEAD)" = "$$(git rev-parse origin/main)" || \
		{ echo "publish: main must match origin/main (push or pull first)"; exit 1; }
	@! git rev-parse -q --verify "refs/tags/$(TAG)" >/dev/null || \
		{ echo "publish: $(TAG) exists; bump VERSION_NAME"; exit 1; }
	@gh auth status >/dev/null 2>&1 || \
		{ echo "publish: run 'gh auth login' first"; exit 1; }
	@$(MAKE) --no-print-directory check
	@$(MAKE) --no-print-directory dist
	@python3 tools/release_notes.py --tag $(TAG) --rom dist/$(RELEASE_ROM).sfc \
		$(if $(NOTES),--intro $(NOTES)) > dist/release-notes.md
	@git tag -a $(TAG) -m "Pickleball '94 $(TAG)"
	@git push -q origin $(TAG)
	@gh release create $(TAG) --title "Pickleball '94 $(TAG)" \
		--notes-file dist/release-notes.md \
		dist/$(RELEASE_ROM).sfc dist/$(RELEASE_ROM).sha256 \
		dist/build-report.md dist/README.md

# M3-ASSET-05: two clean release builds must produce identical generated
# assets and ROM bytes.
repro:
	@$(MAKE) --no-print-directory clean >/dev/null
	@$(MAKE) --no-print-directory release >/dev/null
	@mkdir -p build-repro && shasum -a 256 assets/generated/* \
		build/release/*.sfc | sed 's|build/release/.*sfc|ROM|' > build-repro/a.txt
	@$(MAKE) --no-print-directory clean >/dev/null
	@$(MAKE) --no-print-directory release >/dev/null
	@shasum -a 256 assets/generated/* build/release/*.sfc | \
		sed 's|build/release/.*sfc|ROM|' > build-repro/b.txt
	@diff build-repro/a.txt build-repro/b.txt && \
		echo "reproducible: generated assets and ROM hashes match"; \
		status=$$?; rm -rf build-repro; exit $$status

