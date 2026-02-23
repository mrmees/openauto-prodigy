#include "HostContext.hpp"
#include <QDebug>

namespace oap {

void HostContext::log(LogLevel level, const QString& message)
{
    switch (level) {
        case LogLevel::Debug:   qDebug() << "[plugin] " << message; break;
        case LogLevel::Info:    qInfo() << "[plugin] " << message; break;
        case LogLevel::Warning: qWarning() << "[plugin] " << message; break;
        case LogLevel::Error:   qCritical() << "[plugin] " << message; break;
    }
}

} // namespace oap
