#include "HostContext.hpp"
#include <boost/log/trivial.hpp>

namespace oap {

void HostContext::log(LogLevel level, const QString& message)
{
    auto msg = message.toStdString();
    switch (level) {
        case LogLevel::Debug:   BOOST_LOG_TRIVIAL(debug) << "[plugin] " << msg; break;
        case LogLevel::Info:    BOOST_LOG_TRIVIAL(info) << "[plugin] " << msg; break;
        case LogLevel::Warning: BOOST_LOG_TRIVIAL(warning) << "[plugin] " << msg; break;
        case LogLevel::Error:   BOOST_LOG_TRIVIAL(error) << "[plugin] " << msg; break;
    }
}

} // namespace oap
