
#include "LuaLibAI.h"
#include "LuaLibPlayer.h"
#include "LuaLibUnit.h"
#include "LuaLibWorldObj.h"
#include "LuaLibAux.h"
#include "LuaBotAI.h"

bool is_uint64(const std::string& s)
{
	std::string::const_iterator it = s.begin();
	while (it != s.end() && std::isdigit(*it)) ++it;
	return !s.empty() && it == s.end() && s.size() <= 20;
}

void LuaBindsAI::BindVmangos(lua_State* L) {
	BindPlayer(L);
	BindUnit(L);
	lua_register(L, "GetUnitByGuid", GetUnitByGuid);
	lua_register(L, "GetPlayerByGuid", GetPlayerByGuid);
}

uint64 LuaBindsAI::GetRawGuidFromString(lua_State* L, int n) {
	const char* guidStr = luaL_checkstring(L, n);
	if (is_uint64(guidStr)) {
		uint64 guid = 0;
		try {
			guid = std::stoull(guidStr);
			return guid;
		}
		catch (std::invalid_argument e) {
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "Failed to convert string to uint64, is_uint64 returned true. Str = %s", guidStr);
			luaL_error(L, "Exception caught. Passed string failed to convert to uint64 - %s\n", guidStr);
		}
		catch (std::out_of_range e) {
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "uint64 conversion overflow, is_uint64 returned true. Str = %s", guidStr);
			luaL_error(L, "Exception caught. Passed string overflowed uint64 - %s\n", guidStr);
		}
	}
	else
		luaL_error(L, "Passed string is not a valid uint64 number - %s\n", guidStr);
	return 0;
}

int LuaBindsAI::GetUnitByGuid(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	std::string guidStr = luaL_checkstring(L, 2);

	if (is_uint64(guidStr)) {
		uint64 guid = 0;
		try {
			guid = std::stoull(guidStr);
		}
		catch (std::invalid_argument e) {
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "Failed to convert string to uint64, is_uint64 returned true. Str = %s", guidStr);
			luaL_error(L, "Exception caught. Passed string failed to convert to uint64 - %s\n", guidStr);
		}
		catch (std::out_of_range e) {
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "uint64 conversion overflow, is_uint64 returned true. Str = %s", guidStr);
			luaL_error(L, "Exception caught. Passed string overflowed uint64 - %s\n", guidStr);
		}
		ObjectGuid oGuid(guid);
		Unit* unit = ai->me->GetMap()->GetUnit(oGuid);
		if (unit)
			Unit_CreateUD(unit, L);
		else
			lua_pushnil(L);
	}
	else
		luaL_error(L, "Passed string is not a valid uint64 number - %s\n", guidStr);
	return 1;
}


int LuaBindsAI::GetPlayerByGuid(lua_State* L) {
	std::string guidStr = luaL_checkstring(L, 1);
	if (is_uint64(guidStr)) {
		uint64 guid = 0;
		try {
			guid = std::stoull(guidStr);
		}
		catch (std::invalid_argument e) {
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "Failed to convert string to uint64, is_uint64 returned true. Str = %s", guidStr);
			luaL_error(L, "Exception caught. Passed string failed to convert to uint64 - %s\n", guidStr);
		}
		catch (std::out_of_range e) {
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "uint64 conversion overflow, is_uint64 returned true. Str = %s", guidStr);
			luaL_error(L, "Exception caught. Passed string overflowed uint64 - %s\n", guidStr);
		}
		ObjectGuid oGuid(guid);
		Player* player = ObjectAccessor::FindPlayer(oGuid);
		if (player)
			Player_CreateUD(player, L);
		else
			lua_pushnil(L);
	}
	else
		luaL_error(L, "Passed string is not a valid uint64 number - %s\n", guidStr);
	return 1;
}


float LuaBindsAI::GetAbsoluteAngle(float x, float y, float myX, float myY)
{
	return MapManager::NormalizeOrientation(std::atan2(
		static_cast<float>(y - myY),
		static_cast<float>(x - myX))
	);
}




