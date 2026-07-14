#pragma once
#include <QString>

namespace oap {

// Renders `payload` as a QR code (ECC medium) and returns it as a
// base64 PNG data URI ready for a QML Image source. Used by the API
// pairing surface (ApiServer::pairingQrDataUri).
QString qrPngDataUri(const QString& payload);

} // namespace oap
