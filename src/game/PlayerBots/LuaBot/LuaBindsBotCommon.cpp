#include "LuaBindsBotCommon.h"
#include "GoalManager.h"
#include "LuaBotAI.h"
#include "LuaLibAI.h"
#include "LuaLibAux.h"

void LuaBindsAI::BindAll(lua_State* L) {
	BindGoalManager(L);
	BindGoal(L);
	BindLogicManager(L);
	BindAI(L);
	BindVmangos(L);
}

void LuaBindsAI::BindGlobalFunc(lua_State* L, const char* key, lua_CFunction func) {
	lua_pushcfunction(L, func);
	lua_setglobal(L, key);
}