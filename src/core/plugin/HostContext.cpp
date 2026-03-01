#include "HostContext.hpp"
#include "../Logging.hpp"

namespace oap {

void HostContext::log(LogLevel level, const QString& message)
{
    switch (level) {
        case LogLevel::Debug:   qCDebug(lcPlugin) << message; break;
        case LogLevel::Info:    qCInfo(lcPlugin) << message; break;
        case LogLevel::Warning: qCWarning(lcPlugin) << message; break;
        case LogLevel::Error:   qCCritical(lcPlugin) << message; break;
    }
}

} // namespace oap
