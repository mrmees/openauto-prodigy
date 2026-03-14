#pragma once

#include "IMediaStatusProvider.hpp"

namespace oap {

/// Extended media service interface beyond the narrow IMediaStatusProvider.
class IMediaStatusService : public IMediaStatusProvider {
    Q_OBJECT
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaStatusChanged)
public:
    using IMediaStatusProvider::IMediaStatusProvider;

    virtual bool hasMedia() const = 0;
};

} // namespace oap
