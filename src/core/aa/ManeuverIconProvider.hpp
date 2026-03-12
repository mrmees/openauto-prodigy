#pragma once

#include <QQuickImageProvider>
#include <QMutex>
#include <QMutexLocker>
#include <QImage>
#include <QByteArray>

namespace oap {
namespace aa {

class ManeuverIconProvider : public QQuickImageProvider {
public:
    ManeuverIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override
    {
        Q_UNUSED(id)
        QMutexLocker lock(&mutex_);

        if (currentIcon_.isEmpty()) {
            if (size)
                *size = QSize();
            return QImage();
        }

        QImage img;
        img.loadFromData(currentIcon_);

        if (img.isNull()) {
            if (size)
                *size = QSize();
            return QImage();
        }

        if (size)
            *size = img.size();

        if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0)
            img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        return img;
    }

    void updateIcon(const QByteArray& pngData)
    {
        QMutexLocker lock(&mutex_);
        currentIcon_ = pngData;
    }

private:
    QMutex mutex_;
    QByteArray currentIcon_;
};

} // namespace aa
} // namespace oap
