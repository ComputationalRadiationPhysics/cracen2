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

	template <class T, class RoleCommunicatorMap>
	void run(T&& message, backend::RoleId roleId, RoleCommunicatorMap& roleCommunicatorMap) {
		auto& commVec = roleCommunicatorMap[roleId];
		for(auto& commPtr : commVec) {
			commPtr->send(message);
		}
	}

};

} // End of namespace send_policies

} // End of namespace cracen2
