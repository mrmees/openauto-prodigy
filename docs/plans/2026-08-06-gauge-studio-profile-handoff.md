# Gauge Studio Handoff: Generic Gauge Runtime and Independent Profiles

**Status:** ACTIVE

**Purpose:** standalone input for a future Gauge Studio implementation session.
This document is subordinate to the approved Prodigy design
`2026-08-06-generic-configurable-web-widgets-design.md`, but contains the full
Gauge Studio-facing contract so the target session does not need this chat.

**Upstream anchors:** OpenAuto Prodigy `2506041`, Gauge Studio `f784bf7`, and
`prodigy-obd` `34632d7`.

## Outcome

Gauge Studio must replace its one-exported-widget-per-gauge Prodigy packaging
with:

1. one generic `org.openauto.gauge` web-widget runtime installed once; and
2. independent, ready-to-copy gauge profile directories installed under that
   widget's generic data collection.

Prodigy will then show one **Gauge** picker entry. On add, its generic
full-screen configuration asks the user to choose an installed profile.

## Locked Boundaries

- Gauge Studio normally runs on a laptop, desktop, tablet, or phone, not the
  Raspberry Pi.
- Catalog discovery uses Prodigy's existing paired public External API.
- Trusted manual provider/channel/type/unit entry remains available offline.
- The OBD/CAN backend is not imported, invoked, or read directly.
- Gauge Studio is required by the documented dashboard-gauge workflow, but is
  not a backend code dependency.
- Prodigy does not ship a Gauge runtime or default skin.
- Prodigy does not interpret or validate Gauge documents.
- Transfer is manual. No upload API, installer, marketplace, or signing system
  is in scope.
- A skin is reusable authoring input. A profile is a finished instrument with
  an exact channel binding and a frozen skin snapshot.

## Prodigy Host Contract to Consume

The generic Gauge runtime is a normal web widget at:

```text
~/.openauto/webwidgets/org.openauto.gauge/
```

Its `widget.yaml` must declare one Gauge entry and this configuration:

```yaml
id: org.openauto.gauge
name: Gauge
entry: index.html
category: status
description: Configurable data gauge
icon: ""
size:
  minCols: 1
  minRows: 1
  maxCols: 6
  maxRows: 4
  defaultCols: 2
  defaultRows: 2
configuration:
  configureOnAdd: true
  fields:
    - key: profileId
      label: Gauge Profile
      type: collection
      collection: profiles
      required: true
```

Prodigy persists the selected string as opaque per-instance configuration and
exposes it as:

```js
window.prodigy.config.profileId
```

Later saves replace the full object and emit:

```js
prodigy.on('configchange', function (config) {
  // Rebuild from the complete replacement snapshot.
});
```

The runtime reads its selected profile from:

```text
prodigy://widgets/org.openauto.gauge/__data__/
  profiles/<profileId>/gauge.json
```

## Runtime Package Deliverable

Gauge Studio owns and distributes:

```text
org.openauto.gauge/
  widget.yaml
  index.html
  runtime/
```

The runtime package contains the renderer and Prodigy adapter, but no default
skin, default profile, channel binding, or simulation fallback. The
Prodigy-matching starter skin remains available inside Gauge Studio and is
frozen into profiles that select it.

Required runtime behavior:

- With no profile selected, display `Select Gauge Profile`.
- Load only the selected profile directory.
- Validate the Gauge document before starting a subscription.
- Subscribe only through `window.prodigy.data` using the exact binding stored
  in the profile.
- Pin expected scalar type and normalized unit exactly as current live exports
  do.
- Preserve exact safe-integer, provider-disconnect, channel-unavailable,
  invalid-binding, reconnect, and browser-monotonic staleness behavior.
- On `configchange`, dispose the old adapter/subscription before loading the
  replacement profile.
- Do not silently choose another profile or fall back to simulation.

## Profile Export Deliverable

Export one directory at a time:

```text
engine-rpm/
  item.yaml
  gauge.json
  assets/
  LICENSES/
```

The directory name is the stable profile ID. It uses the same safe identifier
grammar as Prodigy web-widget IDs. `item.yaml` is generic host metadata:

```yaml
id: engine-rpm
name: Engine RPM
description: Optional short text
```

`item.yaml.id` must equal the directory name. `gauge.json` remains Gauge
Studio's domain document and must freeze:

- exact `providerNamespace` and `channelName`;
- expected public `valueType` and normalized `unit`;
- label, minimum, maximum, decimal places, and unavailable text;
- animation and finite monotonic staleness behavior;
- the selected skin snapshot;
- asset references and license closure.

The profile must not contain editor history, drafts, remote asset requests, a
simulation adapter, or a Prodigy top-level `widget.yaml`.

## Authoring Flow

1. Connect Gauge Studio from the authoring device to the Pi's paired External
   API, or select trusted manual binding entry.
2. Choose one compatible numeric channel.
3. Choose or customize a reusable skin.
4. Configure label, range, decimal formatting, staleness, and display behavior
   in Gauge Studio—not in Prodigy.
5. Export the independent profile directory.
6. Copy it manually to:

```text
~/.openauto/widget-data/org.openauto.gauge/profiles/<profile-id>/
```

7. Reopen the Gauge configuration screen to rescan choices. Restart Prodigy
   after replacing or restoring a profile currently in use; reopening
   configuration alone does not reload an active widget.

## Remote Catalog Work

The current Gauge Studio design already has the correct generic catalog and
binding vocabulary. Extend it into a standalone paired client rather than
depending on `window.prodigy` injection from a hosted widget.

The remote client must:

- accept a Prodigy host/address and complete the existing secure-code pairing
  flow;
- store only the credentials required by the existing public API contract;
- list and sort provider catalogs defensively;
- show numeric-compatible double, signed integer, and unsigned integer
  channels;
- keep enum, boolean, and string channels visible but disabled with an
  explanation; enum/status presentation belongs to a future non-gauge widget,
  not this numeric Gauge runtime;
- pin the selected exact provider namespace, channel, type, and unit;
- leave manual binding usable when the Pi or catalog is unavailable.

Direct profile upload is explicitly deferred.

## Existing Fixture Migration

In the Gauge Studio repository, convert:

```text
examples/webwidgets/engine-rpm/
examples/webwidgets/control-module-voltage/
```

into independent profile fixtures. Preserve their proven bindings and runtime
behavior, but remove their top-level Prodigy `widget.yaml` identities. Both
must render through the single generic Gauge runtime.

The existing standalone export path should be removed or clearly deprecated;
normal Gauge Studio export must not recreate one picker entry per profile.

## Errors Owned by the Gauge Runtime

| Condition | Presentation |
|---|---|
| No selected profile | `Select Gauge Profile` |
| Profile directory or document missing | `Gauge Profile Unavailable` |
| Invalid document or asset closure | `Invalid Gauge Profile` |
| Provider disconnected | `Provider Disconnected` |
| Channel absent | `Channel Unavailable` |
| Type or unit mismatch | `Invalid Data Binding` |
| Stale sample | Authored retained/dimmed stale presentation |

Restoring a missing profile with the same stable ID and restarting Prodigy
repairs the saved widget. The runtime must not modify Prodigy dashboard
placement.

## Out of Scope for the Gauge Studio Session

- Changes to `proto/api/` or the provider contract.
- Changes to the OBD/CAN backend.
- Prodigy QML/C++ implementation.
- Direct upload or filesystem mutation on the Pi.
- Hot profile reload.
- Formulas, derived values, multi-channel instruments, or unit guessing.
- A compiled Prodigy plugin.

## Required Tests

- Deterministic profile directory generation.
- Exact `item.yaml` ID/name output and safe profile ID rejection.
- Complete referenced-asset and license closure.
- No `widget.yaml` inside exported profiles.
- No simulation adapter or editor state in live profiles.
- Generic runtime no-selection, missing, invalid, and replacement-profile
  behavior.
- Teardown-before-rebind on `configchange`.
- Exact binding, type/unit mismatch, safe/unsafe integer, availability,
  reconnect, and monotonic staleness coverage.
- Remote catalog and manual entry produce the same canonical binding shape.
- RPM and voltage profiles render through one runtime in an integration fixture.
- `node tests/run-all.mjs` and deterministic generated-source checks remain
  green according to the Gauge Studio repository instructions.

## Cross-Repository Acceptance

The work is complete only when the Pi shows one **Gauge** picker entry, two
instances select RPM and voltage profiles independently, both render live
backend data, resize and persist, report backend disconnection, recover on
reconnect, and preserve a missing profile's placement until the same ID is
restored.
