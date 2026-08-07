# Generic Configurable Web Widgets Implementation Plan

> **For agentic workers:** use `superpowers:executing-plans` to execute this
> plan task by task. Repository policy keeps one inline implementation owner by
> default. Steps use checkbox (`- [ ]`) syntax for execution tracking.

**Intended implementation author:** Codex, inline. If the user explicitly
reassigns implementation to a Claude-family author, the executor must record
that change and route the final review with `--author claude`; the author label
must always describe who actually wrote the implementation.

**Status:** ACTIVE

**Design:**
`docs/plans/2026-08-06-generic-configurable-web-widgets-design.md`

**Gauge Studio input:**
`docs/plans/2026-08-06-gauge-studio-profile-handoff.md`

**Grounded on:** `6194114`

**Goal:** Give Prodigy a source-agnostic, explicit-opt-in widget configuration
lifecycle and a canonically jailed read-only data directory so one separately
installed web-widget runtime can select among independently installed profiles.

**Architecture:** `WidgetDescriptor` and `widget.yaml` describe generic form
fields. `WidgetDataCatalog` resolves collection choices from
`~/.openauto/widget-data/`, while `WebWidgetContentResolver` reserves
`__data__` for read-only item content. `WidgetGridModel` remains the placement
and persistence authority; the full-screen QML form edits a draft and commits
once. `WebWidgetHost` publishes the saved effective configuration through the
existing public JavaScript shim. Prodigy never reads `gauge.json` and the
provider data path does not change.

**Tech stack:** C++17, Qt 6.8 Core/QML/Quick/WebEngine, yaml-cpp, plain browser
JavaScript, Qt Test, Node `vm`, CMake/CTest, Raspberry Pi 4.

## Global Constraints

- Work on `dev`; do not create a worktree or delegate unless the user asks.
- Do not edit `proto/api/` or `libs/prodigy-oaa-protocol/proto/`; this feature
  needs no protocol change.
- Keep Gauge, OBD, CAN, PID, skin, range, decimals, labels, and channel-binding
  semantics out of Prodigy.
- Keep the existing provider publication, batching, subscription, reconnect,
  and backpressure paths unchanged.
- Keep `ConfigFieldType` additive: append `Collection`; do not reorder the
  existing values.
- Keep old web-widget manifests and native widget descriptors working without
  modification. Optional malformed configuration metadata may remove fields,
  but must not hide an otherwise valid web widget.
- Treat configured values as opaque per-instance scalars. Collection
  membership is presentation metadata, not persistence validity: a safe saved
  ID survives when its item is missing.
- Scan collection choices only when configuration opens. Do not add watchers,
  upload, installation, or hot reload.
- Reserve `__data__` unconditionally. A package-local directory with that name
  is never served through the normal package resolver.
- Retain the trusted-local-widget security model. Canonical confinement and
  symlink rejection protect the filesystem; they do not create hostile-widget
  isolation.
- The configuration screen has no live preview. Save commits once; Cancel/X
  writes nothing and leaves a newly placed widget on the dashboard.
- Implement and prove only generic Prodigy behavior here. Do not copy Gauge
  Studio runtime code, profiles, or default skins into this repository.
- Use one coherent commit per task where practical. Do not push during
  execution.

## Fixed Host Contract

### Manifest syntax

The accepted additive `widget.yaml` shape is:

```yaml
configuration:
  configureOnAdd: true
  fields:
    - key: profileId
      label: Gauge Profile
      type: collection
      collection: profiles
      required: true
```

The parser also accepts the existing host field families for future generic
web widgets:

```yaml
configuration:
  fields:
    - key: style
      label: Style
      type: enum
      options:
        - { label: Compact, value: compact }
        - { label: Detailed, value: detailed }
    - key: showLabel
      label: Show Label
      type: bool
    - key: brightness
      label: Brightness
      type: intRange
      min: 0
      max: 100
      step: 5
```

Keys, collection IDs, and collection item values use
`^[A-Za-z0-9][A-Za-z0-9._-]*$`. Labels are non-empty strings. Enum options have
non-empty labels and scalar string/integer/boolean values; duplicate stored
values invalidate that field. Integer ranges require `min <= max`, `step > 0`,
and a step no larger than the range width unless `min == max`. Duplicate field
keys keep the first valid field and log/drop later duplicates. `required` is
honored only for collection fields in this version.

Each field is parsed independently. A bad field is logged and omitted. A bad
or non-map `configuration` block acts like no configuration. The containing
manifest still passes its existing identity, entry, and size validation.
`configureOnAdd` becomes true only when it was requested and at least one valid
field remains.

### Collection form data

`WidgetGridModel::configSchemaForWidget()` returns the existing field maps and
adds these keys:

```text
required    bool
collection  string
```

For `type == "collection"`, `options` is the deterministically sorted item
display-name list and `values` is the parallel item-ID list. QML compares the
effective saved value with `values` to render `Missing: <id>`; an empty list
renders `No items installed`.

Collection persistence accepts any non-empty safe ID, even when the item is
not currently installed. Host form validity requires a non-empty safe ID only
when `required` is true. It never substitutes the first installed item.

### Public configuration object

`window.prodigy.config` contains widget-authored effective configuration only.
The scanner's host-internal `url` default remains available to
`WebWidgetHost`, but is excluded from the public object and every
`configchange` event.

---

## Task 1: Add the descriptor and manifest configuration contract

**Files:**

- Modify: `src/core/widget/WidgetTypes.hpp`
- Modify: `src/core/widget/WebWidgetManifest.hpp`
- Modify: `src/core/widget/WebWidgetManifest.cpp`
- Modify: `src/core/widget/WebWidgetScanner.cpp`
- Modify: `tests/test_widget_types.cpp`
- Modify: `tests/test_web_widget_manifest.cpp`
- Modify: `tests/test_web_widget_scanner.cpp`

**Produces:** additive in-memory schema metadata and defensive parsing of the
fixed manifest syntax without changing legacy discovery.

- [ ] **Step 1: Pin additive defaults and valid parsing in failing tests**

  Add assertions that a default `WidgetDescriptor` has
  `configureOnAdd == false`, that `Collection` follows `IntRange`, and that a
  legacy manifest produces an empty schema. Add manifest fixtures covering all
  four field types, collection `required`, enum scalar values, range bounds,
  and a requested `configureOnAdd`.

- [ ] **Step 2: Pin partial-failure behavior in failing tests**

  Verify that malformed field nodes, unknown types, empty labels, unsafe keys
  or collection IDs, duplicate keys, duplicate enum values, and invalid ranges
  are omitted individually. The base manifest must remain valid. Verify that
  `configureOnAdd` is forced false when no valid fields survive.

- [ ] **Step 3: Run the focused tests and confirm the contract is absent**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_widget_types test_web_widget_manifest test_web_widget_scanner \
    -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R 'test_(widget_types|web_widget_manifest|web_widget_scanner)$'
  ```

  Expected: compile or assertion failure because collection/configuration
  metadata does not exist.

- [ ] **Step 4: Implement the additive types and defensive parser**

  Append `ConfigFieldType::Collection`. Append `bool required = false` and
  `QString collection` after the existing range members in
  `ConfigSchemaField` so its current aggregate initializers remain valid; add
  `bool configureOnAdd = false` to `WidgetDescriptor`. Mirror the schema and
  flag in `WebWidgetManifest`.

  Parse `configuration.fields` with small type-specific helpers. Catch errors
  per field rather than around the entire optional block. Log the manifest
  path and field key/index for every dropped field without logging arbitrary
  file content. Do not add configuration checks to
  `WebWidgetManifest::isValid()`.

- [ ] **Step 5: Propagate only valid metadata through the scanner**

  Copy `m.configSchema` and the normalized `m.configureOnAdd` into the
  descriptor. Preserve the current internal `defaultConfig.url`, sizing,
  duplicate-ID behavior, and resolver registration.

- [ ] **Step 6: Rerun the focused tests and commit**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_widget_types test_web_widget_manifest test_web_widget_scanner \
    -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R 'test_(widget_types|web_widget_manifest|web_widget_scanner)$'
  git diff --check
  git add src/core/widget tests/test_widget_types.cpp \
    tests/test_web_widget_manifest.cpp tests/test_web_widget_scanner.cpp
  git commit -m "feat(widgets): parse configurable web widget manifests"
  ```

---

## Task 2: Add jailed widget-owned collections and read-only delivery

**Files:**

- Create: `src/core/widget/WidgetDataCatalog.hpp`
- Create: `src/core/widget/WidgetDataCatalog.cpp`
- Modify: `src/core/webwidget/WebWidgetContentResolver.hpp`
- Modify: `src/core/webwidget/WebWidgetContentResolver.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/test_widget_data_catalog.cpp`
- Modify: `tests/test_web_widget_resolver.cpp`
- Modify: `tests/CMakeLists.txt`

**Produces:** generic one-level collection discovery and owner-scoped
`__data__` resolution with no Gauge document knowledge.

- [ ] **Step 1: Write failing collection discovery tests**

  Use `QTemporaryDir` fixtures to prove:

  - direct `<widget>/<collection>/<item>/item.yaml` entries are read;
  - `id` must match the directory and `name` must be non-empty;
  - unknown metadata keys are ignored and description is optional;
  - unsafe IDs, malformed YAML, missing metadata, nested entries, and symlinked
    item directories or metadata files are omitted without hiding siblings;
  - results sort by case-folded name and then stable item ID;
  - missing roots/widgets/collections return an empty list.

- [ ] **Step 2: Write failing `__data__` resolver tests**

  Pin valid JSON/assets, unknown package IDs, missing files, `..`, absolute
  input, external and internal symlink paths, and cross-widget access. Create a
  package-local `__data__/shadow.json` and prove the reserved route never serves
  it. Existing package-local resolution outside `__data__` must remain green.

- [ ] **Step 3: Run the focused tests and confirm failure**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_widget_data_catalog test_web_widget_resolver -j$(nproc)
  ```

  Expected: the catalog target/source and data-root resolver API do not exist.

- [ ] **Step 4: Implement `WidgetDataCatalog`**

  Use these value types and public surface (minor naming adjustments are fine;
  ownership and semantics are not):

  ```cpp
  struct WidgetDataItem {
      QString id;
      QString name;
      QString description;
  };

  class WidgetDataCatalog {
  public:
      explicit WidgetDataCatalog(QString rootPath = {});
      void setRootPath(const QString& rootPath);
      QString rootPath() const;
      QList<WidgetDataItem> items(const QString& widgetId,
                                  const QString& collectionId) const;
      static bool isSafeId(const QString& id);
  };
  ```

  Scan on every `items()` call. Store a cleaned absolute root path even when it
  does not exist yet, then canonicalize it and each candidate at scan time so a
  collection copied after startup appears when configuration is reopened.
  Require safe widget, collection, directory, and metadata IDs. Reject
  symlinked item directories/metadata, and require every canonical result to
  remain below the owning widget-data root. Parse only `id`, `name`, and
  `description` with yaml-cpp. Log and continue on bad items.

- [ ] **Step 5: Reserve and route `__data__`**

  Add `WebWidgetContentResolver::setDataRoot(const QString&)`. In `resolve()`,
  route `__data__/...` to `<data-root>/<registered-widget-id>/...` before normal
  package resolution. Store the cleaned absolute data root and canonicalize it
  per request so a root created after startup is usable. Require the widget ID
  to be registered, reject an empty remainder, reject symlink components, and
  apply canonical containment below that widget's data directory. Do not
  change normal package behavior except that its `__data__` segment is now
  reserved.

- [ ] **Step 6: Register sources, rerun tests, and commit**

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  cmake --build ~/builds/openauto-prodigy \
    --target test_widget_data_catalog test_web_widget_resolver -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R 'test_(widget_data_catalog|web_widget_resolver)$'
  git diff --check
  git add src/core/widget/WidgetDataCatalog.* \
    src/core/webwidget/WebWidgetContentResolver.* src/CMakeLists.txt \
    tests/test_widget_data_catalog.cpp tests/test_web_widget_resolver.cpp \
    tests/CMakeLists.txt
  git commit -m "feat(widgets): serve jailed widget data collections"
  ```

---

## Task 3: Implement draft configuration and configure-on-add

**Files:**

- Modify: `src/ui/WidgetGridModel.hpp`
- Modify: `src/ui/WidgetGridModel.cpp`
- Modify: `src/ui/DashboardManager.hpp`
- Modify: `src/ui/DashboardManager.cpp`
- Modify: `src/main.cpp`
- Modify: `qml/components/WidgetConfigSheet.qml`
- Modify: `qml/applications/home/HomeMenu.qml`
- Modify: `tests/test_widget_grid_model.cpp`
- Modify: `tests/test_dashboard_manager.cpp`
- Create: `tests/test_widget_config_sheet_structure.cpp`
- Modify: `tests/CMakeLists.txt`

**Produces:** a full-screen generic form that rescans collections on open,
supports missing selections, and commits only on explicit Save.

- [ ] **Step 1: Write failing model tests for collection configuration**

  Register descriptors with collection schemas and inject a temporary
  `WidgetDataCatalog`. Assert that `configSchemaForWidget()` returns sorted
  names/IDs plus `required`/`collection`, and that a subsequent call sees newly
  installed items without restarting the model.

  Extend config validation tests so safe missing collection IDs round-trip and
  persist, unsafe/non-string values are dropped, unknown keys remain rejected,
  and required validity distinguishes empty from a safe missing ID.

- [ ] **Step 2: Write failing placement/meta tests**

  Add `placeWidgetAndReturnInstance(...)` tests for success, collision/failure,
  stable existing `placeWidget()` behavior, and instance-ID sequencing. Verify
  `widgetMeta()` reports normalized `configureOnAdd`.

- [ ] **Step 3: Write failing QML structure tests**

  Add a source-contract test following
  `test_settings_menu_structure.cpp`. Pin:

  - full overlay width/height and no 60%-height geometry;
  - a draft object separate from persisted config;
  - exactly one `setWidgetConfig` call in the Save path and none in field
    change handlers;
  - explicit Save and Cancel actions with touch-minimum sizing;
  - collection, missing-item, and empty-collection presentation;
  - Save disabled by the model validity check while Cancel remains enabled;
  - configure-on-add uses the returned instance ID after successful placement.

- [ ] **Step 4: Run focused tests and confirm failure**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_widget_grid_model test_dashboard_manager \
             test_widget_config_sheet_structure -j$(nproc)
  ```

  Expected: new model APIs, collection schema output, and full-screen QML
  contracts do not exist.

- [ ] **Step 5: Wire the catalog without disturbing existing constructors**

  Keep `WidgetGridModel(WidgetRegistry*, QObject*)` source-compatible. Add a
  non-owning `setWidgetDataCatalog(const WidgetDataCatalog*)`. Add the same
  setter to `DashboardManager`; it stores the pointer, updates existing models,
  and applies it to every model created during load or `addDashboard()`.

  In `main.cpp`, create one app-lifetime catalog rooted at
  `~/.openauto/widget-data`, give it to `DashboardManager` before
  `loadFromConfig()`, and give the same root to the web resolver under
  `HAS_WEBENGINE`. Keep collection configuration usable by native/plugin
  widgets even in a build without WebEngine.

- [ ] **Step 6: Add model APIs and validation**

  Preserve the bool `placeWidget()` API and route it plus the new
  `Q_INVOKABLE QString placeWidgetAndReturnInstance(...)` through one private
  placement helper. Return an empty string on refusal.

  Add `configureOnAdd` to `widgetMeta()`. Extend schema serialization for
  `collection`. Add
  `Q_INVOKABLE bool isWidgetConfigValid(const QString& widgetId,
  const QVariantMap& effectiveConfig) const`; it enforces required collection
  presence and safe scalar shape, not current filesystem membership. Extend
  `validateConfig()` so safe collection IDs persist even when absent.

- [ ] **Step 7: Replace live edits with full-screen draft/save/cancel**

  In `WidgetConfigSheet.qml`:

  - rescan schema in `openConfig()`;
  - copy effective values into `draftConfig` and retain the original override
    keys for delta persistence;
  - let controls mutate only the draft;
  - render all fields as vertically stacked automotive-sized rows;
  - add a collection selector popup using `options`/`values`;
  - show `Missing: <id>` when the saved value is absent from choices and
    `No items installed` when no choices exist;
  - make the modal fill `Overlay.overlay`, with no preview or outside-tap
    dismissal;
  - make Cancel/X discard and close;
  - make Save build the override-only map, call `setWidgetConfig()` once, and
    close only after `isWidgetConfigValid()` passes.

  Preserve default-delta behavior for native widgets and the existing enum,
  bool, and integer-range controls.

- [ ] **Step 8: Open configuration after opted-in placement**

  In `HomeMenu.qml`, call `placeWidgetAndReturnInstance()`. Keep the picker
  open and show its existing error if placement fails. On success, close it;
  when `widgetMeta(instanceId).configureOnAdd` and `hasConfigSchema` are true,
  use `Qt.callLater()` to open configuration after the picker has closed.
  Cancel does not remove the placement.

- [ ] **Step 9: Rerun focused tests and commit**

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  cmake --build ~/builds/openauto-prodigy \
    --target test_widget_grid_model test_dashboard_manager \
             test_widget_config_sheet_structure -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R 'test_(widget_grid_model|dashboard_manager|widget_config_sheet_structure)$'
  git diff --check
  git add src/ui src/main.cpp qml/components/WidgetConfigSheet.qml \
    qml/applications/home/HomeMenu.qml tests/test_widget_grid_model.cpp \
    tests/test_dashboard_manager.cpp tests/test_widget_config_sheet_structure.cpp \
    tests/CMakeLists.txt
  git commit -m "feat(widgets): add full-screen draft configuration"
  ```

---

## Task 4: Expose complete configuration snapshots to web widgets

**Files:**

- Modify: `qml/widgets/WebWidgetHost.qml`
- Modify: `resources/web/prodigy.js`
- Modify: `tests/test_prodigy_data_js.mjs`
- Create: `tests/test_web_widget_host_structure.cpp`
- Modify: `tests/CMakeLists.txt`

**Produces:** initial `window.prodigy.config` plus full replacement
`configchange` events over the existing public shim, without page reload or a
private bridge.

- [ ] **Step 1: Write failing JavaScript contract tests**

  Extend the Node `vm` harness with bootstrap configuration. Assert that:

  - `prodigy.config` exists before readiness and contains the initial snapshot;
  - `_updateConfig(next)` replaces rather than merges the object;
  - `configchange` receives the same complete replacement snapshot;
  - omitted/invalid bootstrap configuration normalizes to `{}`;
  - the data API, reconnect behavior, and existing context events remain
    unchanged.

- [ ] **Step 2: Write a failing host source-contract test**

  Pin that `WebWidgetHost.qml` adds public configuration to the bootstrap,
  listens for `effectiveConfigChanged`, pushes `_updateConfig`, excludes the
  internal `url` key, and refreshes both context and configuration after a
  successful crash reload.

- [ ] **Step 3: Run focused tests and confirm failure**

  ```bash
  node tests/test_prodigy_data_js.mjs
  cmake --build ~/builds/openauto-prodigy \
    --target test_web_widget_host_structure -j$(nproc)
  ```

- [ ] **Step 4: Implement shim replacement snapshots**

  Initialize `prodigy.config` from a normalized plain bootstrap object. Add the
  host-only `_updateConfig(next)` method beside `_updateContext`; replace with
  a new shallow plain-object snapshot and emit `configchange` with that whole
  object. Do not expose filesystem access or add External API messages.

- [ ] **Step 5: Push configuration from `WebWidgetHost`**

  Add `publicConfigObject()` that copies `effectiveCfg` except the reserved
  host key `url`. Put it in `bootstrapSource()`, call `pushConfig()` when the
  context's `effectiveConfig` changes, and push after load success. Keep the
  existing lazy activation, gesture sentinel, navigation restrictions, and
  crash retry behavior intact.

- [ ] **Step 6: Rerun focused tests and commit**

  ```bash
  node tests/test_prodigy_data_js.mjs
  cmake --build ~/builds/openauto-prodigy \
    --target test_web_widget_host_structure -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R 'test_(prodigy_data_js|web_widget_host_structure)$'
  git diff --check
  git add qml/widgets/WebWidgetHost.qml resources/web/prodigy.js \
    tests/test_prodigy_data_js.mjs tests/test_web_widget_host_structure.cpp \
    tests/CMakeLists.txt
  git commit -m "feat(webwidgets): publish instance configuration snapshots"
  ```

---

## Task 5: Document, demonstrate, verify, review, and live-test the host

**Files:**

- Create: `examples/webwidgets/configurable-collection/widget.yaml`
- Create: `examples/webwidgets/configurable-collection/index.html`
- Create: `examples/widget-data/com.example.configurable/profiles/sample/item.yaml`
- Create: `examples/widget-data/com.example.configurable/profiles/sample/payload.json`
- Modify: `docs/reference/web-widget-authoring.md`
- Modify: `docs/reference/widget-developer-guide.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/session-handoffs.md`
- On accepted completion, move this plan and its design to
  `docs/archive/plans/` and update `docs/INDEX.md`

**Produces:** a domain-neutral fixture, current author documentation, complete
verification evidence, one bounded major review, and a Pi proof of the generic
host before Gauge Studio consumes it.

- [ ] **Step 1: Add the generic example package and data item**

  Use ID `com.example.configurable`, collection `profiles`, and field
  `profileId`. The page displays the selected item's `payload.json`, listens
  for `configchange`, and visibly reports no selection/missing content. It must
  not mention Gauge, OBD, CAN, providers, or vehicle data.

- [ ] **Step 2: Update public author documentation**

  Replace the statement that configuration schemas are unavailable. Document
  the fixed manifest syntax, draft/save/cancel lifecycle, collection layout,
  `item.yaml`, deterministic/missing behavior, `__data__` URL, restart recovery,
  `prodigy.config`, and `configchange`. Document manual copy commands for the
  example and the trusted-local-widget/path-jail boundary.

  Update the native widget guide for `configureOnAdd`, `Collection`, required
  validation, and the same full-screen host. Keep Gauge-specific details only
  in the design/handoff documents.

- [ ] **Step 3: Run the complete local and ARM gates once on the final tree**

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  cmake --build ~/builds/openauto-prodigy -j$(nproc)
  cmake --build ~/builds/openauto-prodigy \
    --target openauto-prodigy -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure
  python3 scripts/check-doc-links.py --scope tracked-live
  git diff --check
  ./cross-build.sh
  ```

- [ ] **Step 4: Run the one bounded cross-family major review**

  At implementation start, record the immutable feature-base SHA. Reset the
  previous plan-review state only after a current user message explicitly
  authorizes implementation (or explicitly authorizes that reset); cite that
  message in the handoff. The plan text itself is not standing reset
  authorization.

  After the gates are green, use the real implementation author. For the
  intended Codex-owned execution:

  ```bash
  bash scripts/review-gate.sh --author codex --major --base <feature-base-sha>
  ```

  If the user reassigned implementation to Claude/Opus/Fable, do not run the
  command above; use `--author claude --major` so the repository routes the
  review to Codex.

  Adjudicate every finding against a supported production entry point,
  reachable call chain, material impact, and concrete evidence. If fixes are
  required, rerun affected focused tests plus the applicable final gate, make
  one remediation commit, and run the single permitted remediation review.
  Do not start a third pass.

- [ ] **Step 5: Deploy the reviewed ARM binary and generic fixtures**

  ```bash
  rsync -av build-pi/src/openauto-prodigy \
    matt@192.168.1.149:~/openauto-prodigy/build/src/
  ssh matt@192.168.1.149 \
    'mkdir -p ~/.openauto/webwidgets ~/.openauto/widget-data/com.example.configurable/profiles'
  rsync -av examples/webwidgets/configurable-collection/ \
    matt@192.168.1.149:~/.openauto/webwidgets/com.example.configurable/
  rsync -av examples/widget-data/com.example.configurable/profiles/sample/ \
    matt@192.168.1.149:~/.openauto/widget-data/com.example.configurable/profiles/sample/
  ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
  ```

- [ ] **Step 6: Run the touch and recovery checklist**

  1. Confirm one configurable example card appears and opens full-screen
     configuration immediately after placement.
  2. Confirm Cancel leaves the widget placed without a selection; reopen it,
     select `Sample`, Save, and verify the page updates once.
  3. Confirm resize, dashboard persistence, restart persistence, and later
     reconfiguration still work.
  4. Remove the sample directory, reopen configuration, and confirm
     `Missing: sample` with no silent fallback; restore it and restart Prodigy
     to recover.
  5. Temporarily move all items out, confirm `No items installed`, Save disabled,
     and Cancel still available; restore the fixture afterward.

- [ ] **Step 7: Record completion and commit**

  Append the exact local/ARM/live results and review adjudication to
  `docs/session-handoffs.md`. Once the generic host is accepted on hardware,
  mark this plan and the design `COMPLETED 2026-08-06`, move both to
  `docs/archive/plans/`, update `docs/INDEX.md` and the roadmap, and leave the
  Gauge Studio handoff ACTIVE for its separate repository session.

  ```bash
  python3 scripts/check-doc-links.py --scope tracked-live
  git diff --check
  git add examples docs
  git commit -m "docs(webwidgets): publish configurable widget contract"
  ```

## Final Acceptance

Prodigy is ready for the separate Gauge Studio session when all of these are
true:

- legacy and configurable native/web widgets coexist;
- opted-in placement opens a full-screen draft form and Cancel never removes
  the new placement;
- one Save produces one persisted config update and one complete
  `configchange` snapshot;
- collection choices rescan deterministically while safe missing IDs persist;
- `__data__` serves only regular files confined to the registered widget's own
  data root;
- the generic Pi fixture passes the touch/restart/missing/empty checklist;
- native build, explicit app target, CTest, ARM cross-build, and the correctly
  author-routed bounded major review are green;
- no Gauge runtime, profile, skin, backend behavior, or protocol change has
  entered the Prodigy implementation.
