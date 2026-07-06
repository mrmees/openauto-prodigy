import QtQuick

/// Hosts framework-registered overlays (design 2026-07-05 §4.3).
/// Stacking/visibility live in OverlayService; content data-binding stays in
/// each overlay component. Loaders are lazy: hidden overlays hold no memory.
///
/// The component ROOT is the Repeater on purpose: a Repeater inserts its
/// delegates as children of ITS OWN parent (the Shell root), so delegate z
/// competes in the same stacking context as the legacy overlays. Wrapping
/// this in an Item would trap all framework overlays at the wrapper's z=0
/// and every legacy overlay (1000-4000) would beat them.
///
/// Geometry contract: a descriptor's geometry map is effectively all-or-nothing —
/// the delegate only leaves self-anchoring (fill) mode when geometry.width is
/// defined. x/y-only maps are treated as self-anchoring; their position is ignored.
Repeater {
    model: OverlayService
    delegate: Loader {
        active: model.overlayVisible
        source: model.qmlComponent
        z: model.z
        // Self-anchoring (fills Shell) unless the descriptor carries geometry
        anchors.fill: (model.geometry && model.geometry.width !== undefined) ? undefined : parent
        x: model.geometry && model.geometry.x !== undefined ? model.geometry.x : 0
        y: model.geometry && model.geometry.y !== undefined ? model.geometry.y : 0
        width: model.geometry && model.geometry.width !== undefined ? model.geometry.width : undefined
        height: model.geometry && model.geometry.height !== undefined ? model.geometry.height : undefined
    }
}
