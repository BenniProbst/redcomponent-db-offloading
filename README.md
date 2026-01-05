# redcomponent-db-offloading

**DB Offloading Module** - BASE module split for database offloading.

## Overview

Handles database offloading scenarios:
- Switches between endpoint and proxy mode on overload
- Uses std::variant for flexible protocol selection
- Integrates with sharding infrastructure

## Architecture

```cpp
using OffloadingProtocol = std::variant<
    IEndpointNetworkProtocol,
    IProxyNetworkProtocol
>;

class IOffloadingController {
public:
    enum class ProtocolMode { ENDPOINT, PROXY };

    ProtocolMode decide_mode(const ResourceStatus& status) {
        if (status.is_overloaded()) {
            return ProtocolMode::PROXY;  // Forward to another node
        }
        return ProtocolMode::ENDPOINT;   // Process locally
    }

    virtual void handle_offload(const DataBlock& data) = 0;
};
```

## Dependencies

- redcomponent-network-protocol-endpoint
- redcomponent-network-protocol-proxy
- redcomponent-storage-db

## Part of

- **Plan 14**: Storage Architecture
- **Product**: redcomponent-db

## License

Proprietary - BEP Venture UG
