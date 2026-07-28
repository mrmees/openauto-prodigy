import QtQuick

Item {
    id: root
    objectName: "nativeNavigationCard"

    property QtObject navigationProvider: null
    property bool aaConnected: false

    readonly property bool routeActive:
        navigationProvider && navigationProvider.navActive
    readonly property bool showGuidance: aaConnected && routeActive
    readonly property real distanceSize:
        Math.max(64, Math.min(96, height * 0.16))
    readonly property real unitSize:
        Math.max(28, Math.min(38, height * 0.064))
    readonly property real roadSize:
        Math.max(30, Math.min(40, height * 0.067))
    readonly property real secondaryCueSize:
        Math.max(24, Math.min(28, height * 0.047))
    readonly property real labelSize: Math.max(22, height * 0.037)
    readonly property real cardPadding:
        Math.max(18, Math.min(32, height * 0.053))
    readonly property string formattedDistance:
        navigationProvider ? navigationProvider.formattedDistance : ""
    readonly property string distanceValue: splitDistance(formattedDistance, 0)
    readonly property string distanceUnit: splitDistance(formattedDistance, 1)

    function splitDistance(distance, part) {
        const match = String(distance).trim().match(/^(.*)\s+([^\s]+)$/)
        if (!match)
            return part === 0 ? String(distance).trim() : ""
        return part === 0 ? match[1] : match[2]
    }

    Rectangle {
        anchors.fill: parent
        radius: Math.max(16, Math.min(28, root.height * 0.047))
        color: ThemeService.surfaceContainerHigh

        Item {
            anchors.fill: parent
            anchors.margins: root.cardPadding
            visible: !root.showGuidance

            MaterialIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: navigationStateText.top
                anchors.bottomMargin: root.cardPadding
                icon: root.aaConnected ? "\ue55c" : "\ue2c1"
                size: Math.max(64, Math.min(96, root.height * 0.16))
                color: ThemeService.onSurfaceVariant
            }

            Text {
                id: navigationStateText
                objectName: "navigationStateText"
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: ThemeService.onSurface
                font.pixelSize: root.secondaryCueSize
                font.weight: Font.DemiBold
                text: root.aaConnected
                      ? "Start a route in Android Auto"
                      : "Connect Android Auto"
            }
        }

        Item {
            id: guidanceContent
            anchors.fill: parent
            anchors.margins: root.cardPadding
            visible: root.showGuidance

            NavigationLaneGuidanceBand {
                id: laneBand
                objectName: "laneGuidanceBand"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Math.max(84, Math.min(116, root.height * 0.19))
                visible: root.navigationProvider
                         && root.navigationProvider.hasLaneGuidance
                laneModel: root.navigationProvider
                           ? root.navigationProvider.laneModel : null
            }

            Item {
                id: primaryGuidance
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: laneBand.visible
                                ? laneBand.top : parent.bottom
                anchors.bottomMargin: laneBand.visible ? root.cardPadding : 0

                Rectangle {
                    id: maneuverTile
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.min(parent.width * 0.27,
                                    Math.max(180, root.height * 0.34))
                    radius: Math.max(14, Math.min(24, root.height * 0.04))
                    color: ThemeService.surfaceContainerLow

                    Text {
                        objectName: "nextLabel"
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: root.cardPadding
                        text: "NEXT"
                        color: ThemeService.onSurfaceVariant
                        font.pixelSize: root.labelSize
                        font.weight: Font.DemiBold
                    }

                    NavigationManeuverGlyph {
                        objectName: "maneuverGlyph"
                        anchors.centerIn: parent
                        maneuverType: root.navigationProvider
                                      ? root.navigationProvider.maneuverType : 0
                        size: Math.max(80, Math.min(112, root.height * 0.187))
                        color: ThemeService.primary
                    }
                }

                Item {
                    anchors.left: maneuverTile.right
                    anchors.leftMargin: root.cardPadding
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    Text {
                        id: secondaryCue
                        objectName: "secondaryCueText"
                        anchors.left: parent.left
                        anchors.top: parent.top
                        text: "Next turn"
                        visible: roadText.visible
                        color: ThemeService.onSurfaceVariant
                        font.pixelSize: root.secondaryCueSize
                    }

                    Row {
                        id: distanceRow
                        anchors.left: parent.left
                        anchors.top: secondaryCue.visible
                                     ? secondaryCue.bottom : parent.top
                        anchors.topMargin: secondaryCue.visible ? 4 : 0
                        spacing: 12
                        visible: root.navigationProvider
                                 && root.navigationProvider.hasDistance

                        Text {
                            objectName: "distanceText"
                            text: root.distanceValue
                            color: ThemeService.onSurface
                            font.pixelSize: root.distanceSize
                            font.weight: Font.Bold
                        }

                        Text {
                            objectName: "distanceUnitText"
                            anchors.baseline: parent.children[0].baseline
                            text: root.distanceUnit
                            visible: text.length > 0
                            color: ThemeService.onSurfaceVariant
                            font.pixelSize: root.unitSize
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        id: roadText
                        objectName: "roadText"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: distanceRow.visible
                                     ? distanceRow.bottom : secondaryCue.bottom
                        anchors.topMargin: 4
                        anchors.bottom: parent.bottom
                        text: root.navigationProvider
                              ? root.navigationProvider.roadName : ""
                        visible: text.length > 0
                        color: ThemeService.onSurface
                        font.pixelSize: root.roadSize
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
