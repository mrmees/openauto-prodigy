#pragma once
#include <QObject>

namespace oap {

class DisplayInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowSizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowSizeChanged)
    Q_PROPERTY(qreal screenSizeInches READ screenSizeInches NOTIFY screenSizeChanged)
    Q_PROPERTY(int computedDpi READ computedDpi NOTIFY windowSizeChanged)
    Q_PROPERTY(int gridColumns READ gridColumns NOTIFY gridDimensionsChanged)
    Q_PROPERTY(int gridRows READ gridRows NOTIFY gridDimensionsChanged)

public:
    explicit DisplayInfo(QObject* parent = nullptr);

    int windowWidth() const;
    int windowHeight() const;
    qreal screenSizeInches() const;
    int computedDpi() const;
    int gridColumns() const;
    int gridRows() const;

    void setWindowSize(int w, int h);
    void setScreenSizeInches(qreal inches);

signals:
    void windowSizeChanged();
    void screenSizeChanged();
    void gridDimensionsChanged();

private:
    void updateGridDimensions();

    int windowWidth_ = 1024;
    int windowHeight_ = 600;
    qreal screenSizeInches_ = 7.0;
    int gridColumns_ = 6;
    int gridRows_ = 4;
};

} // namespace oap
