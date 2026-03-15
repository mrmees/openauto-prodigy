#pragma once

#include "IMediaStatusProvider.hpp"

namespace oap {

/// Extended media service interface beyond the narrow IMediaStatusProvider.
/// Currently empty — hasMedia moved to IMediaStatusProvider.
/// Kept as extension point for future service-only methods.
class IMediaStatusService : public IMediaStatusProvider {
    Q_OBJECT
public:
    using IMediaStatusProvider::IMediaStatusProvider;
};

} // namespace oap
