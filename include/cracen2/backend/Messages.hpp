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

struct Embody {};

struct Disembody {};

using ServerTagList = std::tuple<
	backend::Register,
	backend::RoleGraphRequest,
	backend::AddRoleConnection,
	backend::RolesComplete,
	backend::Embody,
	backend::Disembody
>;

} // End of namespace backend

} // End of namespace cracen2
