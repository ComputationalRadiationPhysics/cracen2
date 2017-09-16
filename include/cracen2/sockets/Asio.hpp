#pragma once

#include "AsioDatagram.hpp"
#include "AsioStreaming.hpp"

namespace cracen2 {

namespace sockets {

enum class AsioProtocol {
	tcp,
	udp
};

/* Defining traits for the AsioSocket proxy */
template <AsioProtocol protocol>
struct AsioProtocolTrait;

template <>
struct AsioProtocolTrait<AsioProtocol::udp> {
	using type = AsioDatagramSocket;
};

template <>
struct AsioProtocolTrait<AsioProtocol::tcp> {
	using type = AsioStreamingSocket;
};

/* Proxy for tcp and udp implementation */
template <AsioProtocol protocol>
using AsioSocket = typename AsioProtocolTrait<protocol>::type;

} // End of namespace sockets

} // End of namespace cracen2
