#pragma once

#include "cracen2/backend/Types.hpp"

namespace cracen2 {

namespace send_policies {

struct broadcast_any {

	template <class RoleEndpointMap>
	auto run(RoleEndpointMap& roleEndpointMap) {
		using Endpoint = typename RoleEndpointMap::value_type::second_type::value_type;
		std::vector<Endpoint> sendToList;
		for(auto& roleEndpointVectorPair : roleEndpointMap) {
			for(auto& ep : roleEndpointVectorPair.second) {
				sendToList.emplace_back(ep);
			}
		}

		return sendToList;
	}

};

struct broadcast_role {

	backend::RoleId roleId;

	broadcast_role(backend::RoleId roleId) :
		roleId(roleId)
	{}

	template <class RoleEndpointMap>
	auto run(RoleEndpointMap& roleEndpointMap) {
		using Endpoint = typename RoleEndpointMap::value_type::second_type::value_type;
		std::vector<Endpoint> sendToList;
		try {
			auto& epVec = roleEndpointMap.at(roleId);

			for(const auto& ep : epVec) {
				sendToList.push_back(ep);
			}
		} catch(const std::out_of_range&) {
		}
		return sendToList;
	}

};

} // End of namespace send_policies

} // End of namespace cracen2
