#include "LogicManager.h"
#include "LuaBotAI.h"

std::unordered_map<int, LogicInfo> LogicManager::logicInfoData;

void LogicManager::RegisterLogic(int logic_id, const char* logic_func, const char* logic_init) {
	logicInfoData.emplace(logic_id, LogicInfo(logic_func, logic_init));
}

LogicManager::LogicManager(int logic_id) : logicId(logic_id) {
	// implement Me
	if (logicInfoData.find(logic_id) != logicInfoData.end()) {
		logicFunc = logicInfoData[logic_id].logicFunc;
		logicInit = logicInfoData[logic_id].logicInit;
	}
	else {
		logicFunc = "Logic ID Not Registered";
		logicInit = "Logic ID Not Registered";
	}
}

void LogicManager::Init(lua_State* L, LuaBotAI* ai) {

	lua_getglobal(L, logicInit.c_str());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetRef());
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error calling logic: %s; in logic init %s\n", lua_tostring(L, -1), logicInit.c_str());
		lua_pop(L, 1);
		ai->CeaseUpdates();
	}

}

void LogicManager::Execute(lua_State* L, LuaBotAI* ai) {

	lua_getglobal(L, logicFunc.c_str());
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetRef());
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error calling logic: %s; in logic function %s\n", lua_tostring(L, -1), logicFunc.c_str());
		lua_pop(L, 1);
		ai->CeaseUpdates();
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
	LogicManager::RegisterLogic(luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3));
	return 0;
}

void LuaBindsAI::BindLogicManager(lua_State* L) {
	lua_register(L, "REGISTER_LOGIC_FUNC", REGISTER_LOGIC_FUNC);
}

