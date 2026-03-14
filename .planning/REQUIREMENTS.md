# Requirements — v0.6.1 Widget Framework & Layout Refinement

## Widget Framework (WF)

- [x] **WF-01**: WidgetDescriptor includes category, description, and icon metadata fields
- [ ] **WF-02**: WidgetGridModel enforces min/max size constraints from WidgetDescriptor as single source of truth
- [ ] **WF-03**: WidgetHost emits opt-in lifecycle signals (created, resized, destroying) observable by widget QML
- [ ] **WF-04**: Widget QML uses grid-span or UiMetrics-based thresholds instead of hardcoded pixel breakpoints

## Grid Layout (GL)

- [ ] **GL-01**: Grid cell dimensions derived from physical mm targets via DPI instead of fixed pixel values
- [ ] **GL-02**: Grid math no longer deducts hardcoded dock height from available space
- [ ] **GL-03**: YAML widget layout includes grid_version field with migration when grid dimensions change

## Navigation (NAV)

- [ ] **NAV-01**: LauncherWidget provides quick-launch tiles (AA, BT Audio, Phone, Settings) as a grid widget
- [ ] **NAV-02**: LauncherDock bottom bar removed from Shell
- [ ] **NAV-03**: LauncherModel data model removed — LauncherWidget uses PluginModel directly

## Documentation (DOC)

- [ ] **DOC-01**: Plugin-widget developer guide covers manifest spec, registration API, lifecycle, and sizing conventions
- [ ] **DOC-02**: Architecture decision records document key design choices for future contributors

## Future Requirements

_None deferred from this milestone._

## Out of Scope

- C++ IWidgetLifecycle interface — QML signals are sufficient; no widget currently needs C++-side lifecycle awareness
- Dynamic plugin loading from external .so — static plugins only for v1.0
- Widget marketplace or remote installation — post-v1.0
- Proportional grid storage (fraction-based positions) — absolute cell indices with migration are sufficient

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| WF-01 | Phase 09 | Complete |
| WF-02 | Phase 11 | Pending |
| WF-03 | Phase 11 | Pending |
| WF-04 | Phase 11 | Pending |
| GL-01 | Phase 09 | Pending |
| GL-02 | Phase 09 | Pending |
| GL-03 | Phase 09 | Pending |
| NAV-01 | Phase 10 | Pending |
| NAV-02 | Phase 10 | Pending |
| NAV-03 | Phase 10 | Pending |
| DOC-01 | Phase 12 | Pending |
| DOC-02 | Phase 12 | Pending |
