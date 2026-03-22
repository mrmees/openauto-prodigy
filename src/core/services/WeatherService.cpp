#include "core/services/WeatherService.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

namespace oap {

// --- WeatherData ---

WeatherData::WeatherData(QObject* parent)
    : QObject(parent)
{
}

double WeatherData::tempC() const { return tempC_; }
double WeatherData::feelsLikeC() const { return feelsLikeC_; }
int WeatherData::weatherCode() const { return weatherCode_; }
double WeatherData::humidity() const { return humidity_; }
double WeatherData::windSpeedKph() const { return windSpeedKph_; }
int WeatherData::windDirection() const { return windDirection_; }
bool WeatherData::isDay() const { return isDay_; }
bool WeatherData::isLoading() const { return loading_; }
QString WeatherData::error() const { return error_; }
QDateTime WeatherData::lastUpdated() const { return lastUpdated_; }
QString WeatherData::locationName() const { return locationName_; }

void WeatherData::setLoading(bool loading)
{
    if (loading_ != loading) {
        loading_ = loading;
        emit dataChanged();
    }
}

void WeatherData::setError(const QString& error)
{
    error_ = error;
    loading_ = false;
    emit dataChanged();
}

void WeatherData::updateFromJson(const QJsonObject& currentObj)
{
    tempC_ = currentObj["temperature_2m"].toDouble();
    feelsLikeC_ = currentObj["apparent_temperature"].toDouble();
    weatherCode_ = currentObj["weather_code"].toInt();
    humidity_ = currentObj["relative_humidity_2m"].toDouble();
    windSpeedKph_ = currentObj["wind_speed_10m"].toDouble();
    windDirection_ = currentObj["wind_direction_10m"].toInt();
    isDay_ = currentObj["is_day"].toInt() == 1;
    lastUpdated_ = QDateTime::currentDateTime();
    error_ = "";
    loading_ = false;
    emit dataChanged();
}

void WeatherData::setLocationName(const QString& name)
{
    if (locationName_ != name) {
        locationName_ = name;
        emit dataChanged();
    }
}

void WeatherData::setLastUpdated(const QDateTime& dt)
{
    lastUpdated_ = dt;
}

// --- WeatherService ---

WeatherService::WeatherService(QObject* parent)
    : QObject(parent)
    , nam_(new QNetworkAccessManager(this))
    , refreshTimer_(new QTimer(this))
{
    refreshTimer_->setInterval(60000);  // 1-minute tick
    connect(refreshTimer_, &QTimer::timeout, this, &WeatherService::onRefreshTimer);
    refreshTimer_->start();
}

QString WeatherService::roundCoordinate(double lat, double lon)
{
    return QString("%1:%2")
        .arg(QString::number(lat, 'f', 2))
        .arg(QString::number(lon, 'f', 2));
}

QString WeatherService::buildUrl(double lat, double lon)
{
    return QString("https://api.open-meteo.com/v1/forecast?"
                   "latitude=%1&longitude=%2"
                   "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
                   "weather_code,wind_speed_10m,wind_direction_10m,is_day"
                   "&timezone=auto")
        .arg(lat, 0, 'f', 4)
        .arg(lon, 0, 'f', 4);
}

QObject* WeatherService::getWeatherData(double lat, double lon)
{
    QString key = roundCoordinate(lat, lon);

    if (cache_.contains(key))
        return cache_.value(key);

    auto* data = new WeatherData(this);
    cache_.insert(key, data);
    coordsByKey_.insert(key, {lat, lon});

    fetchWeather(key, lat, lon);
    fetchLocationName(data, lat, lon);

    if (cache_.size() > MAX_CACHE_SIZE)
        cleanupStaleEntries();

    return data;
}

void WeatherService::subscribe(double lat, double lon, int intervalMinutes)
{
    QString key = roundCoordinate(lat, lon);

    // Ensure cache entry exists
    if (!cache_.contains(key))
        getWeatherData(lat, lon);

    int intervalMs = intervalMinutes * 60 * 1000;
    subscriberIntervals_[key].append(intervalMs);

    // Check if data is stale relative to this subscriber's interval
    WeatherData* data = cache_.value(key);
    if (data) {
        bool stale = !data->lastUpdated().isValid() ||
                     data->lastUpdated().msecsTo(QDateTime::currentDateTime()) > intervalMs;
        if (stale) {
            const auto it = coordsByKey_.constFind(key);
            if (it != coordsByKey_.cend())
                fetchWeather(key, it->first, it->second);
        }
    }
}

void WeatherService::unsubscribe(double lat, double lon, int intervalMinutes)
{
    QString key = roundCoordinate(lat, lon);
    int intervalMs = intervalMinutes * 60 * 1000;

    if (subscriberIntervals_.contains(key)) {
        subscriberIntervals_[key].removeOne(intervalMs);
        if (subscriberIntervals_[key].isEmpty())
            subscriberIntervals_.remove(key);
    }
}

int WeatherService::subscriberCount(const QString& key) const
{
    return subscriberIntervals_.value(key).size();
}

int WeatherService::effectiveIntervalMs(const QString& key) const
{
    const auto& list = subscriberIntervals_.value(key);
    if (list.isEmpty())
        return 0;
    return *std::min_element(list.constBegin(), list.constEnd());
}

void WeatherService::triggerRefreshTimer()
{
    onRefreshTimer();
}

void WeatherService::triggerCleanup()
{
    cleanupStaleEntries();
}

void WeatherService::requestRefresh(double lat, double lon)
{
    QString key = roundCoordinate(lat, lon);
    if (cache_.contains(key)) {
        auto coords = coordsByKey_.value(key);
        fetchWeather(key, coords.first, coords.second);
    }
}

void WeatherService::fetchWeather(const QString& key, double lat, double lon)
{
    WeatherData* data = cache_.value(key);
    if (!data)
        return;

    data->setLoading(true);

    QUrl url(buildUrl(lat, lon));
    QNetworkRequest request(url);
    QNetworkReply* reply = nam_->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, data]() {
        onWeatherReplyFinished(reply, data);
    });
}

void WeatherService::onWeatherReplyFinished(QNetworkReply* reply, WeatherData* data)
{
    if (reply->error() != QNetworkReply::NoError) {
        data->setError(reply->errorString());
    } else {
        parseResponse(data, reply->readAll());
    }
    reply->deleteLater();
}

void WeatherService::parseResponse(WeatherData* data, const QByteArray& jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        data->setError(QStringLiteral("JSON parse error: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("current")) {
        data->setError(QStringLiteral("Invalid response: missing current data"));
        return;
    }

    QJsonObject currentObj = obj["current"].toObject();
    data->updateFromJson(currentObj);
}

void WeatherService::fetchLocationName(WeatherData* data, double lat, double lon)
{
    QString url = QString("https://geocoding-api.open-meteo.com/v1/reverse?"
                          "latitude=%1&longitude=%2&count=1")
                      .arg(lat, 0, 'f', 4)
                      .arg(lon, 0, 'f', 4);

    QUrl geocodeUrl(url);
    QNetworkRequest request(geocodeUrl);
    QNetworkReply* reply = nam_->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, data]() {
        onGeocodingReplyFinished(reply, data);
    });
}

void WeatherService::onGeocodingReplyFinished(QNetworkReply* reply, WeatherData* data)
{
    if (reply->error() == QNetworkReply::NoError) {
        parseGeocodingResponse(data, reply->readAll());
    }
    // On failure, location name stays empty -- widget can show coords or nothing
    reply->deleteLater();
}

void WeatherService::parseGeocodingResponse(WeatherData* data, const QByteArray& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QJsonObject obj = doc.object();
    QJsonArray results = obj["results"].toArray();
    if (!results.isEmpty()) {
        QString name = results[0].toObject()["name"].toString();
        data->setLocationName(name);
    }
}

void WeatherService::onRefreshTimer()
{
    for (auto it = coordsByKey_.constBegin(); it != coordsByKey_.constEnd(); ++it) {
        const QString& key = it.key();

        // Skip locations with no subscribers
        if (!subscriberIntervals_.contains(key) || subscriberIntervals_[key].isEmpty())
            continue;

        // Check per-location interval
        int interval = effectiveIntervalMs(key);
        WeatherData* data = cache_.value(key);
        if (!data)
            continue;

        // Only fetch if data is older than the effective interval
        if (data->lastUpdated().isValid() &&
            data->lastUpdated().msecsTo(QDateTime::currentDateTime()) < interval)
            continue;

        fetchWeather(key, it.value().first, it.value().second);
    }
}

void WeatherService::cleanupStaleEntries()
{
    if (cache_.size() <= MAX_CACHE_SIZE)
        return;

    // Find oldest entry without subscribers
    QString oldestKey;
    QDateTime oldestTime;

    for (auto it = cache_.constBegin(); it != cache_.constEnd(); ++it) {
        // Never evict entries with active subscribers
        if (subscriberIntervals_.contains(it.key()) && !subscriberIntervals_[it.key()].isEmpty())
            continue;

        QDateTime updated = it.value()->lastUpdated();
        if (oldestKey.isEmpty() || !updated.isValid() || updated < oldestTime) {
            oldestKey = it.key();
            oldestTime = updated;
        }
    }

    if (!oldestKey.isEmpty()) {
        WeatherData* data = cache_.take(oldestKey);
        coordsByKey_.remove(oldestKey);
        subscriberIntervals_.remove(oldestKey);
        data->deleteLater();
    }
}

} // namespace oap
