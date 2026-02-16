import QtQuick

Image {
    id: icon

    property color tintColor: "transparent"

    sourceSize.width: width
    sourceSize.height: height
    fillMode: Image.PreserveAspectFit
    smooth: true
    antialiasing: true
}
