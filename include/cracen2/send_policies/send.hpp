#pragma once

#include "cracen2/backend/Types.hpp"

namespace cracen2 {

namespace send_policies {

struct send_any {

	template <class T, class RoleCommunicatorMap>
	void run(T&& message, RoleCommunicatorMap& roleCommunicatorMap) {
		auto comMapIter = roleCommunicatorMap.begin();
		if(comMapIter != roleCommunicatorMap.end()) {
			auto comVecIter = comMapIter->second.begin();
			if(comVecIter != comMapIter->second->end()) {
				comVecIter->send(std::forward(message));
			}
		}

	}

};

struct send_role {

	backend::RoleId role;

	send_role(backend::RoleId role) :
		role(role)
	{}

	template <class T, class RoleCommunicatorMap>
	void run(T&& message, backend::RoleId roleId, RoleCommunicatorMap& roleCommunicatorMap) {
		auto comMapIter = roleCommunicatorMap[role];
		if(comMapIter != roleCommunicatorMap.end()) {
			auto comVecIter = comMapIter->second.begin();
			if(comVecIter != comMapIter->second->end()) {
				comVecIter->send(std::forward(message));
			}
		}
	}

};

} // End of namespace send_policies

} // End of namespace cracen2

