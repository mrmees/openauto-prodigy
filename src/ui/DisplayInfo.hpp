#pragma once
#include <QObject>

namespace oap {

class DisplayInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowSizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowSizeChanged)
    Q_PROPERTY(qreal screenSizeInches READ screenSizeInches NOTIFY screenSizeChanged)
    Q_PROPERTY(int computedDpi READ computedDpi NOTIFY windowSizeChanged)
    Q_PROPERTY(qreal cellSide READ cellSide NOTIFY cellSideChanged)
    Q_PROPERTY(int densityBias READ densityBias WRITE setDensityBias NOTIFY cellSideChanged)

public:
    explicit DisplayInfo(QObject* parent = nullptr);

    int windowWidth() const;
    int windowHeight() const;
    qreal screenSizeInches() const;
    int computedDpi() const;

    qreal cellSide() const;
    int densityBias() const;

    void setWindowSize(int w, int h);
    void setScreenSizeInches(qreal inches);
    void setDensityBias(int bias);
    void setFullscreen(bool fs);
    void setQScreenDpi(qreal dpi);
    void setConfigScreenSizeOverride(qreal inches);

    static constexpr qreal kBaseDivisor = 9.0;
    static constexpr qreal kBiasStep = 0.8;
    static constexpr qreal kMinCellSide = 80.0;

signals:
    void windowSizeChanged();
    void screenSizeChanged();
    void cellSideChanged();

private:
    void updateCellSide();

    int windowWidth_ = 1024;
    int windowHeight_ = 600;
    qreal screenSizeInches_ = 7.0;
    qreal cellSide_ = 0;
    int densityBias_ = 0;
    bool isFullscreen_ = false;
    qreal qscreenDpi_ = 0;
    qreal configScreenSizeOverride_ = 0;
};

} // namespace oap
