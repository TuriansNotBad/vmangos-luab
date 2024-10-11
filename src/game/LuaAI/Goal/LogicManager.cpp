#include "LogicManager.h"
#include "LuaAI/LuaAgent.h"
#include "LuaAI/LuaAgentUtils.h"
#include "lua.hpp"

std::unordered_map<int, LogicInfo> LogicManager::logicInfoData;

void LogicManager::RegisterLogic(int logic_id, const char* logic_func, const char* logic_init, const char* logic_reset) {
	logicInfoData.emplace(logic_id, LogicInfo(logic_func, logic_init, logic_reset));
}

LogicManager::LogicManager(int logic_id) : logicId(logic_id) {
	// implement Me
	if (logicInfoData.find(logic_id) != logicInfoData.end()) {
		logicFunc = logicInfoData[logic_id].logicFunc;
		logicInit = logicInfoData[logic_id].logicInit;
		logicReset = logicInfoData[logic_id].logicReset;
	}
	else {
		logicFunc = "Logic ID Not Registered";
		logicInit = "Logic ID Not Registered";
		logicReset = "Logic ID Not Registered";
	}
}

void LogicManager::Init(lua_State* L, LuaAgent* ai) {

	lua_getglobal(L, logicInit.c_str());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetRef());
	if (lua_dopcall(L, 1, 0) != LUA_OK) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error calling logic: %s; in logic init %s\n", lua_tostring(L, -1), logicInit.c_str());
		lua_pop(L, 1);
		ai->SetCeaseUpdates();
	}

}

void LogicManager::Execute(lua_State* L, LuaAgent* ai) {

	lua_getglobal(L, logicFunc.c_str());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetRef());
	if (lua_dopcall(L, 1, 0) != LUA_OK) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error calling logic: %s; in logic function %s\n", lua_tostring(L, -1), logicFunc.c_str());
		lua_pop(L, 1);
		ai->SetCeaseUpdates();
	}

}


void LogicManager::Reset(lua_State* L, LuaAgent* ai) {
	if (logicReset.empty()) return;
	lua_getglobal(L, logicReset.c_str());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetRef());
	if (lua_dopcall(L, 1, 0) != LUA_OK) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error calling logic reset: %s; in logic reset %s\n", lua_tostring(L, -1), logicReset.c_str());
		lua_pop(L, 1);
		ai->SetCeaseUpdates();
	}
}

void LogicManager::Print() {
	printf("Logic [%d] Function [%s]\n", logicId, logicFunc.c_str());
	/*for ( auto v : logicInfoData )
	{
		printf( "ID: %d\n", v.first );
		printf( "Name: %s\n", v.second.logicFunc.c_str() );
		printf( "-------------------------------\n" );
	}*/

}

// ---------------------------------------------------------
//    LUA BINDS
// ---------------------------------------------------------

int LuaBindsAI::REGISTER_LOGIC_FUNC(lua_State* L) {
	if (lua_gettop(L) > 3)
		LogicManager::RegisterLogic(luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3), luaL_checkstring(L, 4));
	else
		LogicManager::RegisterLogic(luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3));
	return 0;
}

void LuaBindsAI::BindLogicManager(lua_State* L) {
	lua_register(L, "REGISTER_LOGIC_FUNC", REGISTER_LOGIC_FUNC);
}

