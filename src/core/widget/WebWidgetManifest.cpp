#include "core/widget/WebWidgetManifest.hpp"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QDebug>
#include <yaml-cpp/yaml.h>

#include <optional>
#include <utility>

namespace oap {

namespace {

bool isSafeIdentifier(const QString& value)
{
    static const QRegularExpression safeId(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*\\z"));
    return safeId.match(value).hasMatch();
}

QString stringValue(const YAML::Node& node)
{
    return QString::fromStdString(node.as<std::string>()).trimmed();
}

std::optional<QVariant> scalarValue(const YAML::Node& node)
{
    if (!node || !node.IsScalar())
        return std::nullopt;

    const QString text = QString::fromStdString(node.Scalar());
    const std::string tag = node.Tag();
    if (tag == "!" || tag == "tag:yaml.org,2002:str")
        return QVariant(text);

    const QString lower = text.toLower();
    if (lower == QStringLiteral("true"))
        return QVariant(true);
    if (lower == QStringLiteral("false"))
        return QVariant(false);

    static const QRegularExpression integer(QStringLiteral("^-?[0-9]+\\z"));
    if (integer.match(text).hasMatch()) {
        bool ok = false;
        const qlonglong value = text.toLongLong(&ok);
        if (ok)
            return QVariant::fromValue(value);
    }
    return QVariant(text);
}

QString variantIdentity(const QVariant& value)
{
    return QString::number(value.typeId()) + u':' + value.toString();
}

std::optional<ConfigSchemaField> parseConfigField(const YAML::Node& node)
{
    if (!node.IsMap())
        return std::nullopt;

    ConfigSchemaField field;
    field.key = stringValue(node["key"]);
    field.label = stringValue(node["label"]);
    const QString type = stringValue(node["type"]);
    if (!isSafeIdentifier(field.key) || field.label.isEmpty())
        return std::nullopt;

    if (type == QStringLiteral("enum")) {
        const YAML::Node options = node["options"];
        if (!options || !options.IsSequence() || options.size() == 0)
            return std::nullopt;

        field.type = ConfigFieldType::Enum;
        QSet<QString> storedValues;
        for (const auto& option : options) {
            if (!option.IsMap())
                return std::nullopt;
            const QString label = stringValue(option["label"]);
            const auto value = scalarValue(option["value"]);
            if (label.isEmpty() || !value)
                return std::nullopt;
            const QString identity = variantIdentity(*value);
            if (storedValues.contains(identity))
                return std::nullopt;
            storedValues.insert(identity);
            field.options.append(label);
            field.values.append(*value);
        }
        return field;
    }

    if (type == QStringLiteral("bool")) {
        field.type = ConfigFieldType::Bool;
        return field;
    }

    if (type == QStringLiteral("intRange")) {
        field.type = ConfigFieldType::IntRange;
        field.rangeMin = node["min"].as<int>(field.rangeMin);
        field.rangeMax = node["max"].as<int>(field.rangeMax);
        field.rangeStep = node["step"].as<int>(field.rangeStep);
        const qint64 width = static_cast<qint64>(field.rangeMax) - field.rangeMin;
        if (width < 0 || field.rangeStep <= 0
            || (width > 0 && field.rangeStep > width)) {
            return std::nullopt;
        }
        return field;
    }

    if (type == QStringLiteral("collection")) {
        field.type = ConfigFieldType::Collection;
        field.collection = stringValue(node["collection"]);
        if (!isSafeIdentifier(field.collection))
            return std::nullopt;
        field.required = node["required"].as<bool>(false);
        return field;
    }

    return std::nullopt;
}

void parseConfiguration(const YAML::Node& root, const QString& filePath,
                        WebWidgetManifest& manifest)
{
    const YAML::Node configuration = root["configuration"];
    if (!configuration)
        return;
    if (!configuration.IsMap()) {
        qWarning() << "WebWidgetManifest: ignoring invalid configuration in" << filePath;
        return;
    }

    bool requestedConfigureOnAdd = false;
    try {
        requestedConfigureOnAdd = configuration["configureOnAdd"].as<bool>(false);
    } catch (const YAML::Exception&) {
        qWarning() << "WebWidgetManifest: ignoring invalid configureOnAdd in" << filePath;
    }

    const YAML::Node fields = configuration["fields"];
    if (!fields)
        return;
    if (!fields.IsSequence()) {
        qWarning() << "WebWidgetManifest: ignoring invalid configuration fields in" << filePath;
        return;
    }

    QSet<QString> acceptedKeys;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        try {
            auto field = parseConfigField(fields[index]);
            if (!field || acceptedKeys.contains(field->key)) {
                qWarning() << "WebWidgetManifest: dropping configuration field"
                           << static_cast<qulonglong>(index) << "in" << filePath;
                continue;
            }
            acceptedKeys.insert(field->key);
            manifest.configSchema.append(std::move(*field));
        } catch (const YAML::Exception&) {
            qWarning() << "WebWidgetManifest: dropping configuration field"
                       << static_cast<qulonglong>(index) << "in" << filePath;
        }
    }
    manifest.configureOnAdd = requestedConfigureOnAdd && !manifest.configSchema.isEmpty();
}

} // namespace

bool WebWidgetManifest::isValid() const
{
    // id is used verbatim as a URL path segment and resolver key.
    if (id.isEmpty() || name.isEmpty() || !isSafeIdentifier(id))
        return false;
    if (entry.isEmpty() || entry.startsWith(u'/') || entry.contains(QStringLiteral("..")))
        return false;
    if (minCols < 1 || minRows < 1 || maxCols < minCols || maxRows < minRows)
        return false;
    if (defaultCols < minCols || defaultCols > maxCols
        || defaultRows < minRows || defaultRows > maxRows)
        return false;
    return true;
}

WebWidgetManifest WebWidgetManifest::fromFile(const QString& filePath)
{
    WebWidgetManifest m;
    try {
        YAML::Node root = YAML::LoadFile(filePath.toStdString());

        m.id = QString::fromStdString(root["id"].as<std::string>(""));
        m.name = QString::fromStdString(root["name"].as<std::string>(""));
        m.entry = QString::fromStdString(root["entry"].as<std::string>("index.html"));
        m.category = QString::fromStdString(root["category"].as<std::string>("status"));
        m.description = QString::fromStdString(root["description"].as<std::string>(""));
        m.icon = QString::fromStdString(root["icon"].as<std::string>(""));

        if (root["size"] && root["size"].IsMap()) {
            const auto& s = root["size"];
            m.minCols = s["minCols"].as<int>(m.minCols);
            m.minRows = s["minRows"].as<int>(m.minRows);
            m.maxCols = s["maxCols"].as<int>(m.maxCols);
            m.maxRows = s["maxRows"].as<int>(m.maxRows);
            m.defaultCols = s["defaultCols"].as<int>(m.defaultCols);
            m.defaultRows = s["defaultRows"].as<int>(m.defaultRows);
        }
        parseConfiguration(root, filePath, m);
        m.dirPath = QFileInfo(filePath).absolutePath();
    } catch (const YAML::Exception& e) {
        qWarning() << "WebWidgetManifest: failed to parse" << filePath << ":" << e.what();
        return {};
    }
    return m;
}

} // namespace oap
