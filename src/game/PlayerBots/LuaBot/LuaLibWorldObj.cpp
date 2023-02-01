#include "LuaLibWorldObj.h"
#include "LuaUtils.h"


void LuaBindsAI::BindWorldObject(lua_State* L) {
	WObj_CreateMetatable(L);
}


void LuaBindsAI::WObj_CreateMetatable(lua_State* L) {
	luaL_newmetatable(L, LuaBindsAI::WorldObjMtName);
	//lua_pushvalue(L, -1); // copy mt cos setfield pops
	//lua_setfield(L, -1, "__index"); // mt.__index = mt
	//luaL_setfuncs(L, Unit_BindLib, 0); // copy funcs
	lua_pop(L, 1); // pop mt
}


WorldObject** LuaBindsAI::WObj_GetWObjObject(lua_State* L, int idx) {
	return (WorldObject**) luaL_checkudwithfield(L, idx, "isWorldObject");
}



