#ifndef MANGOS_LuaBotWorldObj_H
#define MANGOS_LuaBotWorldObj_H

#include "lua.hpp"

namespace LuaBindsAI {
	static const char* WorldObjMtName = "LuaAI.WorldObject";
	// Creates metatable for the AI userdata with name specified by AI::Unit_MTNAME
	void BindWorldObject(lua_State* L);
	// Creates metatable for the AI userdata with name specified by AI::Unit_MTNAME.
	// Registers all the functions listed in LuaBindsBot::Unit_BindLib with that metatable.
	void WObj_CreateMetatable(lua_State* L);
	WorldObject** WObj_GetWObjObject(lua_State* L, int idx = 1);

}

#endif