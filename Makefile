# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 c10l

# Activity Defaults Switcher — developer convenience tasks.
#
# Wraps the CMake build, the CTest suite, and the C++ formatters/linters so
# the common commands are one target each. Run `make help` for the list.

BUILD_DIR    ?= build
CMAKE        ?= cmake
CTEST        ?= ctest
CLANG_FORMAT ?= clang-format
CLANG_TIDY   ?= clang-tidy
PODMAN       ?= podman

# Passed to the initial cmake configure step. compile_commands.json is needed
# by clang-tidy; the kded6 module flags its own optional deps. Install the
# system-wide -dev packages with `make deps` (scripts/setup-deps.sh) or, in
# the CI workflow, directly via apt.
CMAKE_FLAGS  ?= -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# All C++ sources, used by the format/lint targets.
SOURCES := $(shell find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print)

.DEFAULT_GOAL := help

.PHONY: help configure build test install uninstall lint format format-check tidy clean deps ci check

help:
	@echo "Activity Defaults Switcher — common tasks:"
	@echo ""
	@echo "  make build         Configure (if needed) and build"
	@echo "  make test          Build and run the CTest suite"
	@echo "  make lint          Format check + clang-tidy (skips tools not installed)"
	@echo "  make format        Apply clang-format to src/ and tests/ in place"
	@echo "  make format-check  Fail if clang-format would change anything"
	@echo "  make tidy          Run clang-tidy static analysis"
	@echo "  make install       Build and install to \$${KDE_INSTALL_PLUGINDIR}"
	@echo "  make uninstall     Remove what 'make install' put in place"
	@echo "  make configure     (Re)run the cmake configure step"
	@echo "  make clean         Remove the build directory"
	@echo "  make deps          Install the KDE dev packages system-wide (apt)"
	@echo "  make ci            Run the full gate in a clean Debian container (like CI)"
	@echo "  make check         lint + test (the full gate)"

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)

$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE) --build $(BUILD_DIR) --parallel

install: build
	$(CMAKE) --install $(BUILD_DIR)

uninstall:
	$(CMAKE) --build $(BUILD_DIR) --target uninstall

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || \
		{ echo "error: $(CLANG_FORMAT) not found in PATH" >&2; exit 1; }
	$(CLANG_FORMAT) --style=file -i $(SOURCES)

format-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || \
		{ echo "error: $(CLANG_FORMAT) not found in PATH" >&2; exit 1; }
	$(CLANG_FORMAT) --style=file --dry-run --Werror $(SOURCES)

tidy: $(BUILD_DIR)/compile_commands.json
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || \
		{ echo "error: $(CLANG_TIDY) not found in PATH" >&2; exit 1; }
	$(CLANG_TIDY) -p $(BUILD_DIR) $(SOURCES)

# clang-tidy needs compile_commands.json. An existing build directory created
# before CMAKE_EXPORT_COMPILE_COMMANDS was enabled won't have one, so re-run
# configure once to (re)generate it.
$(BUILD_DIR)/compile_commands.json: $(BUILD_DIR)/CMakeCache.txt
	@test -f $@ || $(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)

# Combined gate: run each lint tool that is installed, and note the ones that
# aren't. Fails only when an available tool reports a problem.
lint:
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		$(MAKE) format-check; \
	else \
		echo "note: $(CLANG_FORMAT) not found in PATH; skipping format check"; \
	fi
	@if command -v $(CLANG_TIDY) >/dev/null 2>&1; then \
		$(MAKE) tidy; \
	else \
		echo "note: $(CLANG_TIDY) not found in PATH; skipping static analysis"; \
	fi

clean:
	rm -rf $(BUILD_DIR)

deps:
	./scripts/setup-deps.sh

# Run exactly what the CI workflow runs, locally, in a clean Debian container.
# The repo is mounted read-only and copied into the container so the local
# (host-path-embedded) build/ never leaks in and skews the result.
ci:
	@command -v $(PODMAN) >/dev/null 2>&1 || \
		{ echo "error: $(PODMAN) not found in PATH" >&2; exit 1; }
	$(PODMAN) run --rm -v "$$(pwd):/src:ro" debian:forky bash -c ' \
		apt-get update -qq && \
		DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
			build-essential cmake git extra-cmake-modules qt6-base-dev \
			libkf6service-dev libkf6config-dev libkf6coreaddons-dev \
			libkf6dbusaddons-dev libplasmaactivities-dev kded6-dev \
			clang-format clang-tidy && \
		cp -r /src /work && cd /work && rm -rf build && \
		make build test lint format-check'

check: lint test
