# Milestones

## v0.4 Logging & Theming (Shipped: 2026-03-01)

**Phases completed:** 2 phases, 9 plans, 0 tasks

**Key accomplishments:**
- Categorized logging infrastructure: 6 subsystem categories, quiet by default, verbose on demand via CLI/config/web panel
- All raw qDebug/qInfo/qWarning calls migrated to categorized macros — zero untagged log calls remain
- Multi-theme support: ThemeService discovers/switches color palettes from bundled + user paths
- DisplayService with 3-tier brightness: sysfs backlight > ddcutil > software dimming overlay
- Wallpaper system: per-theme defaults, user override, settings picker, config persistence
- Gesture overlay brightness control + wallpaper display in shell

---

