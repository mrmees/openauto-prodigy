#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <functional>

namespace oap {

class YamlConfig;
class ThemeService;
class AudioService;
class PluginManager;

/// Unix domain socket IPC server for the web config panel.
///
/// Listens on /tmp/openauto-prodigy.sock for JSON requests from the
/// Flask web server. Handles config read/write, theme changes, and
/// plugin queries. Single-writer rule: only this app writes config.
class IpcServer : public QObject {
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer() override;

    /// Start listening. Returns false if socket already in use.
    bool start(const QString& socketPath = QStringLiteral("/tmp/openauto-prodigy.sock"));
    void stop();

    // Inject dependencies
    void setConfig(YamlConfig* config, const QString& configPath);
    void setThemeService(ThemeService* themeService);
    void setAudioService(AudioService* audioService);
    void setPluginManager(PluginManager* pluginManager);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QByteArray handleRequest(const QByteArray& request);
    QByteArray handleGetConfig();
    QByteArray handleSetConfig(const QVariantMap& data);
    QByteArray handleGetTheme();
    QByteArray handleSetTheme(const QVariantMap& data);
    QByteArray handleListPlugins();
    QByteArray handleStatus();
    QByteArray handleGetAudioDevices();
    QByteArray handleGetAudioConfig();
    QByteArray handleSetAudioConfig(const QVariantMap& data);

    QLocalServer* server_ = nullptr;
    YamlConfig* config_ = nullptr;
    QString configPath_;
    ThemeService* themeService_ = nullptr;
    AudioService* audioService_ = nullptr;
    PluginManager* pluginManager_ = nullptr;
};

} // namespace oap
