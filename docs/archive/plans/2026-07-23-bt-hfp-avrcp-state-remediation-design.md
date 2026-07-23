# Bluetooth, HFP, and AVRCP State Remediation — Design

Date: 2026-07-23
Status: COMPLETED 2026-07-23
Base: `origin/main` at `caf368696c39e3ef2954e2492ecfb09944d2b187`

## Goal

Close one consolidated Bluetooth state wave at the boundaries that own paired
and connected-device state, BlueZ pairing prompts, AVRCP player lifecycle, and
PipeWire telephony ownership. The change must keep the Qt main loop responsive,
preserve the Pi's HFP Hands-Free role, and retain existing A2DP EQ/focus and
External API contracts.

## Current-State Revalidation

| Area | Attempted refutation | Result |
|---|---|---|
| Bluetooth manager state | Traced startup enumeration, first-pair confirmation, Device1 properties, interface add/removal, auto-connect cancellation, and API serialization | Accepted. Completed snapshots do not centrally derive connected/first-run state, and interface changes can leave exported state stale |
| D-Bus responsiveness | Traced every managed-object refresh and BT-audio hot-plug property fallback on the Qt thread | Accepted. Repeated blocking calls and interface introspection remain on the GUI thread |
| Pairing agent | Compared exported slots with the installed BlueZ 5.82 `org.bluez.Agent1` and AgentManager documentation | Accepted. The DisplayYesNo agent omits display, authorization, and explicit rejection paths |
| AVRCP removal | Traced player removal, player sender filtering, media arbitration, and existing tests | Accepted. Metadata/time reset while playback can remain Playing indefinitely |
| HFP ownership | Compared the single-AG guard with live read-only PipeWire ObjectManager state for two connected phones | Accepted. Each AG and its transport share one object path, but a rejected second AG's transport is still adopted |
| SCO call inference | Traced call setup, transport, all headset-audio-gateway node edges, and duration/UI state | Accepted. Generic assistant/voice SCO can promote Idle to a phantom Active call |
| Plugin teardown | Traced plugin shutdown ordering and provider signal connections | Accepted. The provider remains connected after `service_` is cleared |

The existing focused tests pass but do not drive these failure paths.

## Design

### 1. Make BlueZ snapshots asynchronous and authoritative

`BluetoothManager` will use an injectable D-Bus connection and one asynchronous
`GetManagedObjects` request owner. Refresh requests arriving while a call is in
flight coalesce into one trailing refresh rather than queueing blocking scans.
The completed snapshot is parsed once and atomically supplies adapter discovery,
the paired-device model, connected device identity, first-run state, and
auto-connect cancellation.

Startup waits for that first snapshot before deciding whether first-run pairing
is needed or auto-connect should begin. Every later Device1 or ObjectManager
change requests the same refresh. A newly paired device clears the first-run
banner and renew timer only when a completed BlueZ snapshot reports it paired;
there is no timing assumption immediately after the agent reply. D-Bus signal
subscription results are checked and logged.

The remaining adapter/agent calls are explicitly bounded and use the injected
connection. This wave does not redesign BlueZ adapter policy or auto-connect
backoff.

### 2. Fulfil the DisplayYesNo agent contract

The exported Agent1 object implements the documented display and authorization
methods in addition to numeric confirmation. Confirmation and just-works
authorization use delayed replies and the existing explicit user decision.
DisplayPinCode/DisplayPasskey publish an information-only prompt and return
immediately; repeated DisplayPasskey calls update the displayed value. Methods
requiring keyboard input are explicitly rejected with a BlueZ error because the
head unit has no text/passkey-entry UX.

`BluetoothManager` exposes a concrete prompt-mode property for the shell without
changing `IBluetoothService` or the plugin ABI. `PairingDialog.qml` presents
Confirm/Reject only for decision prompts and Dismiss for display-only prompts.
Cancel and shutdown clear both modes safely.

### 3. Remove blocking AVRCP hot-plug reads

`BtAudioPlugin` will enumerate existing BlueZ objects asynchronously and apply
the same carried property maps used by `InterfacesAdded`. Device1 aliases are
cached from ObjectManager snapshots/signals. Missing transport/player payloads
remain tracked with unknown state and can be completed by later
`PropertiesChanged`; no synchronous `QDBusInterface` construction or property
read occurs in a hot-plug handler.

Removing the tracked MediaPlayer1 resets metadata, time validity, and playback
to Stopped as one coherent lifecycle transition. Repeated removal remains
silent. Multi-transport activity, sender filtering, controls, EQ tap, and focus
arbitration are unchanged.

### 4. Bind telephony state to one phone

`TelephonyClient` accepts an AudioGatewayTransport1 only when its object path is
the selected AudioGateway path. Snapshot and signal delivery apply AG before
transport for an object, so a rejected second AG cannot replace the first
phone's state source. Foreign transport properties and removal are ignored.

`PhoneStateService` no longer promotes Idle to Active from SCO alone. SCO still
accepts an in-progress setup/settle transition, debounces active-call audio
loss, and participates in the existing end-state logic. Mid-call UI recovery
after an application restart is intentionally not inferred from an ambiguous
assistant-capable SCO link.

`PhonePlugin::shutdown()` disconnects its provider before clearing the pointer,
and its state slot remains null-safe.

## Failure and Notification Semantics

- A failed or malformed managed-object reply retains the last completed state
  and logs once; one trailing refresh may retry if changes arrived in flight.
- Paired/connected/banner signals emit only for observable changes.
- Unsupported keyboard-input pairing methods fail explicitly rather than as
  `UnknownMethod`.
- Display-only pairing prompts never hold a D-Bus reply open.
- AVRCP player loss emits Stopped only when playback was not already stopped.
- A foreign HFP transport cannot change selected-phone availability, codec, or
  transport state.
- SCO without call-setup evidence does not create caller state or a duration
  timer.

## Out of Scope

- Android Auto, either protobuf tree, External API schema, companion changes,
  logging, or release tagging.
- HFP role changes, ofono, `provide-ofono`, custom RFCOMM/AT handling, hold/swap,
  multiparty, or BVRA/assistant feature implementation.
- PipeWire routing, A2DP EQ real-time behavior, focus policy, codec selection,
  or the next audio/equalizer remediation wave.
- Bluetooth daemon restart, re-pairing, forgetting devices, or adding support
  for a new class of pairing hardware during required validation.

## Verification and Live Acceptance Matrix

| Check | Level | Acceptance |
|---|---|---|
| Bluetooth snapshot boundary | Required local | Delayed fake ObjectManager reply leaves an event-loop heartbeat responsive; burst refreshes coalesce; startup and interface-change snapshots derive correct paired/connected/banner/auto-connect state |
| Pairing Agent1 | Required local | Exported object has the documented method surface; confirmation/authorization delay correctly; display-only prompts do not; unsupported input methods return explicit errors |
| AVRCP lifecycle | Required local | Startup and hot-plug use carried properties without blocking reads; player removal coherently resets playback/metadata/time once |
| HFP ownership | Required local | First AG owns its same-path transport; second AG/transport and foreign properties/removal cannot alter it |
| Call and plugin lifecycle | Required local | Idle SCO stays Idle; setup plus SCO becomes Active; existing end/debounce paths remain; post-shutdown provider signals are harmless |
| Repository gates | Required local | Focused tests, full build, explicit app target, full CTest, docs links, diff check, and fully adjudicated review pass |
| aarch64 application | Required before deploy | `./cross-build.sh` succeeds from the reviewed commit |
| Pi health | Required live | Rollback snapshot; one responsive process; hostapd/Bluetooth PIDs and restart counts unchanged |
| Startup/device state | Required live | App restart with already-connected phone(s) immediately reports Bluetooth connected and does not run a redundant connect storm |
| A2DP/AVRCP | Required live | Normal Moto playback remains audible through the EQ tap; time/progress and stopped state remain coherent |
| Multi-phone telephony | Required live, read-only | With both current AG objects present, only the selected AG's transport drives application telephony state |
| Calls, assistant SCO, player switching | Optional/approval-gated phone action | Exercise only if Matthew elects to place a call, invoke assistant, or manipulate the phone media session |
| Re-pair, legacy device, daemon restart | Not required | Automated agent contract is sufficient; do not forget/re-pair or restart Bluetooth for this wave |

Matthew's standing authorization covers scoped Pi snapshot, deployment,
application restart, inspection, and restoration. Phone-originated calls,
assistant use, media-app force-stop, and re-pairing remain separate actions.
