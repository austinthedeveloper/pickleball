PVSNESLIB_HOME ?= ../pvsneslib
CONFIG ?= debug
VERSION_NAME := 0.3.0
BUILD_ID ?= $(shell id=$$(git rev-parse --short HEAD 2>/dev/null || echo dev); \
	if test -n "$$(git status --porcelain 2>/dev/null)"; then \
		fingerprint=$$({ git diff --binary HEAD; git status --porcelain; } | \
			shasum -a 256 | cut -c1-8); \
		echo "$$id-dirty-$$fingerprint"; \
	else echo "$$id"; fi)
ROMNAME := pickleball-$(VERSION_NAME)-$(CONFIG)-$(BUILD_ID)
RELEASE_ROM := pickleball-$(VERSION_NAME)-release-$(BUILD_ID)
ROMTITLE := PICKLEBALL 94

export ROMNAME ROMTITLE

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

.PHONY: all debug release hardware-test profile assets check host-check asset-check clean dist smoke repro

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
	@$(MAKE) --no-print-directory $(ROMNAME).sfc CONFIG=$(CONFIG) PVSNESLIB_DEBUG=$(PVSNESLIB_DEBUG)
	@mkdir -p build/$(CONFIG)
	@rm -f build/$(CONFIG)/pickleball-*.sfc \
		build/$(CONFIG)/pickleball-*.sym build/$(CONFIG)/pickleball-*.map
	@cp $(ROMNAME).sfc build/$(CONFIG)/$(ROMNAME).sfc
	@cp $(ROMNAME).sym build/$(CONFIG)/$(ROMNAME).sym
	@cp $(ROMNAME).symfull build/$(CONFIG)/$(ROMNAME).map
	@echo "Built build/$(CONFIG)/$(ROMNAME).sfc"

assets:
	@python3 tools/generate_assets.py

asset-check: assets
	@python3 tools/generate_assets.py --check
	@python3 tools/test_asset_validation.py

HOST_CORE := src/ai.c src/ball.c src/collision.c src/court.c src/game.c \
	src/input_state.c src/match.c src/player.c src/projection.c src/rules.c \
	src/serve.c
HOST_TESTS := player ball collision serve rules ai projection match match_flow input

host-check: assets
	@mkdir -p build/host
	@for t in $(HOST_TESTS); do \
		$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
			-Iassets/generated test/test_$$t.c $(HOST_CORE) \
			-o build/host/test_$$t && build/host/test_$$t || exit 1; \
	done

CC_HOST ?= cc
check: host-check asset-check

MESEN_BIN ?= /Applications/Mesen.app/Contents/MacOS/Mesen
SMOKE ?= m1
SMOKE_FRAMES ?= 600

# Headless MesenCE run of test/smoke/$(SMOKE).timeline against the debug ROM;
# screenshots land in build/smoke/$(SMOKE)/ for review.
smoke: debug
	@rm -rf build/smoke/$(SMOKE) && mkdir -p build/smoke/$(SMOKE)
	@SMOKE_TIMELINE=$(CURDIR)/test/smoke/$(SMOKE).timeline \
		SMOKE_OUT=$(CURDIR)/build/smoke/$(SMOKE) SMOKE_FRAMES=$(SMOKE_FRAMES) \
		"$(MESEN_BIN)" --testRunner --doNotSaveSettings --noAudio \
		--debug.scriptWindow.allowIoOsAccess=true \
		$(CURDIR)/tools/mesen_smoke.lua \
		$(CURDIR)/build/debug/pickleball-$(VERSION_NAME)-debug-$(BUILD_ID).sfc
	@ls build/smoke/$(SMOKE) | wc -l | xargs echo "smoke screenshots:"

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

