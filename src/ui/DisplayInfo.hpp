#pragma once
#include <QObject>

namespace oap {

class DisplayInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowSizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowSizeChanged)

public:
    explicit DisplayInfo(QObject* parent = nullptr);

    int windowWidth() const;
    int windowHeight() const;

    void setWindowSize(int w, int h);

signals:
    void windowSizeChanged();

private:
    int windowWidth_ = 1024;
    int windowHeight_ = 600;
};

} // namespace oap
