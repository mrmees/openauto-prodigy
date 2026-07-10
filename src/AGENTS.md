# src/ — Qt / D-Bus / PipeWire Gotchas

Subsystem rules for C++ code under `src/`. Root `AGENTS.md` holds the hard constraints and workflow; this file holds the build/runtime traps that have actually bitten this codebase.

## Qt

- **QTimer needs `#include <QTimer>`** — a forward declaration alone produces "not declared in scope" errors.
- **QTimer only works on threads with a Qt event loop.** Starting a QTimer from a Boost.ASIO thread silently does nothing. Marshal to the main thread with `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
- **Q_OBJECT in a header-only class needs a .cpp file listed in CMakeLists.txt** — otherwise MOC never runs and you get undefined vtable references.
- **`QColor` needs `Qt6::Gui`** in the link line, not `Qt6::Core`.
- **QVideoFrame is ref-counted — do NOT reuse frame buffers.** Allocate fresh frames each decode (see `VideoFramePool`).

## D-Bus

- **`QDBusArgument::operator>>` cannot extract `QVariantMap` directly.** Use manual `beginMap()`/`endMap()` with `QDBusVariant` for property values.

## PipeWire

- **Playback: always output full periods.** Set `d.chunk->size = maxSize` and silence-fill any gap. PipeWire's graph timing is fixed by quantum/rate; variable `chunk->size` values cause tempo wobble ("skippy" audio).
- **`SPA_DICT_INIT_ARRAY` inline syntax** triggers "taking address of temporary array" — use named `spa_dict_item` arrays with `SPA_DICT_INIT` instead.
