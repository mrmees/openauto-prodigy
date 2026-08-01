import QtQuick

Item {
    id: root
    objectName: "nativeNavigationCard"

    property QtObject navigationProvider: null
    property bool aaConnected: false

    readonly property bool routeActive:
        navigationProvider && navigationProvider.navActive
    readonly property bool guidanceFresh:
        navigationProvider && navigationProvider.guidanceFresh
    readonly property bool rerouting:
        navigationProvider && navigationProvider.rerouting
    readonly property bool showGuidance:
        aaConnected && routeActive && guidanceFresh && !rerouting
    readonly property real distanceSize:
        Math.max(64, Math.min(96, height * 0.16))
    readonly property real unitSize:
        Math.max(28, Math.min(38, height * 0.064))
    readonly property real roadSize:
        Math.max(30, Math.min(40, height * 0.067))
    readonly property real secondaryCueSize:
        Math.max(24, Math.min(28, height * 0.047))
    readonly property real labelSize: Math.max(22, height * 0.037)
    readonly property real destinationSize:
        Math.max(30, Math.min(40, height * 0.067))
    readonly property real cardPadding:
        Math.max(18, Math.min(32, height * 0.053))
    readonly property string formattedDistance:
        navigationProvider ? navigationProvider.formattedDistance : ""
    readonly property string distanceValue: splitDistance(formattedDistance, 0)
    readonly property string distanceUnit: splitDistance(formattedDistance, 1)
    readonly property string destination:
        navigationProvider ? String(navigationProvider.destination).trim() : ""

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
                text: !root.aaConnected
                      ? "Connect Android Auto"
                      : root.routeActive
                        && (!root.guidanceFresh || root.rerouting)
                        ? "Finding a new route"
                        : "Start a route in Android Auto"
            }
        }

        Item {
            id: guidanceContent
            objectName: "guidanceContent"
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

            Rectangle {
                id: destinationFooter
                objectName: "destinationFooter"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: laneBand.height
                radius: Math.max(14, Math.min(24, root.height * 0.04))
                visible: root.showGuidance
                         && !laneBand.visible
                         && root.destination.length > 0
                color: ThemeService.surfaceContainerLow

                Column {
                    anchors.fill: parent
                    anchors.leftMargin: root.cardPadding
                    anchors.rightMargin: root.cardPadding
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    spacing: 4

                    Item {
                        id: destinationMetricRow
                        objectName: "destinationMetricRow"
                        width: parent.width + root.cardPadding
                        height: root.labelSize + 4

                        readonly property real itemSpacing: 6
                        readonly property real sectionSpacing: 0
                        readonly property real labelGap: 4
                        readonly property real coreMetricWidth:
                            destinationDistanceText.implicitWidth
                            + (destinationDistanceText.visible
                               && destinationEtaText.visible
                               ? itemSpacing + 1 : 0)
                            + destinationEtaText.implicitWidth
                        readonly property bool hasRoomForLabel:
                            width >= destinationPin.width + labelGap
                                     + destinationLabel.implicitWidth
                                     + sectionSpacing + coreMetricWidth
                        readonly property bool hasRoomForDuration:
                            hasRoomForLabel
                            && width >= destinationPin.width + labelGap
                                        + destinationLabel.implicitWidth
                                        + sectionSpacing + coreMetricWidth
                                        + (destinationDistanceText.visible
                                           || destinationEtaText.visible
                                           ? itemSpacing + 1 : 0)
                                        + destinationDurationText.implicitWidth

                        MaterialIcon {
                            id: destinationPin
                            objectName: "destinationPin"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            icon: "\ue55f"
                            size: root.labelSize + 2
                            color: ThemeService.primary
                        }

                        Text {
                            id: destinationLabel
                            objectName: "destinationLabel"
                            anchors.left: destinationPin.right
                            anchors.leftMargin: destinationMetricRow.labelGap
                            anchors.verticalCenter: parent.verticalCenter
                            text: "DESTINATION"
                            visible: destinationMetricRow.hasRoomForLabel
                            color: ThemeService.onSurfaceVariant
                            font.pixelSize: root.labelSize
                            font.weight: Font.DemiBold
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: destinationMetricRow.itemSpacing

                            Text {
                                id: destinationDistanceText
                                objectName: "destinationDistanceText"
                                text: root.navigationProvider
                                      ? root.navigationProvider
                                            .formattedDestinationDistance : ""
                                visible: root.navigationProvider
                                         && root.navigationProvider
                                               .hasDestinationDistance
                                color: ThemeService.onSurface
                                font.pixelSize: root.labelSize
                                font.weight: Font.DemiBold
                            }

                            Rectangle {
                                width: 1
                                height: root.labelSize * 0.8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: destinationDistanceText.visible
                                         && destinationEtaText.visible
                                color: ThemeService.outlineVariant
                            }

                            Text {
                                id: destinationEtaText
                                objectName: "destinationEtaText"
                                text: root.navigationProvider
                                      ? root.navigationProvider.destinationEta : ""
                                visible: root.navigationProvider
                                         && root.navigationProvider.hasDestinationEta
                                color: ThemeService.onSurface
                                font.pixelSize: root.labelSize
                                font.weight: Font.DemiBold
                            }

                            Rectangle {
                                width: 1
                                height: root.labelSize * 0.8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: (destinationDistanceText.visible
                                          || destinationEtaText.visible)
                                         && destinationDurationText.visible
                                color: ThemeService.outlineVariant
                            }

                            Text {
                                id: destinationDurationText
                                objectName: "destinationDurationText"
                                text: root.navigationProvider
                                      ? root.navigationProvider
                                            .formattedTimeToArrival : ""
                                visible: root.navigationProvider
                                         && root.navigationProvider.hasTimeToArrival
                                         && destinationMetricRow
                                               .hasRoomForDuration
                                color: ThemeService.onSurface
                                font.pixelSize: root.labelSize
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Item {
                        id: destinationViewport
                        objectName: "destinationTextViewport"
                        width: parent.width
                        height: parent.height - destinationMetricRow.height
                                - parent.spacing
                        clip: true

                        readonly property bool overflow:
                            destinationText.implicitWidth > width
                        readonly property real overflowDistance:
                            Math.max(0, destinationText.implicitWidth - width)
                        readonly property int marqueeDwellMs: 2000
                        readonly property real marqueePixelsPerSecond: 24
                        property real scrollOffset: 0

                        onOverflowChanged: {
                            if (!overflow)
                                scrollOffset = 0
                        }

                        Text {
                            id: destinationText
                            objectName: "destinationText"
                            x: -destinationViewport.scrollOffset
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.destination
                            color: ThemeService.onSurface
                            font.pixelSize: root.destinationSize
                            font.weight: Font.DemiBold
                            wrapMode: Text.NoWrap
                        }

                        SequentialAnimation {
                            id: destinationMarquee
                            objectName: "destinationMarquee"
                            running: destinationFooter.visible
                                     && destinationViewport.overflow
                            loops: Animation.Infinite

                            PauseAnimation {
                                duration: destinationViewport.marqueeDwellMs
                            }
                            NumberAnimation {
                                target: destinationViewport
                                property: "scrollOffset"
                                from: 0
                                to: destinationViewport.overflowDistance
                                duration: Math.max(
                                    1,
                                    destinationViewport.overflowDistance
                                    / destinationViewport.marqueePixelsPerSecond
                                    * 1000)
                                easing.type: Easing.Linear
                            }
                            PauseAnimation {
                                duration: destinationViewport.marqueeDwellMs
                            }
                            PropertyAction {
                                target: destinationViewport
                                property: "scrollOffset"
                                value: 0
                            }

                            onRunningChanged: {
                                if (!running)
                                    destinationViewport.scrollOffset = 0
                            }
                        }
                    }
                }
            }

            Item {
                id: primaryGuidance
                objectName: "primaryGuidance"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: laneBand.visible
                                ? laneBand.top
                                : destinationFooter.visible
                                  ? destinationFooter.top : parent.bottom
                anchors.bottomMargin: laneBand.visible
                                      || destinationFooter.visible
                                      ? root.cardPadding : 0

                Item {
                    id: primaryTopRow
                    objectName: "primaryTopRow"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Math.min(
                        parent.height,
                        Math.max(root.height * 0.25,
                                 root.distanceSize * 1.25
                                 + root.secondaryCueSize + 8))

                    Rectangle {
                        id: maneuverTile
                        objectName: "maneuverTile"
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * 0.35
                        radius: Math.max(14, Math.min(24,
                                                      root.height * 0.04))
                        color: ThemeService.surfaceContainerLow

                        NavigationManeuverGlyph {
                            objectName: "maneuverGlyph"
                            anchors.centerIn: parent
                            maneuverType: root.navigationProvider
                                          ? root.navigationProvider.maneuverType
                                          : 0
                            size: Math.max(
                                80,
                                Math.min(168,
                                         maneuverTile.width * 0.84,
                                         maneuverTile.height * 0.84))
                            color: ThemeService.primary
                        }
                    }

                    Item {
                        id: topInfoBlock
                        objectName: "topInfoBlock"
                        anchors.left: maneuverTile.right
                        anchors.leftMargin: root.cardPadding
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        Item {
                            id: cueTimeRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: root.secondaryCueSize * 1.2
                            visible: roadText.visible

                            readonly property real separatorSpacing: 12
                            readonly property real horizontalMargins:
                                root.cardPadding
                            readonly property bool stepTimeFits:
                                root.navigationProvider
                                && root.navigationProvider.hasTimeToStep
                                && secondaryCue.implicitWidth
                                   + stepTime.implicitWidth
                                   + separatorSpacing + horizontalMargins
                                   <= width

                            Text {
                                id: stepTime
                                objectName: "stepTimeText"
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.navigationProvider
                                      ? root.navigationProvider
                                            .formattedTimeToStep : ""
                                visible: cueTimeRow.stepTimeFits
                                horizontalAlignment: Text.AlignRight
                                color: ThemeService.onSurfaceVariant
                                font.pixelSize: root.secondaryCueSize
                                wrapMode: Text.NoWrap
                            }

                            Text {
                                id: secondaryCue
                                objectName: "secondaryCueText"
                                anchors.left: parent.left
                                anchors.right: stepTime.visible
                                               ? stepTime.left : parent.right
                                anchors.rightMargin: stepTime.visible
                                                     ? cueTimeRow.separatorSpacing : 0
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                text: root.navigationProvider
                                      && root.navigationProvider.hasActionCue
                                      ? root.navigationProvider.actionCue
                                      : "Next turn"
                                color: ThemeService.onSurfaceVariant
                                font.pixelSize: root.secondaryCueSize
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }
                        }

                        Row {
                            id: distanceRow
                            objectName: "distanceRow"
                            anchors.right: parent.right
                            anchors.top: cueTimeRow.visible
                                         ? cueTimeRow.bottom : parent.top
                            anchors.topMargin: cueTimeRow.visible ? 4 : 0
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
                    }
                }

                Text {
                    id: roadText
                    objectName: "roadText"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: primaryTopRow.bottom
                    anchors.topMargin: 12
                    anchors.bottom: parent.bottom
                    text: root.navigationProvider
                          ? root.navigationProvider.roadName : ""
                    visible: text.length > 0
                    color: ThemeService.onSurface
                    font.pixelSize: root.roadSize
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
