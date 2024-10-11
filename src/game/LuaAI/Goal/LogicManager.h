#pragma once

struct lua_State;
class LuaAgent;

struct LogicInfo {
	std::string logicFunc;
	std::string logicInit;
	std::string logicReset;
	LogicInfo() : logicFunc("BAD") {}
	LogicInfo(const char* logicFunc, const char* logicInit, const char* logicReset = "") : logicFunc(logicFunc), logicInit(logicInit), logicReset(logicReset) {}
};

class LogicManager {
	static std::unordered_map<int, LogicInfo> logicInfoData;
	std::string logicFunc;
	std::string logicInit;
	std::string logicReset;
	int logicId;

public:
	static void RegisterLogic(int logic_id, const char* logic_func, const char* logic_init, const char* logic_reset = "");

	LogicManager(int logic_id);

	void Init(lua_State* L, LuaAgent* ai);
	void Execute(lua_State* L, LuaAgent* ai);
	void Reset(lua_State* L, LuaAgent* ai);

	static void ClearRegistered() { logicInfoData.clear(); }
	void Print();

};

// ---------------------------------------------------------
//    LUA BINDS
// ---------------------------------------------------------

namespace LuaBindsAI {
	int REGISTER_LOGIC_FUNC(lua_State* L);
	void BindLogicManager(lua_State* L);
}