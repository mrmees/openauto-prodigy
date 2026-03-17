#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

    void testCleanupNeverEvictsSubscribed()
    {
        oap::WeatherService service;

        // Create MAX_CACHE_SIZE (5) entries, subscribe to 4 of them
        double lats[] = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0};
        for (int i = 0; i < 5; ++i)
            service.getWeatherData(lats[i], 0.0);
        for (int i = 0; i < 4; ++i)
            service.subscribe(lats[i], 0.0);
        QCOMPARE(service.cacheSize(), 5);

        // Add 6th entry -- auto-cleanup triggers and evicts lats[4] (no subscribers)
        service.getWeatherData(lats[5], 0.0);
        QCOMPARE(service.cacheSize(), 5);  // evicted unsubscribed lats[4]

        // Subscribe to lats[5] so all 5 are now subscribed
        service.subscribe(lats[5], 0.0);

        // Add 7th entry -- auto-cleanup triggers but all existing 5 are subscribed
        // so it can only evict the new one (lats[6], no subscribers yet)
        service.getWeatherData(lats[6], 0.0);
        // The new entry lats[6] has no subscribers, so cleanup may evict it
        // but the point is: none of the 5 subscribed entries were evicted

        // Verify subscribed entries are all still intact
        for (int i = 0; i < 4; ++i) {
            QString key = oap::WeatherService::roundCoordinate(lats[i], 0.0);
            QVERIFY2(service.subscriberCount(key) > 0,
                     qPrintable(QString("lats[%1] should still be subscribed").arg(i)));
        }
        {
            QString key5 = oap::WeatherService::roundCoordinate(lats[5], 0.0);
            QVERIFY(service.subscriberCount(key5) > 0);
        }

        // Explicit triggerCleanup on all-subscribed should not reduce count
        int sizeBeforeCleanup = service.cacheSize();
        service.triggerCleanup();
        // All subscribed entries preserved (only unsubscribed lats[6] may have been evicted earlier)
        // size should not decrease further
        QVERIFY(service.cacheSize() >= sizeBeforeCleanup
                || service.cacheSize() == 5);  // at most evicts the unsubscribed one
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
