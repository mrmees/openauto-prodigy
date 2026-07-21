#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>
#include "core/services/WeatherService.hpp"

static const QByteArray VALID_RESPONSE = R"({
    "current": {
        "temperature_2m": 22.3,
        "relative_humidity_2m": 65,
        "apparent_temperature": 21.8,
        "weather_code": 3,
        "wind_speed_10m": 12.5,
        "wind_direction_10m": 180,
        "is_day": 1
    }
})";

class FakeNetworkReply final : public QNetworkReply {
public:
    explicit FakeNetworkReply(QObject* parent = nullptr)
        : QNetworkReply(parent)
    {
        setUrl(QUrl(QStringLiteral("https://example.invalid/weather")));
        open(QIODevice::ReadOnly);
        setFinished(true);
    }

    void abort() override {}

protected:
    qint64 readData(char*, qint64) override { return -1; }
};

class TestWeatherService : public QObject {
    Q_OBJECT
private slots:

    void testWeatherDataDefaults()
    {
        oap::WeatherData data;
        QCOMPARE(data.tempC(), 0.0);
        QCOMPARE(data.feelsLikeC(), 0.0);
        QCOMPARE(data.weatherCode(), 0);
        QCOMPARE(data.humidity(), 0.0);
        QCOMPARE(data.windSpeedKph(), 0.0);
        QCOMPARE(data.windDirection(), 0);
        QVERIFY(data.isDay());
        QVERIFY(!data.isLoading());
        QVERIFY(data.error().isEmpty());
        QVERIFY(!data.lastUpdated().isValid());
        QVERIFY(data.locationName().isEmpty());
    }

    void testParseValidResponse()
    {
        oap::WeatherService service;
        oap::WeatherData data;

        service.parseResponse(&data, VALID_RESPONSE);

        QCOMPARE(data.tempC(), 22.3);
        QCOMPARE(data.feelsLikeC(), 21.8);
        QCOMPARE(data.weatherCode(), 3);
        QCOMPARE(data.humidity(), 65.0);
        QCOMPARE(data.windSpeedKph(), 12.5);
        QCOMPARE(data.windDirection(), 180);
        QVERIFY(data.isDay());
        QVERIFY(!data.isLoading());
        QVERIFY(data.error().isEmpty());
        QVERIFY(data.lastUpdated().isValid());
    }

    void testParseMalformedJson()
    {
        oap::WeatherService service;
        oap::WeatherData data;

        service.parseResponse(&data, "not json");
        QVERIFY(!data.error().isEmpty());
    }

    void testParseMissingCurrentKey()
    {
        oap::WeatherService service;
        oap::WeatherData data;

        service.parseResponse(&data, R"({"other": {}})");
        QVERIFY(data.error().contains("missing current"));
    }

    void testRoundCoordinate()
    {
        QCOMPARE(oap::WeatherService::roundCoordinate(37.7749, -122.4194),
                 QStringLiteral("37.77:-122.42"));
        QCOMPARE(oap::WeatherService::roundCoordinate(37.7751, -122.4150),
                 QStringLiteral("37.78:-122.42"));
    }

    void testGetWeatherDataCaching()
    {
        oap::WeatherService service;
        QObject* d1 = service.getWeatherData(37.774, -122.419);
        QObject* d2 = service.getWeatherData(37.774, -122.419);
        QCOMPARE(d1, d2);  // Same rounded location = same pointer

        QObject* d3 = service.getWeatherData(37.80, -122.43);
        QVERIFY(d3 != d1);  // Different location = different pointer
    }

    void testBuildUrl()
    {
        QString url = oap::WeatherService::buildUrl(37.7749, -122.4194);
        QVERIFY(url.contains("api.open-meteo.com"));
        QVERIFY(url.contains("latitude="));
        QVERIFY(url.contains("longitude="));
        QVERIFY(url.contains("temperature_2m"));
        QVERIFY(url.contains("weather_code"));
    }

    void testSubscribeIncrementsCount()
    {
        oap::WeatherService service;
        QString key = oap::WeatherService::roundCoordinate(37.77, -122.42);

        service.getWeatherData(37.77, -122.42);
        QCOMPARE(service.subscriberCount(key), 0);

        service.subscribe(37.77, -122.42);
        QCOMPARE(service.subscriberCount(key), 1);

        service.subscribe(37.77, -122.42);
        QCOMPARE(service.subscriberCount(key), 2);

        service.unsubscribe(37.77, -122.42);
        QCOMPARE(service.subscriberCount(key), 1);

        service.unsubscribe(37.77, -122.42);
        QCOMPARE(service.subscriberCount(key), 0);

        // Clamp to 0 -- extra unsubscribe shouldn't go negative
        service.unsubscribe(37.77, -122.42);
        QCOMPARE(service.subscriberCount(key), 0);
    }

    void testRefreshTimerSkipsUnsubscribed()
    {
        oap::WeatherService service;
        QString key1 = oap::WeatherService::roundCoordinate(40.0, -74.0);
        QString key2 = oap::WeatherService::roundCoordinate(34.0, -118.0);

        // Create two cache entries
        auto* d1 = qobject_cast<oap::WeatherData*>(service.getWeatherData(40.0, -74.0));
        auto* d2 = qobject_cast<oap::WeatherData*>(service.getWeatherData(34.0, -118.0));
        QVERIFY(d1);
        QVERIFY(d2);

        // Subscribe to only one (5 min default)
        service.subscribe(40.0, -74.0, 5);

        // Parse valid responses for both to give them data
        service.parseResponse(d1, VALID_RESPONSE);
        service.parseResponse(d2, VALID_RESPONSE);

        // Age both past the 5-minute interval
        d1->setLastUpdated(QDateTime::currentDateTime().addSecs(-600));
        d2->setLastUpdated(QDateTime::currentDateTime().addSecs(-600));

        // Reset loading state (parseResponse sets it to false)
        // The trigger should set loading=true for the subscribed one
        service.triggerRefreshTimer();

        // Subscribed location should have been fetched (loading=true)
        QVERIFY(d1->isLoading());
        // Unsubscribed location should NOT have been fetched
        QVERIFY(!d2->isLoading());
    }

    void testSubscribeStaleTriggersImmediateFetch()
    {
        oap::WeatherService service;

        auto* data = qobject_cast<oap::WeatherData*>(service.getWeatherData(40.0, -74.0));
        QVERIFY(data);

        // Give it data so it has a lastUpdated, then age it
        service.parseResponse(data, VALID_RESPONSE);
        data->setLastUpdated(QDateTime::currentDateTime().addSecs(-600));  // 10 min old
        QVERIFY(!data->isLoading());

        // Subscribe with 5-minute interval -- data is 10 min old, so stale
        service.subscribe(40.0, -74.0, 5);
        QVERIFY(data->isLoading());  // Immediate fetch triggered
    }

    void testNewEntrySurvivesCapacityCleanup()
    {
        oap::WeatherService service;

        const QDateTime oldest = QDateTime::currentDateTime().addSecs(-600);
        for (int i = 0; i < 5; ++i) {
            auto* data = qobject_cast<oap::WeatherData*>(
                service.getWeatherData(10.0 + i * 10.0, 0.0));
            QVERIFY(data);
            data->setLastUpdated(oldest.addSecs(i));
        }
        QCOMPARE(service.cacheSize(), 5);

        QPointer<oap::WeatherData> sixth = qobject_cast<oap::WeatherData*>(
            service.getWeatherData(60.0, 0.0));
        QVERIFY(sixth);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(sixth);
        QCOMPARE(service.cacheSize(), 5);
        const QString sixthKey =
            oap::WeatherService::roundCoordinate(60.0, 0.0);
        QVERIFY(service.containsCachedKeyForTest(sixthKey));
        QVERIFY(!service.containsCachedKeyForTest(
            oap::WeatherService::roundCoordinate(10.0, 0.0)));
        QCOMPARE(service.getWeatherData(60.0, 0.0), sixth.data());
        service.subscribe(60.0, 0.0);
        QCOMPARE(service.subscriberCount(sixthKey), 1);
    }

    void testCapacityIsSoftWhenAllEntriesSubscribed()
    {
        oap::WeatherService service;
        QList<QPointer<oap::WeatherData>> entries;

        for (int i = 0; i < 5; ++i) {
            const double lat = 10.0 + i * 10.0;
            entries.append(qobject_cast<oap::WeatherData*>(
                service.getWeatherData(lat, 0.0)));
            QVERIFY(entries.back());
            service.subscribe(lat, 0.0);
        }

        QPointer<oap::WeatherData> sixth = qobject_cast<oap::WeatherData*>(
            service.getWeatherData(60.0, 0.0));
        QVERIFY(sixth);
        entries.append(sixth);
        service.subscribe(60.0, 0.0);

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(service.cacheSize(), 6);
        for (const auto& entry : entries)
            QVERIFY(entry);

        service.unsubscribe(60.0, 0.0);
        QCOMPARE(service.cacheSize(), 5);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!sixth);
        for (int i = 0; i < 5; ++i) {
            QVERIFY(entries[i]);
            QCOMPARE(service.subscriberCount(
                         oap::WeatherService::roundCoordinate(
                             10.0 + i * 10.0, 0.0)),
                     1);
        }
    }

    void testWeatherReplyIgnoresDeletedTarget()
    {
        oap::WeatherService service;
        QPointer<oap::WeatherData> target = new oap::WeatherData;
        delete target.data();
        QVERIFY(!target);

        QPointer<FakeNetworkReply> reply = new FakeNetworkReply;
        service.finishWeatherReplyForTest(reply, target);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!reply);
    }

    void testGeocodingReplyIgnoresDeletedTarget()
    {
        oap::WeatherService service;
        QPointer<oap::WeatherData> target = new oap::WeatherData;
        delete target.data();
        QVERIFY(!target);

        QPointer<FakeNetworkReply> reply = new FakeNetworkReply;
        service.finishGeocodingReplyForTest(reply, target);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!reply);
    }

    void testRefreshIntervalRespected()
    {
        oap::WeatherService service;

        auto* data = qobject_cast<oap::WeatherData*>(service.getWeatherData(40.0, -74.0));
        QVERIFY(data);

        // Subscribe with 30-minute interval
        service.subscribe(40.0, -74.0, 30);

        // Give it data, age it 10 minutes
        service.parseResponse(data, VALID_RESPONSE);
        data->setLastUpdated(QDateTime::currentDateTime().addSecs(-600));

        service.triggerRefreshTimer();
        QVERIFY(!data->isLoading());  // 10 min < 30 min interval -- not stale

        // Age it to 35 minutes
        data->setLastUpdated(QDateTime::currentDateTime().addSecs(-2100));

        service.triggerRefreshTimer();
        QVERIFY(data->isLoading());  // 35 min > 30 min interval -- stale
    }

    void testIntervalRecomputesOnUnsubscribe()
    {
        oap::WeatherService service;
        QString key = oap::WeatherService::roundCoordinate(40.0, -74.0);

        service.getWeatherData(40.0, -74.0);

        // Subscribe twice with different intervals
        service.subscribe(40.0, -74.0, 5);
        service.subscribe(40.0, -74.0, 60);

        QCOMPARE(service.effectiveIntervalMs(key), 5 * 60 * 1000);

        // Unsubscribe the 5-minute subscriber
        service.unsubscribe(40.0, -74.0, 5);
        QCOMPARE(service.effectiveIntervalMs(key), 60 * 60 * 1000);

        // Unsubscribe the 60-minute subscriber
        service.unsubscribe(40.0, -74.0, 60);
        QCOMPARE(service.subscriberCount(key), 0);
        QCOMPARE(service.effectiveIntervalMs(key), 0);
    }
};

QTEST_MAIN(TestWeatherService)
#include "test_weather_service.moc"
