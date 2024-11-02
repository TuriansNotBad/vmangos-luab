
#include "LuaAgentCommandQ.h"
#include "lua.hpp"
#include "LuaAI/LuaAgentLibWorldObj.h"


int AgentCmd::PushType(lua_State* L)
{
	lua_pushinteger(L, lua_Integer(tp));
	return 1;
}


int AgentCmdMove::Push(lua_State* L)
{
	lua_pushnumber(L, destination.x);
	lua_pushnumber(L, destination.y);
	lua_pushnumber(L, destination.z);
	return 3;
}


int AgentCmdFollow::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	lua_pushnumber(L, dist);
	lua_pushnumber(L, angle);
	return 3;
}


int AgentCmdEngage::Push(lua_State* L)
{
	lua_pushnumber(L, angle);
	return 1;
}


int AgentCmdTank::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	lua_pushnumber(L, desiredThreat);
	return 2;
}


int AgentCmdHeal::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	return 1;
}


int AgentCmdPull::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	return 1;
}


int AgentCmdCC::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	return 1;
}


int AgentCmdBuff::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	lua_pushnumber(L, spellid);
	lua_pushstring(L, key.c_str());
	return 3;
}


int AgentCmdDispel::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	lua_pushstring(L, key.c_str());
	return 2;
}

int AgentCmdTrade::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	lua_pushinteger(L, bag);
	lua_pushinteger(L, slot);
	return 3;
}

int AgentCmdLoot::Push(lua_State* L)
{
	LuaBindsAI::Guid_CreateUD(L, targetGuid);
	lua_pushinteger(L, itemId);
	return 2;
}
