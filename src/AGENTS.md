# src/ — Qt / D-Bus / PipeWire Gotchas

Subsystem rules for C++ code under `src/`. Root `AGENTS.md` holds the hard constraints and workflow; this file holds the build/runtime traps that have actually bitten this codebase.

## Qt

- **QTimer needs `#include <QTimer>`** — a forward declaration alone produces "not declared in scope" errors.
- **QTimer follows QObject thread affinity and needs a running Qt event loop.**
  Start and stop it from the timer's owning thread; marshal cross-thread work to
  that owner with `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
- **Q_OBJECT in a header-only class needs a .cpp file listed in CMakeLists.txt** — otherwise MOC never runs and you get undefined vtable references.
- **`QColor` needs `Qt6::Gui`** in the link line, not `Qt6::Core`.
- **QVideoFrame is ref-counted — use a fresh wrapper for each decoded output.**
  Backing storage may be recycled by `VideoFramePool` only when its generation,
  capacity, and weak return-state lifetime still match the current pool.

## D-Bus

- **`QDBusArgument::operator>>` cannot extract `QVariantMap` directly.** Use manual `beginMap()`/`endMap()` with `QDBusVariant` for property values.

## PipeWire

- **Raw auxiliary PipeWire handles require an owner-driven stop edge.** Before
  starting an object that retains `AudioService` loop/core/proxy handles,
  connect `AudioService::aboutToDestroyPipeWire` to its stop method with a
  direct connection. QObject child order and `aboutToQuit` do not cover every
  teardown path.
- **Playback: always fill and publish the full requested period.** Compute `wantBytes` from `buf->requested` (bounded by `d.maxsize` — that is buffer *capacity*, not the request) and silence-fill any gap; PipeWire's resampler sets `requested` to exactly what it needs. Variable short `chunk->size` values cause tempo wobble ("skippy" audio).
- **`SPA_DICT_INIT_ARRAY` inline syntax** triggers "taking address of temporary array" — use named `spa_dict_item` arrays with `SPA_DICT_INIT` instead.
