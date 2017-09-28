#pragma once

#include "cracen2/backend/Types.hpp"

namespace cracen2 {

namespace send_policies {

struct broadcast_any {

	template <class T, class RoleCommunicatorMap>
	void run(T&& message, RoleCommunicatorMap& roleCommunicatorMap) {
		for(auto& roleCommVectorPair : roleCommunicatorMap) {
			for(auto& commPtr : roleCommVectorPair.second) {
				commPtr->send(message);
			}
		}
	}

};

struct broadcast_role {

	backend::RoleId roleId;

	broadcast_role(backend::RoleId roleId) :
		roleId(roleId)
	{}

	template <class T, class RoleCommunicatorMap>
	void run(T&& message, RoleCommunicatorMap& roleCommunicatorMap) {
		try {
		auto& commVec = roleCommunicatorMap.at(roleId);

		for(auto& commPtr : commVec) {
			commPtr->send(message);
		}
		} catch(const std::out_of_range&) {
		}
	}

};

} // End of namespace send_policies

} // End of namespace cracen2
