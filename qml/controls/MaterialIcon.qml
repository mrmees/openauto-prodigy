import QtQuick

/// Material Symbols Outlined icon rendered as a text glyph.
/// Usage:  MaterialIcon { icon: "\ue050"; size: 24; color: "#ffffff" }
///
/// The font is loaded once globally (see Shell.qml FontLoader).
/// Icon codepoints — reference table:
///   ArrowBack       \ue5c4      VolumeUp        \ue050
///   VolumeDown      \ue04d      VolumeOff       \ue04f
///   VolumeMute      \ue04e      BrightnessHigh  \ue1ac
///   LightMode       \ue518      DarkMode        \ue51c
///   Home            \ue9b2      Close           \ue5cd
///   MusicNote       \ue405      SkipPrevious    \ue045
///   PlayArrow       \ue037      Pause           \ue034
///   SkipNext        \ue044      Phone           \uf0d4
///   PhoneEnabled    \ue9cd      CallEnd         \uf0bc
///   Backspace       \ue14a      DirectionsCar   \ueff7
///   Settings        \ue8b8      DisplaySettings \ueb97
///   Palette         \ue40a      Bluetooth       \ue1a7
///   Build           \uf8cd      Info            \ue88e
///   Headphones      \uf01f
Text {
    id: root

    /// Icon codepoint string (e.g. "\ue050" for volume_up)
    property string icon: ""

    /// Shorthand for font.pixelSize
    property int size: 24

    font.family: materialFont.name
    font.pixelSize: size
    text: icon

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    FontLoader {
        id: materialFont
        source: "qrc:/fonts/MaterialSymbolsOutlined.ttf"
    }
}
