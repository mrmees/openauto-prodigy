# Domain Pitfalls

**Domain:** Automotive-minimal UI redesign and settings reorganization for Qt 6 QML app on Raspberry Pi 4 (1024x600)
**Researched:** 2026-03-02
**Confidence:** MEDIUM-HIGH (Qt official docs for performance/API, codebase analysis for project-specific issues, community sources for Pi 4 GPU behavior)

## Critical Pitfalls

### Pitfall 1: StackView Transition Animations Kill Frame Rate on Pi 4

**What goes wrong:** You add slide/fade transitions to the settings StackView navigation (push/pop between category list and sub-pages). On the dev VM it looks smooth. On the Pi 4 with VideoCore VI, transitions drop to 15-20 fps during the animation, producing a visible stutter that feels broken in a car UI. Worse: if AA video is decoding in the background (user navigated to settings mid-session), the transition competes for GPU bandwidth and both stutter.

**Why it happens:** StackView's default push/pop transitions involve simultaneous rendering of TWO full-screen items (the outgoing and incoming pages) with animated opacity and/or x-position. This means the GPU must:
1. Render the outgoing page to a texture (layer caching)
2. Render the incoming page to a texture
3. Composite both with blending

VideoCore VI on Pi 4 has limited fill rate. At 1024x600, two full-screen blended layers = ~1.2M pixels/frame of blended content. Add the existing TopBar, NavStrip, and wallpaper layers, and you're pushing past the GPU's comfortable throughput for 60fps.

**Consequences:** Janky settings navigation that makes the whole app feel cheap. Users will associate the stutter with poor quality even though the rest of the app runs fine.

**Prevention:**
1. **Use instant transitions or very short ones (80-100ms max).** The existing Sidebar.qml animations use 80ms -- stick with that ceiling. The car context actually benefits from instant: users want to see the page NOW, not watch it slide in.
2. **Prefer opacity-only transitions over slide transitions.** Sliding requires both pages to be rendered simultaneously at different x-positions. A simple opacity crossfade (fade out old at the same position, fade in new) is cheaper because the compositor can partially skip the hidden page once its opacity drops below a threshold.
3. **Set `StackView.pushEnter/pushExit` to null for instant transitions.** This is the nuclear option but the safest for Pi 4:
   ```qml
   StackView {
       pushEnter: null
       pushExit: null
       popEnter: null
       popExit: null
   }
   ```
4. **Test transitions on Pi 4 FIRST, not on the dev VM.** The VM has no GPU -- it software-renders everything, which ironically can be slower than Pi 4 for some things and faster for others. Neither is predictive of the other.
5. **Never use `StackView.Transition` (deprecated) -- use the named transition properties directly.**

**Detection:** Use `QSG_RENDER_TIMING=1` environment variable on Pi to log frame render times. Any frame exceeding 16ms during a transition = visible jank.

**Phase:** Address immediately when building the new settings navigation structure. Don't add fancy transitions first and tune later -- start with null/instant and only add animation if Pi 4 testing proves it's smooth.

---

### Pitfall 2: layer.enabled Cascading GPU Memory Exhaustion

**What goes wrong:** You add `layer.enabled: true` to controls for visual effects (rounded corners with clip, drop shadows, blur behind panels, opacity animations on complex subtrees). Each layered item allocates a GPU texture of `width x height x 4 bytes`. A few layered panels at 400x300 = 480KB each. But if you layer the settings category grid (1024x500+), the AA video surface, and a modal dialog simultaneously, you've allocated 6-8MB of GPU textures on a device with limited VRAM shared with system RAM.

**Why it happens:** `layer.enabled` is the go-to solution for several visual needs: applying opacity to a subtree, enabling `layer.effect` for shaders, caching complex item trees. It LOOKS like an optimization (cache the rendered tree), but Qt's own documentation explicitly warns: "the overhead of rendering to an offscreen and the blending involved with drawing the resulting texture is often more costly than simply letting the item and its children be drawn normally."

**Consequences:** GPU memory pressure causes texture eviction, leading to visual corruption or frame drops. On Pi 4 with 1GB RAM (GPU and CPU share the same pool), this directly competes with FFmpeg decode buffers and PipeWire audio.

**Prevention:**
1. **Do not use `layer.enabled` at all in this milestone.** The current codebase has zero `layer.enabled` usage and the UI works fine. Maintain this.
2. **For rounded corners:** Use `Rectangle` with `radius` property (GPU-native, no layer needed). The codebase already does this everywhere.
3. **For opacity on subtrees:** Animate individual child opacities instead of layering the parent. More verbose but no texture allocation.
4. **For clipping:** `clip: true` on Flickable/ListView is necessary and acceptable. Do NOT add `clip: true` inside delegates -- Qt docs say "clipping inside a delegate is especially bad and should be avoided at all costs."
5. **If you absolutely must layer:** Set `layer.enabled` only during the animation, then disable it. Use `layer.textureSize` to limit resolution if the visual doesn't need full-res.

**Detection:** `QSG_RENDERER_DEBUG=render` logs texture allocations. Watch for texture count/size growth when navigating settings.

**Phase:** Enforce as a rule throughout the entire UI redesign. Add to code review checklist.

---

### Pitfall 3: EQ Dual-Access Pattern Creates Two Component Instances with Divergent State

**What goes wrong:** EQ is accessible from two places: Audio > EQ subsection in settings, and an EQ shortcut on the NavStrip. Both load `EqSettings.qml`. User opens EQ via NavStrip, adjusts sliders, then navigates to Settings > Audio > EQ and sees the OLD state because a new `EqSettings` instance was created with fresh local QML state (`currentStream`, `currentBypassed`, `presetLabel`).

**Why it happens:** StackView's `push()` with a Component creates a NEW instance every time. The `EqSettings.qml` has local properties (`property int currentStream: 0`, `property bool currentBypassed: false`) that are initialized fresh on each instantiation. The underlying `EqualizerService` C++ singleton has the correct authoritative state, but the QML instance's local state (which stream tab is selected, what the preset label shows) is duplicated and not synchronized.

**Consequences:** User confusion -- they adjust EQ in one place, navigate to the other, and it looks different. Worse: if both instances exist simultaneously (StackView keeps the old one alive while transition occurs), signal connections to `EqualizerService` fire on BOTH instances, potentially causing double-apply issues.

**Prevention:**
1. **Single EqSettings instance, loaded into a Loader, parented to Shell.qml or the settings area.** Both navigation paths show/hide the same instance rather than creating new ones.
2. **Alternatively, make ALL visual state in EqSettings derived from EqualizerService.** Replace local `currentStream` property with a C++-side `EqualizerService.activeUiStream` property. Replace the preset label local logic with a bound property: `text: EqualizerService.activePresetForStream(EqualizerService.activeUiStream)`. This way, any instance will show consistent state because it's all driven from the singleton.
3. **`Component.onCompleted: refreshSliders()` is your safety net** -- it re-reads authoritative state on creation. This already exists and handles the "new instance shows correct gains" case. The gap is UI-only state like which stream tab is selected.
4. **Test: Open EQ from NavStrip, switch to Nav stream, go back, open EQ from Settings. Verify Nav stream is still selected.** This will fail unless you implement option 1 or 2.

**Detection:** Navigate between the two EQ entry points and verify visual consistency. Automated: compare `EqualizerService.activePresetForStream(0)` to what the preset label actually displays.

**Phase:** Address during the settings reorganization phase when defining the EQ's new navigation structure. The architecture decision (shared instance vs. derived state) must be made before implementation.

---

## Moderate Pitfalls

### Pitfall 4: Qt 6.4 vs 6.8 QML Behavioral Differences Break Dev/Target Parity

**What goes wrong:** You implement the redesign on the Ubuntu dev VM (Qt 6.4), push to Pi (Qt 6.8), and discover visual or behavioral differences that weren't caught during development. Specific known divergences:

1. **`font.contextFontMerging` and `font.preferTypoLineMetrics`** -- new in Qt 6.7/6.8. If you accidentally use these (e.g., from a Qt 6.8 example), the QML fails to parse on Qt 6.4.
2. **Text rendering algorithm differences** -- Qt 6.7 introduced CurveRendering for Text. Qt 6.4 uses only QtRendering (distance field) or NativeRendering. Font glyph metrics may differ slightly between versions, causing text truncation or misalignment.
3. **Popup `popupType` property** -- new in Qt 6.8. Not available in 6.4.
4. **`Image.retainWhileLoading`** -- new in Qt 6.8. Using it on Qt 6.4 produces a QML warning and is ignored.
5. **Dialog signal ordering** -- `accepted()`/`rejected()` are now emitted before `closed()` in newer Qt 6. Code that depends on signal ordering may behave differently.
6. **StackView internal changes** -- some push() overloads were added in Qt 6.7. The basic `push(Component)` signature works on both.

**Why it happens:** The project MUST support both Qt versions (6.4 for dev, 6.8 for target). The CLAUDE.md already documents `loadFromModule()` as a known compat issue. But as the UI gets more complex, the surface area for version-specific behavior grows.

**Prevention:**
1. **Stick to Qt 6.4-era APIs exclusively.** Before using any QML property or type, verify it exists in the Qt 6.4 docs, not just 6.8.
2. **Test the EXACT same QML on both platforms after every significant UI change.** Don't batch up changes and discover 5 compat issues at once.
3. **Create a compat checklist of known-forbidden APIs:**
   - `loadFromModule()` -- already documented
   - `font.contextFontMerging` / `font.preferTypoLineMetrics`
   - `Popup.popupType`
   - `Image.retainWhileLoading`
   - `Text.renderType: Text.CurveRendering`
   - Any API marked "since Qt 6.5/6.6/6.7/6.8" in the docs
4. **Pin a comment in Shell.qml or UiMetrics.qml listing the minimum Qt version:** `// Minimum Qt version: 6.4 (Ubuntu 24.04). Do not use APIs introduced after Qt 6.4.`

**Detection:** Build and run on both platforms after each UI component is redesigned. The dev VM build will catch missing API errors as QML warnings at startup.

**Phase:** Ongoing throughout the entire milestone. Every QML change needs dual-platform validation.

---

### Pitfall 5: Icon Font Inconsistency -- Two Different Font References for Material Symbols

**What goes wrong:** The codebase currently has TWO different ways to reference Material icons, and they can produce different results or silently fail:

1. `MaterialIcon.qml` uses `FontLoader { source: "qrc:/fonts/MaterialSymbolsOutlined.ttf" }` and references `materialFont.name`
2. `EqSettings.qml` (and potentially new code) uses `font.family: "Material Icons"` as a hardcoded string

If the font's internal family name doesn't match `"Material Icons"` exactly (Material Symbols Outlined fonts register as `"Material Symbols Outlined"`, not `"Material Icons"`), the hardcoded references will fall back to the system default font, rendering garbled text instead of icons.

**Why it happens:** The `MaterialIcon` component was built correctly with a FontLoader. But when writing inline icon text (e.g., in EqSettings dialogs), developers bypassed the component and used raw `Text` with a guessed font.family name. On one platform this might work (font name resolved correctly), on another it fails silently.

**Consequences:** Missing or garbled icons on one platform but not the other. Hard to debug because the Text element doesn't error -- it just renders the codepoint as a regular character in whatever fallback font the system picks.

**Prevention:**
1. **Fix all `font.family: "Material Icons"` references to use `MaterialIcon` component instead.** Search for `font.family.*Material` in the QML directory -- currently 5 instances in EqSettings.qml.
2. **Alternatively, expose the font family name from a single source.** Make `UiMetrics` or a `FontConstants` singleton expose `readonly property string iconFontFamily: materialFont.name` from a FontLoader, and use that everywhere.
3. **During the UI redesign, mandate: all icons go through `MaterialIcon.qml` or the shared font family property.** No inline font.family references.
4. **Add to the visual QA pass: verify every icon renders correctly on both Qt 6.4 and 6.8.**

**Detection:** Grep for `font.family.*[Mm]aterial` in the QML tree. Any hit that isn't inside `MaterialIcon.qml` is a bug.

**Phase:** Fix existing instances during the visual redesign pass. Enforce the pattern for all new code.

---

### Pitfall 6: Settings State Lost on StackView Pop/Re-Push

**What goes wrong:** User navigates to Settings > Audio, adjusts master volume slider, goes back to settings list, then navigates to Settings > Audio again. The slider resets to the config-file value instead of showing the value they just set. Or worse: they're mid-scroll in a long settings page, go back, re-enter, and they're at the top again.

**Why it happens:** The current StackView uses `Component` references (`Component { id: audioPage; AudioSettings {} }`). When you `push(audioPage)`, a new instance is created. When you `pop()`, the old instance is destroyed (StackView's default `destroyOnPop: true` behavior). Re-pushing creates a fresh instance with `Component.onCompleted` re-running.

For settings that bind to `ConfigService`, this is mostly fine -- the slider will read the current config value, which was persisted by the previous instance. But:
- Scroll position is lost (always returns to top)
- Transient UI state (expanded sections, selected tabs) is lost
- Any unsaved changes are lost (if implementing a save-on-exit pattern)

**Consequences:** Annoying UX -- user feels like the app forgets their place every time they navigate. In a car, losing scroll position means more taps to get back to where they were.

**Prevention:**
1. **For the current architecture, this is acceptable IF all settings persist immediately.** The codebase already does this -- sliders call `ConfigService.setValue()` + `ConfigService.save()` on change. Re-creation reads back the correct values.
2. **For scroll position:** Consider using `StackView { destroyOnPop: false }` (available since Qt 6.x but NOT in Qt 5). This keeps instances alive when popped, preserving scroll position and transient state. **WARNING: verify this property exists in Qt 6.4.** If not, accept scroll position loss as a minor UX cost.
3. **Alternative: persistent Loader items instead of StackView.** Load all settings pages as `Loader { active: false }` children, toggle `active` for the visible one. No destruction/recreation. More memory usage (all pages stay in memory) but trivial for settings-weight QML.
4. **For the EQ page specifically:** The stream tab selection and scroll position matter more because EQ interaction is iterative (adjust, listen, re-adjust). Use option 1 (shared instance) from Pitfall 3.

**Detection:** Navigate to a settings page, scroll down, go back, re-enter. Is scroll position preserved? Adjust a slider, go back, re-enter. Is the value correct?

**Phase:** Decide during settings architecture phase whether to use StackView, persistent Loaders, or SwipeView for the new category navigation.

---

### Pitfall 7: EVIOCGRAB + New Touch UX Creates Invisible Interaction Gaps

**What goes wrong:** You redesign touch interactions -- adding press-and-hold menus, swipe-to-dismiss, drag handles, long-press context menus. They work beautifully on the launcher and settings. Then during AA, none of them work because EVIOCGRAB routes ALL touch to the evdev reader, and the QML MouseAreas/touch handlers never receive events. The user doesn't understand why the same gesture works in one context but not another.

**Why it happens:** The existing architecture handles this correctly for the AA sidebar (visual-only QML, C++ evdev hit zones). But the NEW touch interactions being added in this milestone may inadvertently rely on QML touch handling that simply doesn't exist during AA. Examples:
- Button press feedback animations (color change on press) won't trigger during AA
- Swipe-to-dismiss on the gesture overlay won't work if the overlay relies on QML Flickable
- Any new modal dismiss-on-tap-outside won't work during AA

**Consequences:** Inconsistent UX between AA-active and AA-inactive states. Users try interactions that worked moments ago and they don't respond.

**Prevention:**
1. **Classify every new touch interaction as "works during AA" or "settings-only."** If it's settings-only, it doesn't matter (settings can't be open during AA fullscreen). If it's visible during AA (overlay, sidebar, gesture area), it MUST use the evdev path.
2. **The 3-finger gesture overlay and sidebar already have evdev hit zones.** Any new interactive elements in those areas must be added to the evdev hit zone map, not as QML MouseAreas.
3. **During AA, don't show interactive UI elements that can't respond to touch.** If the gesture overlay has new buttons, they need C++ evdev handling or shouldn't appear.
4. **Document the boundary clearly:** QML touch works = when `eviocGrabActive == false` (no AA connection). Evdev touch works = always. Design accordingly.

**Detection:** Test every new touch interaction with AA connected and projecting video. If it doesn't respond, it's in the EVIOCGRAB gap.

**Phase:** Design-time consideration. When specifying new touch interactions, tag each one with its touch input path.

---

### Pitfall 8: Animation During Video Decode Causes Frame Competition

**What goes wrong:** User opens the gesture overlay (3-finger tap) while AA video is playing. The overlay has a fade-in animation. During the 200ms fade, both the video decode output AND the overlay opacity animation are trying to update the scene graph every frame. The video frame rate drops visibly, and the overlay animation stutters.

**Why it happens:** Qt's scene graph renderer processes all visual updates in a single render pass. When an animation is running, it triggers scene graph updates every frame (~16ms). Video frames arriving from the FFmpeg decode thread also trigger scene graph updates via `QVideoSink::setVideoFrame()`. On Pi 4, the compositor has limited bandwidth, and rendering a changing video texture PLUS animating overlay elements can exceed the frame budget.

**Consequences:** Video stutters when overlay appears/disappears. The animation looks bad because it's competing with video. This is the ONE context where animations actually hurt perceived quality rather than helping.

**Prevention:**
1. **Keep overlay animations extremely short (80ms or less) or instant.** The existing sidebar animations use 80ms -- this is the proven safe ceiling.
2. **Use `OpacityAnimator` or `XAnimator` instead of `Behavior on opacity`.** The `*Animator` types run on the render thread and don't require scene graph re-processing from the GUI thread. Available since Qt 5.2, safe for Qt 6.4.
3. **Avoid concurrent opacity animation + position animation on the same item.** Each animator type that touches a different property can trigger separate render passes.
4. **During AA video playback, consider disabling all transitions entirely.** Check `AndroidAutoService.connected` and skip animations when AA is active:
   ```qml
   Behavior on opacity {
       enabled: !aaService.connected  // or: duration based on connection state
       NumberAnimation { duration: 150 }
   }
   ```

**Detection:** Open gesture overlay during AA video playback. Watch for frame drops in both video and overlay animation. Use `QSG_RENDER_TIMING=1`.

**Phase:** Test during the animation implementation phase. Adjust durations based on Pi 4 testing with active AA video.

---

## Minor Pitfalls

### Pitfall 9: Icon Size Inconsistency at 1024x600 DPI

**What goes wrong:** Material Symbols Outlined icons are designed for standard screen DPI (~160dpi). The 1024x600 DFRobot touchscreen is ~7 inches diagonal, giving roughly 170 dpi. Icons look fine. But the `UiMetrics` scale factor (`shortest / 600`) evaluates to 1.0, meaning icons render at their design size. On this screen that's physically ~5mm for a 24px icon, which is at the lower bound of readability in a car at arm's length (~60cm viewing distance).

**Prevention:**
1. **Use `UiMetrics.iconSize` (36px scaled) as the MINIMUM for navigation icons, not 24px.** The current NavStrip already uses `parent.height * 0.5` which resolves to ~36px. New icons should follow this convention.
2. **For icon-only buttons (no label), increase to `UiMetrics.iconSize * 1.2`.** Without a label for context, the icon itself needs to be more recognizable.
3. **Test icon legibility at arm's length.** Hold the Pi screen 60cm away and verify every icon is identifiable without squinting.
4. **Material Symbols Outlined `FILL` axis:** The font supports variable fill (0=outlined, 1=filled). Filled icons are more readable at small sizes in a car. Consider using filled variants for nav strip icons. This requires the variable font file, which may differ from the currently bundled static font.

**Detection:** Visual inspection on the actual Pi display at a realistic mounting distance.

**Phase:** During the iconography pass. Choose icon sizes and styles before implementing across all views.

---

### Pitfall 10: Settings Reorganization Breaks Existing Config Paths

**What goes wrong:** Moving settings from their current flat categories (Display, Audio, Connection, Video, System) into new categories (Android Auto, Display, Audio+EQ, Connectivity, Companion, System/About) tempts you to reorganize the YAML config paths to match the new UI structure. You rename `video.resolution` to `android_auto.video.resolution`. Existing installations break because `ConfigService` can't find values at the old paths.

**Why it happens:** Clean organization instinct. The UI hierarchy and the config hierarchy feel like they should match. But config paths are a persistence API -- changing them is a breaking migration.

**Prevention:**
1. **Do NOT change config YAML paths to match UI categories.** The UI structure and config structure are independent. Settings UI can group `video.*`, `connection.*`, and `android_auto.*` config keys under an "Android Auto" UI category without moving the config keys.
2. **If you must restructure config paths, add migration code:** On startup, check for old paths, copy values to new paths, delete old paths. But this is unnecessary complexity for a UI reskin.
3. **The SettingsMenu.qml already maps pageId to Components -- the indirection exists.** Reorganizing which `pageId` maps to which Component, and which Components contain which config-bound controls, does not require config path changes.

**Detection:** After settings reorganization, check that `~/.openauto/config.yaml` still has the same key paths. Install fresh, configure everything, then apply the update -- verify nothing reverts to defaults.

**Phase:** Address during the settings architecture phase. Establish the rule early: UI categories != config paths.

---

### Pitfall 11: Hardcoded Pixel Values in New QML Components

**What goes wrong:** New components use hardcoded pixel values (`width: 200`, `margin: 16`, `font.pixelSize: 20`) instead of `UiMetrics` references. On the 1024x600 target they look fine. But the scale factor system exists for a reason -- if the display resolution changes (future hardware), everything hardcoded breaks.

**Why it happens:** UiMetrics requires importing the singleton and using property references, which is more verbose than a raw number. Under time pressure, developers shortcut.

**Prevention:**
1. **The existing codebase is already mostly consistent with UiMetrics** -- maintain this. The Tile.qml hardcodes `radius: 8` and `spacing: 8` which should be `UiMetrics.radius` and `UiMetrics.spacing`, but this is the exception.
2. **For the redesign: every spacing, margin, font size, touch target, and radius must use UiMetrics.** No raw pixel values except for 0 and 1 (zero spacing, 1px divider lines).
3. **Add any new metrics needed by the redesign to UiMetrics.qml** (e.g., `gridGap` is already there; add `categoryTileSize`, `iconLarge`, etc. as needed).

**Detection:** Grep QML files for bare number assignments to size-related properties. Any `width:`, `height:`, `font.pixelSize:`, `spacing:`, `margin*:` with a raw number > 1 is suspect.

**Phase:** Enforce from the start of implementation. Easier to do right the first time than fix later.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Settings StackView restructure | State loss on pop/re-push (Pitfall 6) | Decide architecture: StackView vs Loaders vs destroyOnPop |
| Settings StackView restructure | Config path breakage (Pitfall 10) | UI categories independent of config paths |
| StackView transitions | GPU frame drops on Pi 4 (Pitfall 1) | Start with null transitions, add only if Pi 4 handles it |
| EQ dual-access pattern | Divergent state between instances (Pitfall 3) | Shared instance or fully C++-derived state |
| Visual redesign (controls) | layer.enabled temptation (Pitfall 2) | Ban layer.enabled entirely; use Rectangle radius instead |
| Visual redesign (controls) | Hardcoded pixels (Pitfall 11) | UiMetrics for everything; grep for violations |
| Icon integration | Font reference inconsistency (Pitfall 5) | Fix existing Material Icons refs; mandate MaterialIcon component |
| Icon integration | Size/readability at car distance (Pitfall 9) | Test at 60cm; minimum 36px for nav icons |
| Animation additions | Frame competition with AA video (Pitfall 8) | Max 80ms, use *Animator types, disable during AA |
| All QML changes | Qt 6.4/6.8 compat (Pitfall 4) | Test on both platforms; forbidden API checklist |
| Touch UX improvements | EVIOCGRAB invisible gaps (Pitfall 7) | Classify each interaction by touch input path |

## Sources

- [Qt Quick Performance Considerations](https://doc.qt.io/qt-6/qtquick-performance.html) -- HIGH confidence (official Qt docs, layer.enabled warnings, clipping costs, animation guidance)
- [Qt 6.8 What's New](https://doc.qt.io/qt-6/whatsnew68.html) -- HIGH confidence (official, documents new APIs unavailable in 6.4)
- [StackView QML Type documentation](https://doc.qt.io/qt-6/qml-qtquick-controls-stackview.html) -- HIGH confidence (official, transition customization, destroyOnPop)
- [Qt Item QML Type - layer property](https://doc.qt.io/qt-6/qml-qtquick-item.html) -- HIGH confidence (official, GPU memory warning for layer.enabled)
- [Qt Text improvements in 6.7](https://www.qt.io/blog/text-improvements-in-qt-6.7) -- HIGH confidence (official blog, CurveRendering details)
- [Qt Forum: QML animations slow on Raspberry Pi](https://forum.qt.io/topic/125095/qml-runs-animations-on-raspberry-pi-very-slow) -- MEDIUM confidence (community, Pi-specific GPU constraints)
- [Spyro Soft: QML Performance Dos and Don'ts](https://spyro-soft.com/developers/qt-quick-qml-performance-optimisation) -- MEDIUM confidence (third-party, but aligns with official docs)
- [Qt Forum: Keep state of view in StackView](https://forum.qt.io/topic/91400/keep-the-state-of-the-view-in-a-stackview) -- MEDIUM confidence (community, documents state loss on push/pop)
- Project codebase analysis: Shell.qml, SettingsMenu.qml, NavStrip.qml, EqSettings.qml, MaterialIcon.qml, UiMetrics.qml, Sidebar.qml -- HIGH confidence (read directly)
- Project CLAUDE.md and MEMORY.md: EVIOCGRAB behavior, Qt version constraints, evdev touch architecture -- HIGH confidence (project documentation)

---
*Pitfalls research for: OpenAuto Prodigy v0.4.3 Interface Polish & Settings Reorganization milestone*
*Researched: 2026-03-02*
