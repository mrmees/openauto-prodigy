#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

namespace oap {
namespace plugins {

/// Serves the current track's embedded cover art to QML as
/// image://mediaart/current/<rev>. The revision suffix busts QML's image
/// cache on track change. Thread-safe: requestImage() is called from QML's
/// image-loader threads while setCurrentArt() runs on the main thread.
class MediaArtProvider : public QQuickImageProvider {
public:
    MediaArtProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    /// Main thread. Null image = no art (currentUrl() becomes empty).
    void setCurrentArt(const QImage& image);

    /// "" when no art is available.
    QString currentUrl() const;

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    mutable QMutex mutex_;
    QImage art_;
    quint64 revision_ = 0;
};

} // namespace plugins
} // namespace oap
