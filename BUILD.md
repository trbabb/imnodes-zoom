# Building

One-liner (configure + build + run tests):

```
cmake --workflow --preset default
```

That's it for a fresh checkout on a properly provisioned machine.

## What you need on the machine

These live outside the source tree and are *not* fetched by `vcpkg`:

| Dependency           | Default path                  | Override                          |
| -------------------- | ----------------------------- | --------------------------------- |
| Dear ImGui sources   | `/usr/local/dearimgui`        | `-DIMGUI_DIR=…`                   |
| imgui_test_engine    | `/usr/local/imgui-test-engine`| `-DIMGUI_TEST_ENGINE_DIR=…`       |
| libpng               | (found via `find_package`)    | normal CMake / pkg-config         |

`SDL2` *is* fetched via the bundled vcpkg submodule, so no system SDL2 is needed.

The `SessionStart` hook in `.claude/settings.json` runs `git submodule update --init --recursive` automatically when a Claude session opens, so the vcpkg submodule is always populated. If you're building outside Claude, run it once yourself.

## Targets

| Target            | What it does                                                       |
| ----------------- | ------------------------------------------------------------------ |
| `hello` / `colornode` / `multieditor` / `saveload` | upstream examples — open an interactive window |
| `render_test`     | Renders the shared 3-node graph headlessly to `imnodes_test.png`. Pass `-i` / `--interactive` to open a window instead; `s` saves a snapshot, `q`/Esc quits. |
| `node_drag_test`  | imgui_test_engine driven: synthesizes a mouse drag on a node, checks the editor-grid position updated, and writes `node_drag_before.png` + `node_drag_after.png`. Returns nonzero on test failure. |

`ctest --preset default` (or the workflow above) runs `node_drag_test`.

## Other presets

- `cmake --preset debug` — Debug build in `build/` (override `binaryDir` if you want both Debug and Release side-by-side).
- `-DIMNODES_TEST_ENGINE=OFF` — skip imgui_test_engine if `/usr/local/imgui-test-engine` is unavailable. Disables `node_drag_test` and reverts imgui to a no-hooks build.
