#include "ITransport.hpp"
#include "GrpcTransport.hpp"
#include "ZmqTransport.hpp"

namespace delila {
namespace test {

std::unique_ptr<ITransport> TransportFactory::Create(TransportType type, ComponentType component) {
    switch (type) {
        case TransportType::GRPC:
            return nullptr;  // gRPC disabled due to linking issues
        case TransportType::ZEROMQ:
            return std::make_unique<ZmqTransport>(component);
        default:
            return nullptr;
    }
}

}} // namespace delila::test