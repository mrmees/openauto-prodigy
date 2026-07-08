#include "MediaArtProvider.hpp"

namespace oap {
namespace plugins {

void MediaArtProvider::setCurrentArt(const QImage& image) {
    QMutexLocker lock(&mutex_);
    art_ = image;
    ++revision_;
}

QString MediaArtProvider::currentUrl() const {
    QMutexLocker lock(&mutex_);
    if (art_.isNull()) return {};
    return QStringLiteral("image://mediaart/current/%1").arg(revision_);
}

QImage MediaArtProvider::requestImage(const QString& /*id*/, QSize* size,
                                      const QSize& requestedSize) {
    QMutexLocker lock(&mutex_);
    QImage img = art_;
    if (img.isNull()) {
        img = QImage(1, 1, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
    }
    if (requestedSize.isValid() && !requestedSize.isEmpty())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size) *size = img.size();
    return img;
}

} // namespace plugins
} // namespace oap
