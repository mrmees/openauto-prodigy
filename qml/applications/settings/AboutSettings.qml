import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.gap

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: UiMetrics.sectionGap
            spacing: UiMetrics.marginRow

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.onSurface
            }

            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        ElevatedButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.4
            Layout.preferredHeight: UiMetrics.rowH
            text: "Close App"
            iconCode: "\ue5cd"
            onClicked: exitDialog.open()
        }
    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
