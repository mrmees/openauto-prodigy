# qml/ — UI Rules

- **NEVER put pointer handlers over a `WebEngineView`** — they break its input handling. (Standing rule from the web-widget work.)
- **QML ships inside the binary** (qt_add_qml_module + qmlcache): QML changes require a rebuild — on the Pi that means cross-build + binary rsync; a `git pull` there will NOT update the UI.
- **Sidebar `MouseArea`s are visual-only during an AA session** on the Pi — EVIOCGRAB routes all touch to `EvdevTouchReader`, which handles sidebar actions via evdev hit zones (`src/core/aa/AGENTS.md`).
- Apps open via dashboard launcher widgets — there is no nav strip. A new plugin/app needs a launcher widget to be reachable.
