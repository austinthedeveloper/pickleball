PVSNESLIB_HOME ?= ../pvsneslib
CONFIG ?= debug
VERSION_NAME := 0.1.0
BUILD_ID ?= $(shell id=$$(git rev-parse --short HEAD 2>/dev/null || echo dev); \
	if test -n "$$(git status --porcelain 2>/dev/null)"; then \
		fingerprint=$$({ git diff --binary HEAD; git status --porcelain; } | \
			shasum -a 256 | cut -c1-8); \
		echo "$$id-dirty-$$fingerprint"; \
	else echo "$$id"; fi)
ROMNAME := pickleball-$(VERSION_NAME)-$(CONFIG)-$(BUILD_ID)
RELEASE_ROM := pickleball-$(VERSION_NAME)-release-$(BUILD_ID)
ROMTITLE := PICKLEBALL 94 M1

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

CFLAGS += -Iinclude -DBUILD_ID_STRING=\"$(VERSION_NAME)-$(CONFIG_TAG)-$(BUILD_ID)\"

ifeq ($(CONFIG),debug)
CFLAGS += -DBUILD_DEBUG=1
endif
ifeq ($(CONFIG),hardware-test)
CFLAGS += -DBUILD_HARDWARE_TEST=1
endif

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

.PHONY: all debug release hardware-test profile assets check host-check asset-check clean dist

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

host-check:
	@mkdir -p build/host
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		test/test_player.c src/court.c src/game.c src/input_state.c src/player.c \
		-o build/host/test_player
	@build/host/test_player
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		test/test_ball.c src/ball.c src/court.c -o build/host/test_ball
	@build/host/test_ball
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		test/test_collision.c src/ball.c src/collision.c src/court.c src/player.c \
		-o build/host/test_collision
	@build/host/test_collision
	@$(CC_HOST) -std=c99 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
		test/test_serve.c src/ball.c src/court.c src/game.c src/player.c \
		src/serve.c -o build/host/test_serve
	@build/host/test_serve

CC_HOST ?= cc
check: host-check asset-check

clean: cleanBuildRes cleanRom cleanLogs
	@rm -rf build dist assets/generated
	@rm -f pickleball-*.sfc pickleball-*.sym pickleball-*.symfull pickleball-*.log

dist: release
	@mkdir -p dist
	@rm -f dist/pickleball-*.sfc dist/pickleball-*.sym dist/pickleball-*.map \
		dist/pickleball-*.sha256 dist/pickleball-*-build.txt
	@cp build/release/$(RELEASE_ROM).sfc dist/$(RELEASE_ROM).sfc
	@cp build/release/$(RELEASE_ROM).sym dist/$(RELEASE_ROM).sym
	@cp build/release/$(RELEASE_ROM).map dist/$(RELEASE_ROM).map
	@shasum -a 256 dist/$(RELEASE_ROM).sfc > dist/$(RELEASE_ROM).sha256
	@python3 tools/build_report.py --config release --build-id $(BUILD_ID) \
		--rom dist/$(RELEASE_ROM).sfc --output dist/$(RELEASE_ROM)-build.txt
	@echo "Distribution written to dist/"

