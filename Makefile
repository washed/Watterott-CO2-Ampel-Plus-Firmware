PROJECT = CO2-Ampel_Plus
BOARD_TYPE = co2ampel:samd:sb
ARDUINO_CLI = arduino-cli
SERIAL_PORT = COM18
VERBOSE = 1

# Build path -- used to store built binary and object files
BUILD_DIR=_build
BUILD_CACHE_DIR=_build_cache
BUILD_PATH=$(PROJECT)/$(BUILD_DIR)
BUILD_CACHE_PATH=$(PROJECT)/$(BUILD_CACHE_DIR)
BUILD_COMMAND=$(ARDUINO_CLI) compile $(VERBOSE_FLAG) --warnings more --build-path=$(BUILD_PATH) --build-cache-path=$(BUILD_CACHE_PATH) -b $(BOARD_TYPE) $(PROJECT)
CLEAN_COMMAND=rm -rf $(BUILD_PATH) && rm -rf $(BUILD_CACHE_PATH)
FORMAT_COMMAND=clang-format --style=Chromium -i **/*.cpp **/*.h
FORMAT_TEST_COMMAND=clang-format --style=Chromium --Werror --dry-run **/*.cpp **/*.h

ifeq (${CI}, true)
	BUILDER_USER=root
	WORKDIR=/root
else
	BUILDER_USER=runner
	WORKDIR=/home/runner
endif

ifneq ($(V), 0)
	VERBOSE_FLAG=-v
else
	VERBOSE_FLAG=
endif

BUILDER_BASE_COMMAND=BUILDER_USER=$(BUILDER_USER) WORKDIR=$(WORKDIR) docker-compose run arduino-builder

.PHONY: all example program clean

all: example

builder:
	docker-compose build

clean:
	$(BUILDER_BASE_COMMAND) $(CLEAN_COMMAND)

build: builder clean
	$(BUILDER_BASE_COMMAND) $(BUILD_COMMAND)

format:
	$(BUILDER_BASE_COMMAND) $(FORMAT_COMMAND)

format-test:
	$(BUILDER_BASE_COMMAND) $(FORMAT_TEST_COMMAND)

# program:
# 	$(ARDUINO_CLI) upload $(VERBOSE_FLAG) -p $(SERIAL_PORT) --fqbn $(BOARD_TYPE) $(PROJECT)
