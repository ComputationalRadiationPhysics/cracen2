#pragma once

#include "cracen2/backend/Types.hpp"

namespace cracen2 {

namespace send_policies {

struct broadcast_any {

	template <class T, class Communicator, class RoleEndpointMap>
	void run(T&& message, Communicator& com, RoleEndpointMap& roleEndpointMap) {
		for(auto& roleEndpointVectorPair : roleEndpointMap) {
			for(auto& ep : roleEndpointVectorPair.second) {
				com.sendTo(message, ep);
			}
		}
	}

};

struct broadcast_role {

	backend::RoleId roleId;

	broadcast_role(backend::RoleId roleId) :
		roleId(roleId)
	{}

	template <class T, class Communicator, class RoleEndpointMap>
	void run(T&& message, Communicator& com, RoleEndpointMap& roleEndpointMap) {
		try {
			auto& epVec = roleEndpointMap.at(roleId);

			for(const auto& ep : epVec) {
				com.sendTo(message, ep);
			}
		} catch(const std::out_of_range&) {
		}
	}

};

} // End of namespace send_policies

} // End of namespace cracen2
