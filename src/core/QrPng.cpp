#include "QrPng.hpp"

#include <QBuffer>
#include <QImage>
#include <qrcodegen.hpp>

namespace oap {

QString qrPngDataUri(const QString& payload)
{
    using namespace qrcodegen;
    QrCode qr = QrCode::encodeText(payload.toUtf8().constData(), QrCode::Ecc::MEDIUM);

    int size = qr.getSize();
    int border = 4;   // ISO 18004 quiet zone: 4 modules minimum
    int total = size + border * 2;
    int scale = 8;  // pixels per module — crisp at 200x200 display

    QImage image(total * scale, total * scale, QImage::Format_Grayscale8);
    image.fill(255);  // white

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (qr.getModule(x, y)) {
                int px = (x + border) * scale;
                int py = (y + border) * scale;
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        image.setPixel(px + dx, py + dy, qRgb(0, 0, 0));
            }
        }
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,")
           + QString::fromLatin1(pngData.toBase64());
}

} // namespace oap
