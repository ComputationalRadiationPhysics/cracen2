#pragma once

#include <tuple>
#include "cracen2/backend/Types.hpp"

namespace cracen2 {

namespace backend {

struct Register {};

struct RoleGraphRequest{};

struct AddRoleConnection{
	RoleId from;
	RoleId to;
};

struct RolesComplete{};

template <class Endpoint>
struct Embody {
	Endpoint endpoint;
	RoleId roleId;
};

template <class Endpoint>
struct Announce {
	Endpoint endpoint;
	RoleId roleId;
};

template <class Endpoint>
struct Disembody {
	Endpoint endpoint;
};

struct ServerClose {

};

template <class Endpoint>
using ServerTagList = std::tuple<
	backend::Register,
	backend::RoleGraphRequest,
	backend::AddRoleConnection,
	backend::RolesComplete,
	backend::Embody<Endpoint>,
	backend::Announce<Endpoint>,
	backend::Disembody<Endpoint>,
	backend::ServerClose
>;

} // End of namespace backend

} // End of namespace cracen2
