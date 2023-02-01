#include "LuaUtils.h"
#include "LuaLibPlayer.h"
#include "LuaLibUnit.h"

bool luaL_checkboolean(lua_State* L, int idx) {
	bool result = false;
	if (lua_isboolean(L, idx))
		result = lua_toboolean(L, idx);
	else
		luaL_error(L, "Invalid argument %d type. Boolean expected, got %s", idx, lua_typename(L, lua_type(L, idx)));
	return result;
}


void lua_pushplayerornil(lua_State* L, Player* u) {
	if (u)
		LuaBindsAI::Player_CreateUD(u, L);
	else
		lua_pushnil(L);
}


void lua_pushunitornil(lua_State* L, Unit* u) {
	if (u)
		LuaBindsAI::Unit_CreateUD(u, L);
	else
		lua_pushnil(L);
}


void* luaL_checkudwithfield(lua_State* L, int idx, const char* fieldName) {
	void* p = lua_touserdata(L, idx);
	if (p != NULL)  /* value is a userdata? */
		if (lua_getmetatable(L, idx)) {  /* does it have a metatable? */
			if (lua_getfield(L, -1, fieldName))
				if (lua_toboolean(L, -1)) {
					lua_pop(L, 2);  /* remove metatable and field value */
					return p;
				}
			lua_pop(L, 2);  /* remove metatable and field value */
		}
	luaL_error(L, "Invalid argument type. Userdata expected, got %s", lua_typename(L, lua_type(L, idx)));
}


// copied from combatbotbaseai.cpp
bool LuaBindsAI::IsValidHostileTarget(Unit* me, Unit const* pTarget) {
	return me->IsValidAttackTarget(pTarget) &&
		pTarget->IsVisibleForOrDetect(me, me, false) &&
		!pTarget->HasBreakableByDamageCrowdControlAura() &&
		!pTarget->IsTotalImmune() &&
		pTarget->GetTransport() == me->GetTransport();
}




