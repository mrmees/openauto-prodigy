# Platform / Plugin Architecture Refactor — Design

**Date:** 2026-03-13
**Branch:** `dev/0.6`
**Status:** Approved

## Overview

Prodigy has enough shell, plugin, widget, and service infrastructure that a full rewrite is unnecessary, but the boundaries are still loose enough that temporary wiring is becoming architecture. This design formalizes Prodigy as a platform shell with capability-based plugins before more feature work is built on top of the current drift.

The core idea is:

- Prodigy core is a platform runtime, not a feature app.
- The home screen is a first-class dashboard destination, not just a launcher.
- One foreground plugin owns the main experience at a time.
- Plugins contribute feature behavior and UI through explicit capability types.
- Core retains ownership of singleton hardware, shell policy, and lifecycle arbitration.

## Goals

- Preserve Prodigy as a Pi-first Android Auto product while making the architecture extensible.
- Make Android Auto a clean projection plugin instead of a special case smeared across shell and root QML context.
- Keep the dashboard/home screen as a real destination for AA-derived information and non-AA plugin features.
- Prevent future plugins from needing to reimplement transport, layout, or hardware policy.
- Refactor incrementally rather than pausing the project for a speculative full rewrite.

## Non-Goals

- Building a full standalone native phone stack or Bluetooth media product now.
- Making every subsystem a plugin regardless of whether it owns singleton hardware/system policy.
- Supporting arbitrary freeform window management across the shell.
- Solving full CarPlay or non-AA projection support in this phase.

## Approved Architectural Principles

### 1. Core Is A Thin Platform, Not A Feature Layer

Core owns:

- process bootstrap and runtime assembly
- config/state persistence
- display metrics, physical size, DPI, safe areas, and user scaling
- touch continuity and routing
- shell chrome and fullscreen policy
- dashboard/grid infrastructure
- plugin discovery, lifecycle, and isolation
- singleton device/system services such as Bluetooth, Wi-Fi, audio, and display control

Core does not own product features like Android Auto, OBD, radio, or media-player behavior.

### 2. The Home Screen Is A First-Class Surface

Prodigy exposes two top-level user surfaces:

- `dashboard/home`
- `foreground experience`

The dashboard is core-owned and always available. It is the composition surface for widgets and plugin-provided dashboard content. The foreground experience is owned by one plugin at a time.

### 3. Plugins Use Capability Types

A plugin can contribute any combination of:

- `foreground experience`
- `settings contribution`
- `dashboard widget`
- `live-surface widget`
- `background provider`

These capabilities are optional. A plugin is not required to expose settings or a foreground app just to exist.

### 4. The Widget Grid Is The Universal Dashboard Placement System

“Anywhere” on the dashboard means anywhere within the widget layout system, not arbitrary shell-wide pane ownership.

The dashboard remains one page-based grid editor and persistence model. Plugins contribute tiles into that grid. Some tiles are cheap dashboard-native widgets; some are heavier plugin-rendered live-surface widgets.

This avoids turning Prodigy into a general window manager while still allowing embeddable AA navigation panes, OBD gauges, and similar ideas later.

### 5. Widgets And Live Surfaces Are Not The Same Thing

Normal dashboard widgets are lightweight, shell-friendly tiles. Live-surface widgets are plugin-rendered embedded surfaces with stricter lifecycle and resource constraints.

Live-surface widgets must declare enough metadata for the shell/dashboard to enforce:

- supported sizes
- aspect/resize behavior
- touch handling expectations
- suspend/resume rules
- failure/fallback behavior
- resource class or admission limits

Without this distinction, the widget system would inherit projection-surface complexity and become unstable.

## Target Runtime Shape

Prodigy should be split conceptually into five areas.

### Core Platform

- runtime/bootstrap assembly
- config/state services
- display metrics and scaling services
- touch/input routing services
- Bluetooth/Wi-Fi/audio/display/system services
- plugin lifecycle/runtime manager

### Shell

- top-level chrome
- status/navigation controls
- overlays and interruptions
- fullscreen policy
- dashboard host
- foreground plugin host

### Dashboard

- page/grid model
- widget editing and persistence
- plugin contribution placement
- lightweight widget hosting
- live-surface widget hosting

### Plugin Capability Layer

- foreground contribution contracts
- settings contribution contracts
- dashboard widget contracts
- live-surface widget contracts
- background provider contracts

### Feature Plugins

- Android Auto
- OBD / gauges
- radio / tuner
- local media
- future community plugins

## Android Auto Position

Android Auto is the primary projection plugin and the highest-priority consumer of the platform.

It should provide three contribution levels over time:

- full AA foreground experience
- structured AA dashboard widgets such as nav summary, now playing, and connection status
- selected AA live-surface widgets later, such as embeddable navigation/map panes when the protocol path is understood well enough

Android Auto should own:

- AA protocol/session orchestration
- AA-specific feature UI
- AA-derived structured data contributions
- AA-specific live-surface widget logic

Android Auto should not own:

- generic Bluetooth adapter control
- generic HFP lifecycle/platform policy
- generic audio-route policy
- shell layout/navbar policy
- global touch-routing policy for the entire system

## Bluetooth / Phone / Media Boundary

Bluetooth is required to bootstrap Android Auto, but that does not make Bluetooth an Android Auto concern.

Core platform should own:

- Bluetooth adapter and pairing agent
- paired-device registry
- profile registration and connection policy
- HFP/A2DP transport lifecycle
- audio route/focus policy
- generic phone/media/call state services

Optional plugins or UI layers can later own:

- phone dialer UI
- Bluetooth media player UI
- call screen UI
- settings pages specific to those experiences

This supports the intended product focus:

- rock-solid AA integration now
- minimal system UI for pairing/device management
- no requirement to flesh out a native phone/media UX unless it becomes desirable later

The recommended direction is to move current phone/media monitoring logic into core services and keep any future phone/media plugins as thin UI layers over those services.

## Settings Model

Core retains a small system settings surface for platform concerns:

- display sizing/scaling
- touch/device calibration
- shell layout/chrome
- device-level audio routing
- Bluetooth pairing/device management
- other transport/platform settings

Plugins may contribute their own settings sections or pages for feature-specific behavior. “Every plugin gets a settings page” is explicitly not a requirement.

## Lifecycle And Failure Model

Plugin failures must remain contained.

Core should guarantee:

- shell/dashboard still start if a plugin fails to initialize
- Bluetooth/phone transport survives AA plugin crashes or disconnects
- one bad plugin cannot poison global QML context
- live-surface widgets can be suspended or removed without killing the dashboard

Plugin contributions should eventually support explicit lifecycle states such as:

- available
- active
- suspended
- failed

This is especially important for live-surface widgets and background providers.

## Fit-Gap Summary Against Current Code

The current codebase already contains useful foundations:

- plugin discovery/lifecycle/runtime context machinery
- one active foreground plugin model
- a real dashboard/home surface
- a page-based widget grid with persistence
- core services for Bluetooth, audio, display, config, notifications, and actions

The main architectural problems are boundary leaks, not missing infrastructure.

Current mismatches:

- `main.cpp` still acts as a god-assembler for product architecture
- shell and dashboard QML still depend on root-context globals for AA/phone state
- the widget system only models “a QML component in a tile,” not typed dashboard contributions
- legacy enum-based app navigation still exists alongside plugin-driven foreground navigation
- Android Auto currently owns too much platform-adjacent display/touch/session wiring
- phone/media “plugins” currently behave more like platform monitors than clear feature plugins

## Migration Strategy

This should be implemented as a staged architectural refactor, not a full rewrite.

### Recommended order

1. Formalize the runtime seam and plugin contribution model.
2. Move platform-ish logic out of Android Auto / Phone / BtAudio into core capabilities.
3. Replace shell/dashboard root-context globals with explicit plugin/dashboard contribution interfaces.
4. Extend the widget model to distinguish lightweight widgets from live-surface widgets.
5. Remove remaining legacy enum-app navigation and fold it into shell/runtime state.
6. Reclassify surviving plugins around the new capability model.

## Out Of Scope For This Design

- detailed implementation task breakdown
- API naming and header-level interface design
- specific storage schema changes for widget contribution metadata
- exact live-surface rendering plumbing
- test-by-test execution sequence

Those belong in the implementation plan.
