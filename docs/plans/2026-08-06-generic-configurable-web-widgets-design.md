# Generic Configurable Web Widgets and Gauge Profiles

**Status:** ACTIVE

**Grounded on:** OpenAuto Prodigy `2506041`, Gauge Studio `f784bf7`, and
`prodigy-obd` `34632d7`.

## Decision Summary

Prodigy will gain a source-agnostic configuration lifecycle for dashboard
widgets and a generic read-only data directory owned by each web-widget ID.
Widgets explicitly opt into configuration immediately after placement. The
host renders their declared fields, persists opaque per-instance values, and
exposes the resulting configuration through the public web-widget shim.

Gauge Studio will stop exporting one top-level Prodigy widget for every gauge.
It will instead own and distribute one `org.openauto.gauge` web-widget runtime
plus independent gauge profiles installed under that widget's data directory.
Prodigy will show one **Gauge** picker entry. On add, the user selects one
installed profile. The Gauge runtime—not Prodigy—owns profile interpretation,
data binding, rendering, and gauge-specific failures.

The OBD/CAN backend remains an independent typed-scalar provider. Gauge Studio
is a required workflow step for producing dashboard gauges, not a code
dependency of the backend.

## Problem

The completed live vehicle-data proof exports Engine RPM and Module Voltage as
two standalone web-widget packages. That proves the public provider handshake,
but every additional gauge would add another card to Prodigy's widget picker.
The current web-widget manifest also cannot declare per-instance settings, and
the current compact widget configuration sheet is not suitable for a growing
touch-oriented configuration flow.

The solution must scale beyond gauges. A future weather, camera, GPIO, MQTT, or
other data-backed widget must be able to own configuration choices and private
content without teaching Prodigy the domain semantics of each system.

## Goals

- Keep one picker entry per configurable widget runtime, not per configured
  instance or data channel.
- Give native, plugin-contributed, and web widgets one generic opt-in
  configuration lifecycle.
- Let a web widget own independently copied data collections under a stable
  widget ID.
- Preserve opaque per-instance configuration in the dashboard YAML.
- Keep all OBD, CAN, PID, gauge, skin, range, and formatting semantics outside
  Prodigy core.
- Keep live sample delivery on the existing public `prodigy.data` path at the
  provider's chosen cadence.
- Give the separate Gauge Studio session an implementation-ready package
  contract.

## Non-Goals

- A Gauge Studio editor running on the Pi.
- Direct profile upload, an installer, package signing, a marketplace, or
  remote package management.
- Hot-reloading a profile already displayed by a live gauge.
- A compiled Gauge `IPlugin` or a new plugin ABI.
- Provider-side widget generation or any OBD/CAN-specific Prodigy API.
- Prodigy validation of gauge documents, bindings, ranges, decimal formatting,
  skins, or assets beyond generic path and metadata safety.
- Widget-to-widget communication.
- Formulas, derived channels, multi-channel gauges, or unit conversion.

## Terminology

- **Web widget:** an HTML/JavaScript package discovered from
  `~/.openauto/webwidgets/<widget-id>/` and hosted by `WebWidgetHost`.
- **Widget data:** read-only runtime content under
  `~/.openauto/widget-data/<widget-id>/`, owned semantically by that widget.
- **Collection:** a named, one-directory-deep group of widget-data items.
- **Collection item:** a directory with generic `item.yaml` picker metadata and
  widget-defined content.
- **Gauge skin:** reusable appearance authored in Gauge Studio.
- **Gauge profile:** one finished gauge definition containing an exact channel
  binding, label, range, formatting, frozen skin snapshot, assets, and runtime
  behavior.
- **Gauge runtime:** the single `org.openauto.gauge` web widget distributed by
  Gauge Studio. It is not a compiled Prodigy plugin.

## Architectural Invariants

1. Prodigy hosts configuration but does not assign meaning to opaque widget
   values.
2. A widget must explicitly opt into immediate configuration; a non-configured
   widget keeps the current one-tap placement behavior.
3. Provider cadence, batching, and backpressure remain unchanged. Configuration
   and profile discovery never enter the live sample path.
4. A Gauge profile pins one exact `{providerNamespace, channelName}` plus the
   expected public scalar type and normalized unit.
5. Gauge Studio may browse Prodigy's public catalog remotely or accept trusted
   manual binding input. It never reads backend-private files.
6. Prodigy performs generic structural and canonical-path checks only. The
   owning widget interprets domain files and presents domain errors.
7. Missing widget data never deletes a dashboard placement or silently selects
   a replacement.
8. Existing web-widget manifests remain valid without new fields.
9. No change is required under frozen `proto/api/` for the Prodigy host work.

## System Ownership

### OpenAuto Prodigy

Prodigy owns widget placement, the full-screen configuration shell, generic
field rendering, draft/save/cancel behavior, per-instance persistence,
collection discovery, canonical path confinement, and read-only delivery of
widget-owned data. It exposes the saved configuration to the hosted page.

Prodigy does not ship the Gauge runtime or a default gauge skin. It does not
open or validate `gauge.json`.

### Gauge Studio

Gauge Studio owns the reusable skin library, the Prodigy-matching starter skin,
the generic Gauge runtime package, the Gauge document/profile contract, remote
catalog discovery, trusted manual binding entry, deterministic profile export,
and the conversion of the two current reference gauges.

Gauge Studio normally runs on a laptop, desktop, tablet, or phone. It uses the
Pi network only for public catalog discovery. Profile transfer is manual in
this version.

### Data Backends

The OBD/CAN backend and any future provider own acquisition, decoding,
publication cadence, channel identity, scalar type, unit, availability, and
metadata. They have no dependency on Gauge Studio or the Gauge runtime.

## Generic Widget Configuration Contract

### Descriptor and Manifest Shape

`WidgetDescriptor` gains an additive `configureOnAdd` boolean, defaulting to
`false`. `ConfigFieldType` gains an appended `Collection` value. The schema
field model gains additive `required` and `collection` properties used only by
collection fields.

The web-widget manifest gains an optional `configuration` map:

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

Supported manifest field types initially match the host's existing schema
(`enum`, `bool`, and `intRange`) plus `collection`. This feature does not add
arbitrary widget-supplied executable configuration UI.

Invalid optional configuration metadata does not make the web widget itself
undiscoverable. Prodigy logs the bad fields, omits them from the form, and
disables `configureOnAdd` if no valid fields remain.

### Full-Screen Configuration Lifecycle

The current configuration sheet becomes a full-screen touch surface for every
widget type. Fields are rendered as stacked rows with automotive-sized touch
targets; the shell does not reserve a preview region.

The add sequence is:

1. The picker places the widget at its declared default size.
2. If `configureOnAdd` is true and at least one valid field exists, Prodigy
   opens that instance's configuration immediately.
3. The form edits a draft copied from effective instance configuration.
4. **Save** validates required host-level fields and commits the draft once.
5. **Cancel** discards the draft and leaves the widget placed.

Widgets with useful defaults may expose settings without setting
`configureOnAdd`; they remain one-tap additions and can be configured later.
Required fields prevent Save when empty but never prevent Cancel.

### Persistence and Web Runtime Surface

The existing placement `config` map remains the storage authority. Prodigy
stores the selected collection item as an opaque scalar, for example:

```yaml
config:
  profileId: engine-rpm
```

Hosted pages receive a plain-object snapshot at `window.prodigy.config`.
Saving a later edit replaces the snapshot and emits
`prodigy.on('configchange', callback)`. The callback receives the complete new
snapshot, not a patch. A widget must rebuild any configuration-dependent
runtime state from that replacement snapshot. This is a documented public
web-widget capability; no QWebChannel or private QML side channel is added.

## Widget-Owned Data Contract

### Filesystem Layout

```text
~/.openauto/widget-data/
  <widget-id>/
    <collection-id>/
      <item-id>/
        item.yaml
        ... widget-defined files ...
```

Widget, collection, and item IDs use the existing safe web-widget identifier
grammar. Discovery is exactly one directory level below the declared
collection. Symlinks or canonical paths escaping the owning widget-data root
are rejected.

`item.yaml` has a deliberately small generic contract:

```yaml
id: engine-rpm
name: Engine RPM
description: Optional short text
```

`id` must equal the containing directory name. `name` is required.
`description` is optional and is metadata only. Unknown keys are ignored.
Malformed or escaping items are omitted from choices and logged; they do not
invalidate other items or the owning web widget.

### Collection Choice Semantics

The configuration model resolves collection choices when the form opens. It
sorts them deterministically by case-folded display name and then ID. The saved
value is the item ID; the display label is `name`.

If the saved ID is missing, the form preserves and displays it as
`Missing: <id>` until the user selects an installed item. It never silently
falls back to the first item. An empty collection displays **No items
installed** and leaves a required field unsatisfied.

### Read-Only Web Access

The existing `prodigy://widgets/<widget-id>/` resolver reserves a `__data__`
path segment for the owning widget:

```text
prodigy://widgets/org.openauto.gauge/__data__/
  profiles/engine-rpm/gauge.json
```

The resolver maps this segment to
`~/.openauto/widget-data/org.openauto.gauge/` and applies the same canonical
path jail and read-only scheme behavior as package-local content. The normal
web-widget package may not supply a colliding `__data__` directory. Generic
collection discovery does not make arbitrary local files visible.

This project retains its accepted trusted-local-widget model. The path jail is
a filesystem safety boundary, not a claim that separately installed local web
widgets are mutually hostile principals.

## Gauge Studio Package Contract

### Generic Gauge Runtime

Gauge Studio distributes one installable package:

```text
~/.openauto/webwidgets/org.openauto.gauge/
  widget.yaml
  index.html
  runtime/
```

Its manifest declares one picker entry named **Gauge**, the collection-backed
`profileId` field, and `configureOnAdd: true`. It contains rendering code but
no default skin or default channel binding. Without an installed profile, the
entry remains useful for showing setup state but cannot render a gauge.

### Independent Gauge Profile

Gauge Studio exports one ready-to-copy directory at a time:

```text
engine-rpm/
  item.yaml
  gauge.json
  assets/
  LICENSES/
```

The export is installed by copying the directory into
`~/.openauto/widget-data/org.openauto.gauge/profiles/`. There is no dedicated
installer and no Prodigy semantic validation.

`gauge.json` freezes:

- exact provider namespace and channel name;
- expected public scalar type and normalized unit;
- label, range, decimal formatting, and unavailable text;
- staleness and animation behavior;
- the selected reusable skin snapshot;
- referenced asset identities and licenses.

The generic Gauge runtime owns document/schema validation, asset resolution,
subscription teardown, public data subscription, safe integer handling,
monotonic staleness, and visible failure states.

## Remote Authoring and Manual Transfer

Gauge Studio connects from another device to Prodigy's paired External API to
list the generic data catalog. It may store its paired identity according to
the existing External API contract. Manual namespace, channel, type, and unit
entry remains available when the Pi is offline.

Direct upload is out of scope. The first version exports a directory or archive
that the user manually extracts and copies. A typical directory transfer is:

```bash
scp -r engine-rpm/ \
  matt@prodigy:~/.openauto/widget-data/org.openauto.gauge/profiles/
```

Prodigy is not responsible for completing or repairing a partial copy. The
collection scan ignores incomplete entries without a valid `item.yaml`.
Closing and reopening configuration rescans choices. A Prodigy restart is the
documented recovery path after replacing an active profile.

## Gauge Runtime Data Flow

```text
saved profileId
    -> fetch item-owned gauge.json
    -> Gauge runtime validates document
    -> subscribe exact provider/channel through prodigy.data
    -> render at provider-controlled cadence
```

The runtime does not call backend-private APIs. Prodigy does not poll, coalesce,
throttle, convert, or derive gauge samples beyond the already shipped generic
provider bridge and JavaScript shim behavior.

Changing `profileId` causes the Gauge runtime to stop the old adapter and
subscription before loading and starting the replacement. Repeated instances
may select the same profile. The existing JavaScript shim may share the exact
server subscription among local callbacks; this is transparent to profiles.

## Failure Behavior

| Condition | Owner | Required presentation |
|---|---|---|
| No `profileId` selected | Gauge runtime | `Select Gauge Profile` |
| Selected item directory missing | Gauge runtime | `Gauge Profile Unavailable` |
| Invalid `gauge.json` or assets | Gauge runtime | `Invalid Gauge Profile` |
| Provider disconnected | Existing data adapter | `Provider Disconnected` |
| Channel absent | Existing data adapter | `Channel Unavailable` |
| Type or unit mismatch | Existing data adapter | `Invalid Data Binding` |
| Sample stale | Gauge profile/runtime | Retain/dim/label exactly as authored |

Removing a selected profile never removes or retargets the dashboard widget.
The saved ID survives so restoring that item can repair the instance. Provider
and channel recovery remains automatic through the existing public data shim.
Profile file replacement is not hot-reloaded in version one.

## Migration from the Live Proof

The standalone `Engine RPM` and `Module Voltage` packages are replaceable
reference fixtures, not compatibility commitments. Gauge Studio converts their
documents into two profile directories and supplies the single Gauge runtime.
The old packages are removed from `~/.openauto/webwidgets/`; their existing
dashboard placements are removed and recreated through the **Gauge** entry.

No permanent alias, automatic placement migration, backend change, or External
API change is required.

## Verification Contract

### Prodigy

- Existing web-widget manifests without `configuration` behave unchanged.
- `configureOnAdd` opens configuration only for widgets that request it.
- Save commits one complete config map; Cancel discards the draft.
- Required collection fields block Save while empty but not Cancel.
- Collection order, missing saved IDs, empty collections, malformed metadata,
  symlinks, and traversal attempts have focused coverage.
- Widget-data fetches remain read-only and canonically jailed.
- Initial `prodigy.config` and replacement `configchange` snapshots are tested.
- Full-screen configuration preserves reference-display touch targets.
- Native build, explicit app target, offscreen CTest, and ARM cross-build pass.

### Gauge Studio Handoff

- One deterministic profile directory is exported at a time.
- The runtime package and profiles are separate deliverables.
- RPM and voltage become profiles rather than picker widgets.
- The remote paired catalog and trusted manual entry both produce the same
  pinned binding shape.
- Export closure contains every referenced asset and required notice.
- No profile contains editor state or a simulation fallback.

### Pi Integration

1. Copy the generic Gauge runtime and two profile directories.
2. Confirm the picker contains one **Gauge** entry, not RPM and voltage cards.
3. Add two instances and select a different profile for each.
4. Confirm both render live data at backend-controlled cadence.
5. Confirm resize and per-instance persistence.
6. Stop and restart the backend; both gauges become unavailable and recover.
7. Remove one profile; its placement remains and reports unavailable.
8. Restore the profile and restart or reopen configuration; it renders again.

## Implementation Sequencing

1. Implement and verify only the generic Prodigy host capabilities using a
   repository test web widget and collection fixtures.
2. In a separate Gauge Studio session, implement the runtime/profile exporter
   against the companion handoff document.
3. Convert the two live fixtures and run the Pi integration checklist.
4. Update current reference documentation and archive this design and its
   implementation plan when the cross-repository behavior is accepted.

No Gauge Studio source changes belong in the Prodigy implementation session.

