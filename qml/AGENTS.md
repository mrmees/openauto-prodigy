# qml/ — UI Rules

- **NEVER put pointer handlers over a `WebEngineView`** — they break its input handling. (Standing rule from the web-widget work.)
- **QML ships inside the binary** (qt_add_qml_module + qmlcache): QML changes require a rebuild — on the Pi that means cross-build + binary rsync; a `git pull` there will NOT update the UI.
- **Navbar `MouseArea`s do not own Pi touch input during an AA session** —
  EVIOCGRAB routes touch to `EvdevTouchReader`, while `NavbarController`
  registers the Navbar and popup evdev zones through `EvdevCoordBridge`
  (`src/core/aa/AGENTS.md`).
- Apps open via dashboard launcher widgets; the Navbar is shell control chrome,
  not an app launcher. A new plugin/app needs a launcher widget to be reachable.
