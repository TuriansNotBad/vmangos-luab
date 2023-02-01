#ifndef MANGOS_LuaUtils_H
#define MANGOS_LuaUtils_H

#include "lua.hpp"

bool luaL_checkboolean(lua_State* L, int idx);
void lua_pushplayerornil(lua_State* L, Player* u);
void lua_pushunitornil(lua_State* L, Unit* u);
void* luaL_checkudwithfield(lua_State* L, int idx, const char* fieldName);
namespace LuaBindsAI {
	bool IsValidHostileTarget(Unit* me, Unit const* pTarget);
}


#endif