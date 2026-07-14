#pragma once
#include <QString>

namespace oap {

// Renders `payload` as a QR code (ECC medium) and returns it as a
// base64 PNG data URI ready for a QML Image source. Shared by the API
// pairing surface; the legacy CompanionListenerService keeps its private
// copy until the 9876 retirement (B2) deletes it.
QString qrPngDataUri(const QString& payload);

} // namespace oap
