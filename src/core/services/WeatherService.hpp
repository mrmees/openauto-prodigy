#pragma once
#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;

namespace oap {

class WeatherData : public QObject {
    Q_OBJECT
    Q_PROPERTY(double tempC READ tempC NOTIFY dataChanged)
    Q_PROPERTY(double feelsLikeC READ feelsLikeC NOTIFY dataChanged)
    Q_PROPERTY(int weatherCode READ weatherCode NOTIFY dataChanged)
    Q_PROPERTY(double humidity READ humidity NOTIFY dataChanged)
    Q_PROPERTY(double windSpeedKph READ windSpeedKph NOTIFY dataChanged)
    Q_PROPERTY(int windDirection READ windDirection NOTIFY dataChanged)
    Q_PROPERTY(bool isDay READ isDay NOTIFY dataChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY dataChanged)
    Q_PROPERTY(QString error READ error NOTIFY dataChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY dataChanged)
    Q_PROPERTY(QString locationName READ locationName NOTIFY dataChanged)
public:
    explicit WeatherData(QObject* parent = nullptr);

    double tempC() const;
    double feelsLikeC() const;
    int weatherCode() const;
    double humidity() const;
    double windSpeedKph() const;
    int windDirection() const;
    bool isDay() const;
    bool isLoading() const;
    QString error() const;
    QDateTime lastUpdated() const;
    QString locationName() const;

    void setLoading(bool loading);
    void setError(const QString& error);
    void updateFromJson(const QJsonObject& currentObj);
    void setLocationName(const QString& name);
    void setLastUpdated(const QDateTime& dt);  // test seam -- force age for staleness tests

signals:
    void dataChanged();

private:
    double tempC_ = 0.0;
    double feelsLikeC_ = 0.0;
    int weatherCode_ = 0;
    double humidity_ = 0.0;
    double windSpeedKph_ = 0.0;
    int windDirection_ = 0;
    bool isDay_ = true;
    bool loading_ = false;
    QString error_;
    QDateTime lastUpdated_;
    QString locationName_;
};

class WeatherService : public QObject {
    Q_OBJECT
public:
    explicit WeatherService(QObject* parent = nullptr);

    Q_INVOKABLE QObject* getWeatherData(double lat, double lon);
    Q_INVOKABLE void requestRefresh(double lat, double lon);
    Q_INVOKABLE void subscribe(double lat, double lon, int intervalMinutes = 5);
    Q_INVOKABLE void unsubscribe(double lat, double lon, int intervalMinutes = 5);

    // For testing
    static QString roundCoordinate(double lat, double lon);
    static QString buildUrl(double lat, double lon);
    void parseResponse(WeatherData* data, const QByteArray& jsonData);
    void parseGeocodingResponse(WeatherData* data, const QByteArray& jsonData);

    // For testing -- expose internals
    int subscriberCount(const QString& key) const;
    int effectiveIntervalMs(const QString& key) const;
    void triggerRefreshTimer();     // calls onRefreshTimer() -- test seam
    void triggerCleanup();          // calls cleanupStaleEntries() -- test seam

    int cacheSize() const { return cache_.size(); }

private slots:
    void onWeatherReplyFinished(QNetworkReply* reply, WeatherData* data);
    void onGeocodingReplyFinished(QNetworkReply* reply, WeatherData* data);
    void onRefreshTimer();

private:
    void fetchWeather(const QString& key, double lat, double lon);
    void fetchLocationName(WeatherData* data, double lat, double lon);
    void cleanupStaleEntries();

    QNetworkAccessManager* nam_;
    QHash<QString, WeatherData*> cache_;
    QHash<QString, QPair<double,double>> coordsByKey_;
    QHash<QString, QList<int>> subscriberIntervals_;  // per-location: one entry per active subscriber's intervalMs
    QTimer* refreshTimer_;
    static constexpr int DEFAULT_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
    static constexpr int MAX_CACHE_SIZE = 5;
};

} // namespace oap
