import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: settingsMenu

    Component.onCompleted: ApplicationController.setTitle("Settings")

    Connections {
        target: ApplicationController
        function onBackRequested() {
            if (settingsStack.depth > 1) {
                settingsStack.pop()
                ApplicationController.setTitle("Settings")
                ApplicationController.setBackHandled(true)
            }
        }
    }

    StackView {
        id: settingsStack
        anchors.fill: parent
        initialItem: settingsList
    }

    Component {
        id: settingsList

        ListView {
            anchors.fill: parent
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            model: ListModel {
                id: settingsListModel
            }

            delegate: Loader {
                width: ListView.view.width
                sourceComponent: type === "section" ? sectionDelegate : itemDelegate
                required property string type
                required property string icon
                required property string label
                required property string subtitle
                required property string pageId
            }

            Component.onCompleted: {
                // General
                settingsListModel.append({ type: "section", icon: "", label: "General", subtitle: "", pageId: "" })
                settingsListModel.append({ type: "item", icon: "\ueb97", label: "Display", subtitle: "Brightness, theme, night mode", pageId: "display" })
                settingsListModel.append({ type: "item", icon: "\ue050", label: "Audio", subtitle: "Volume, devices, microphone", pageId: "audio" })
                settingsListModel.append({ type: "item", icon: "\ue1d8", label: "Connection", subtitle: "WiFi, Bluetooth, Android Auto", pageId: "connection" })
                settingsListModel.append({ type: "item", icon: "\ue04b", label: "Video", subtitle: "Resolution, codecs, sidebar", pageId: "video" })
                settingsListModel.append({ type: "item", icon: "\uf8cd", label: "System", subtitle: "Identity, hardware, touch", pageId: "system" })

                // Companion
                settingsListModel.append({ type: "section", icon: "", label: "Companion", subtitle: "", pageId: "" })
                settingsListModel.append({ type: "item", icon: "\ue324", label: "Companion", subtitle: "Phone pairing, GPS, internet", pageId: "companion" })

                // Plugins
                var pluginCount = 0
                if (typeof PluginModel !== "undefined") {
                    for (var i = 0; i < PluginModel.rowCount(); i++) {
                        var idx = PluginModel.index(i, 0)
                        var settingsUrl = PluginModel.data(idx, 264)  // SettingsQmlRole
                        if (settingsUrl && settingsUrl.toString() !== "") {
                            if (pluginCount === 0) {
                                settingsListModel.append({ type: "section", icon: "", label: "Plugins", subtitle: "", pageId: "" })
                            }
                            var pluginId = PluginModel.data(idx, 257)    // PluginIdRole
                            var pluginName = PluginModel.data(idx, 258)  // PluginNameRole
                            var pluginIcon = PluginModel.data(idx, 260)  // PluginIconTextRole
                            settingsListModel.append({
                                type: "item",
                                icon: pluginIcon || "\ue8b8",
                                label: pluginName + " Settings",
                                subtitle: "",
                                pageId: "plugin:" + pluginId
                            })
                            pluginCount++
                        }
                    }
                }

                // About
                settingsListModel.append({ type: "section", icon: "", label: "", subtitle: "", pageId: "" })
                settingsListModel.append({ type: "item", icon: "\ue88e", label: "About", subtitle: "Version, license, close app", pageId: "about" })
            }
        }
    }

    Component {
        id: sectionDelegate
        SectionHeader { text: parent ? parent.label : "" }
    }

    Component {
        id: itemDelegate
        SettingsListItem {
            icon: parent ? parent.icon : ""
            label: parent ? parent.label : ""
            subtitle: parent ? parent.subtitle : ""
            onClicked: openPage(parent ? parent.pageId : "")
        }
    }

    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: videoPage; VideoSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: companionPage; CompanionSettings {} }
    Component { id: aboutPage; AboutSettings {} }

    function openPage(pageId) {
        var titles = {
            "display": "Display",
            "audio": "Audio",
            "connection": "Connection",
            "video": "Video",
            "system": "System",
            "companion": "Companion",
            "about": "About"
        }
        var pages = {
            "display": displayPage,
            "audio": audioPage,
            "connection": connectionPage,
            "video": videoPage,
            "system": systemPage,
            "companion": companionPage,
            "about": aboutPage
        }

        if (pageId.startsWith("plugin:")) {
            var pluginId = pageId.substring(7)
            if (typeof PluginModel === "undefined") {
                return
            }
            for (var i = 0; i < PluginModel.rowCount(); i++) {
                var idx = PluginModel.index(i, 0)
                if (PluginModel.data(idx, 257) === pluginId) {
                    var settingsUrl = PluginModel.data(idx, 264)  // SettingsQmlRole
                    if (settingsUrl && settingsUrl.toString() !== "") {
                        ApplicationController.setTitle("Settings > " + PluginModel.data(idx, 258))
                        settingsStack.push(settingsUrl)
                    }
                    return
                }
            }
            return
        }

        if (pages[pageId]) {
            ApplicationController.setTitle("Settings > " + titles[pageId])
            settingsStack.push(pages[pageId])
        }
    }
}
