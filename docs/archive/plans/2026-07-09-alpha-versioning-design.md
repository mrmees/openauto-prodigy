# ALPHA-YY-MM-DD-NN Versioning Scheme — Design

Status: COMPLETED 2026-07-09
Date: 2026-07-09
Grounded: c65d939 (dev)
Approved: Matthew, 2026-07-09 (design + kill `identity.sw_version` + tag on landing)
Reviewed: Codex plan review 2026-07-09 — revisions folded into this design
and the plan (AA `swVersion` stamped too; NN = max+1 semantics; describe
output validated; stale docs swept)

## Problem

The project has no coherent version identity. Five sources disagree:

| Where | Says | Feeds |
|---|---|---|
| `CMakeLists.txt:2` `project(... VERSION 0.1.0)` | 0.1.0 | CMake internals only |
| `src/main.cpp:209` | "0.1.0" | Qt applicationVersion / `--version` CLI |
| `src/core/services/IpcServer.cpp:332` | "0.1.0" | IPC `status` JSON |
| `identity.sw_version` config (default "0.3.0") | whatever config says | External API / web widgets (`ApiServer.cpp:117`) |
| `src/core/aa/ServiceDiscoveryBuilder.cpp:74` | "1" | AA `sw_build` — what the phone logs |

Git tags meanwhile run semver through `v0.6.6`. The comment at
`src/CMakeLists.txt:518` claims `OAP_GIT_HASH` feeds the AA `sw_build` field;
it does not (only `ApiServer` uses it).

## Decision

Adopt **`ALPHA-YY-MM-DD-NN`** (e.g. `ALPHA-26-07-09-01`) until beta.
NN = build number of the day, zero-padded two digits.

- **Tags are the version record.** Annotated git tags, created **only when
  Matthew declares a milestone** — not per deploy, not per build.
- **One derivation, one definition.** CMake derives the string at configure
  time; a single `OAP_VERSION` compile definition feeds every surface.
- At beta, the prefix swaps to `BETA-`: one-line change in the tag script and
  the `--match` pattern.

## Mechanism

### Derivation (top-level `CMakeLists.txt`, beside the existing git-hash block)

```
git describe --tags --match 'ALPHA-*' --dirty
```

- Tagged build → `ALPHA-26-07-09-01`
- N commits later → `ALPHA-26-07-09-01-5-gabc123f`
- Uncommitted tree → `...-dirty` suffix
- No ALPHA tag / git failure → fallback `ALPHA-untagged-<shorthash>`
  (or `ALPHA-untagged-unknown` if git is entirely absent)

Configure-time capture, same trade-off as the existing `OAP_GIT_HASH`:
a rebuild without reconfigure can be stale. Acceptable because tagging is
rare and deliberate; the milestone workflow reconfigures before building.
Cross-build is covered: the Docker container mounts the repo, runs as the
host uid, and has git installed (the `OAP_GIT_HASH` path already works there).

Defined alongside `OAP_GIT_HASH` as a PUBLIC compile definition on
`openauto-core`.

### Surfaces (all unified)

1. `src/main.cpp` — `app.setApplicationVersion(OAP_VERSION)`.
   `parser.addVersionOption()` already exists, so `--version` prints it.
2. `src/core/services/IpcServer.cpp` `handleStatus()` — `OAP_VERSION`
   replaces the hardcoded "0.1.0".
3. `src/core/api/ApiServer.cpp` — `appVersion_ = OAP_VERSION " (" OAP_GIT_HASH ")"`.
   **Stops reading `identity.sw_version`; the config key is removed** —
   config must not be able to lie about which binary is running.
4. `src/core/aa/ServiceDiscoveryBuilder.cpp` — `config.swBuild` AND
   `config.swVersion` both become `OAP_VERSION` (each is serialized into the
   AA ServiceDiscoveryResponse — phone-side logs then identify the real
   build). The Crankshaft-NG match keys (manufacturer/model/year/serial)
   stay untouched. Locked by a new `test_service_discovery_builder` slot.
5. `CMakeLists.txt:2` — `VERSION 0.1.0` stays (CMake requires numeric dotted
   versions; it surfaces nowhere after this change) with a comment pointing
   at the git-derived scheme. Fix the false `sw_build` comment at
   `src/CMakeLists.txt:518`.
6. `qml/applications/settings/SystemSettings.qml` — the Software section's
   "Version" `ReadOnlyField` switches from `configPath: "identity.sw_version"`
   to `value: Qt.application.version` (reflects `setApplicationVersion`,
   i.e. `OAP_VERSION`).

Removing `identity.sw_version` fans out to: `YamlConfig.{cpp,hpp}` (default
write + `swVersion()`/`setSwVersion()` accessors — no production callers,
tests only), `tests/test_yaml_config.cpp`, `tests/data/test_config.yaml`,
`tests/test_config_key_coverage.cpp`, `docs/reference/config-schema.md`.
No config migration needed: `YamlMerge` preserves unknown overlay keys, so a
leftover `sw_version` entry is retained in the file (and re-emitted on save)
but read by nothing; `setValueByPath()` rejects writes to it once it leaves
the defaults schema (locked by a test).

### Tagging workflow — `scripts/tag-alpha.sh`

- Computes `ALPHA-$(date +%y-%m-%d)-NN` where NN = highest existing NN for
  today + 1, zero-padded two digits (grows to three past 99). Deleting the
  day's newest tag frees its number for reuse — never delete a tag that
  shipped.
- Creates an **annotated** tag on HEAD; prints the tag plus reminders:
  push the tag, reconfigure before the milestone build.
- Does NOT push (repo rule: pushes are deliberate).

Flow: Matthew declares a milestone → run script → reconfigure + rebuild →
deploy.

## Documentation changes

- `AGENTS.md`: record the convention (format, milestone-triggered, tag
  script, beta transition).
- `docs/reference/config-schema.md`: remove `identity.sw_version`
  rows/examples; note the removed key under Migration Policy.
- `docs/reference/settings-tree.md`: two rows still cite
  `identity.sw_version` as the displayed version — point at the compiled
  `OAP_VERSION` via `Qt.application.version`.
- `docs/reference/release-packaging.md`: naming convention still defines
  semver `vX.Y.Z` tags as canonical — switch to `ALPHA-YY-MM-DD-NN` with the
  old `v*` tags noted as legacy history.
- `docs/wishlist.md`: delete the "Fix version mismatch" entry — this design
  completes it.

## First tag

The commit landing this scheme is milestone #1 — tag it
`ALPHA-<landing date>-01` once it's on `dev` and the Codex gate passes.

## Verification

- Native build (ext4 build dir): `QT_QPA_PLATFORM=offscreen ./openauto-prodigy --version`
  prints the describe-derived string.
- IPC `status` returns the same string; API `ServerHello` version carries
  `OAP_VERSION (hash)`.
- Cross-build for Pi stamps a real value (not the fallback).
- No tests assert "0.1.0" today (checked 2026-07-09); the sw_version tests
  in `test_yaml_config.cpp` / `test_config_key_coverage.cpp` are removed with
  the key. A new `test_version` asserts `OAP_VERSION` is well-formed.

## Out of scope

- `system-service/openauto_system.py` reports its own component version
  (`"0.1.0"`) — separate Python daemon with no compile step; not an
  app-version surface.
- `libs/prodigy-oaa-protocol` keeps its own numeric CMake project version
  (library-internal, never displayed).
- Any auto-tagging on deploy.
