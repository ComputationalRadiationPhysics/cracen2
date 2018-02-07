#pragma once

#include "cracen2/backend/Types.hpp"

namespace cracen2 {

namespace send_policies {

struct round_robin {

	backend::RoleId roleId;
	backend::RoleId counter;

	round_robin(backend::RoleId roleId) :
		roleId(roleId),
		counter(0)
	{}

	template <class RoleEndpointMap>
	auto run(RoleEndpointMap& roleEndpointMap) {
		using Endpoint = typename RoleEndpointMap::value_type::second_type::value_type;
		std::vector<Endpoint> sendToList;
		try {
			auto& epVec = roleEndpointMap.at(roleId);
			if(epVec.size() > 0) {
				counter = counter % epVec.size();
				sendToList.push_back(epVec.at(counter));
			}
		} catch(const std::out_of_range&) {
		}
		counter++;
		return sendToList;
	}

};

} // End of namespace send_policies

} // End of namespace cracen2


