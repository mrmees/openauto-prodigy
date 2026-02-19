#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QList>

namespace oap {

struct PluginSettingDef {
    QString key;
    QString type;       // "bool", "int", "string", "enum"
    QVariant defaultValue;
    QString label;
    QStringList options; // for "enum" type
};

struct PluginManifest {
    QString id;
    QString name;
    QString version;
    int apiVersion = 0;
    QString type;           // "full" or "qml-only"
    QString description;
    QString author;
    QString icon;

    QStringList requiredServices;   // Presence-only checks, no semver
    QList<PluginSettingDef> settings;

    int navStripOrder = 99;
    bool navStripVisible = true;

    QString dirPath;    // Absolute path to the plugin directory

    bool isValid() const;

    /// Parse a plugin.yaml file. Returns a manifest with isValid() == false on failure.
    static PluginManifest fromFile(const QString& filePath);
};

} // namespace oap
