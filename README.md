# Activity Defaults Switcher

A KDE Plasma 6 project that gives each KDE Activity its own default
applications — web browser, email client, text editor, and so on. It has two
parts:

- **Daemon** — an always-on kded6 module that rewrites the system-wide default
  for each component to match the current Activity. Switch to your work
  Activity and links open in the work browser; switch back and they open in
  your personal one.
- **System Settings module (KCM)** — a QML `KQuickConfigModule` to view, edit,
  and reset per-Activity profiles.

## How it works

The profile store (`activity-defaults-switcherrc`, one group per component) is
the single source of truth shared by both parts:

- The **daemon** resolves the current Activity's profile and rewrites the
  system-wide default (`mimeapps.list` for most components, `kdeglobals` for
  the terminal) whenever the Activity changes, a System Settings default is
  edited, or the profile store changes. Every application — native or Flatpak —
  resolves through those files, so one write point covers both.
- The **KCM** (in progress) reads and writes the profile store directly and
  stays a dumb editor; the daemon watches the store and applies edits live
  (see [ADR-0002](docs/adr/0002-kcm-writes-profile-store-daemon-watches.md)).

See the repository glossary (`CONTEXT.md`) for the domain vocabulary and
`docs/adr/` for the design rationale.

## Requirements

- CMake 3.16+
- Extra CMake Modules (ECM)
- A C++20 compiler
- Qt 6 (`Core`, `Test`, QML)
- KF6 (`Service`, `Config`, `CoreAddons`)
- For the kded6 module: KF6 `DBusAddons` and Plasma Activities **dev** packages
- For the KCM: KF6 `KCMUtils` and `KDeclarative` **dev** packages

When the kded6 dev packages are not installed system-wide,
`scripts/setup-deps.sh` stages them under `./deps` (gitignored) without
touching the system.

## Getting started

```sh
make deps    # optional — stages the KDE dev packages not installed system-wide
make build
make test
```

To install the daemon into the running KDE session:

```sh
sudo make install
```

`make install` builds, then runs `cmake --install` into Qt's real plugin
location (`${KDE_INSTALL_PLUGINDIR}/kf6/kded`). Remove it again with
`sudo make uninstall`. For packaging, override the prefix and use `DESTDIR` as
usual:

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
DESTDIR=/tmp/stage make install
```

`make build` configures into `./build` (gitignored) on first use and then builds
incrementally.

## Development tasks

All common tasks are wired up as `make` targets:

| Target | What it does |
| ------ | ------------ |
| `make build`        | Configure (if needed) and build |
| `make test`         | Build, then run the CTest suite |
| `make install`      | Build and install to `${KDE_INSTALL_PLUGINDIR}` |
| `make uninstall`    | Remove what `make install` put in place |
| `make lint`         | Format check + clang-tidy (skips tools not installed) |
| `make format`       | Apply `clang-format` to `src/` and `tests/` in place |
| `make format-check` | Fail if `clang-format` would change anything |
| `make tidy`         | Run `clang-tidy` static analysis |
| `make configure`    | (Re)run the cmake configure step |
| `make clean`        | Remove the `build/` directory |
| `make deps`         | Stage KDE dev packages into `./deps` |
| `make check`        | `lint` + `test` (the full gate) |
| `make help`         | Print the target list (default target) |

`make format`, `make format-check`, and `make tidy` fail with a clear message if
the corresponding tool (`clang-format` / `clang-tidy`) is not in `PATH`. The
combined `make lint` gate instead skips missing tools and only fails when an
installed tool reports a problem.

The build, test, and tool settings are overridable:

```sh
make build BUILD_DIR=build-ninja CMAKE_FLAGS="-G Ninja"
```

## Project layout

```
src/       shared domain (resolver, profile, store, writer), the kded6
           module, and the KCM (in progress)
tests/     Qt Test suites (resolver, writer, switcher, store, module)
docs/adr/  architecture decision records
scripts/   setup-deps.sh (stages the KDE dev packages)
build/     out-of-source CMake build directory (gitignored)
deps/      staged KDE dev packages (gitignored)
```

Install destinations follow KDE convention via `KDEInstallDirs`: the daemon
lands in `${KDE_INSTALL_PLUGINDIR}/kf6/kded`; the KCM will get its own rule
under `${KDE_INSTALL_KCMDIR}` when it lands.
