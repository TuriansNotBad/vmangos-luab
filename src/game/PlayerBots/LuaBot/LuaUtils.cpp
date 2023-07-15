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


int LuaBindsAI::Player_GetTalentRankUtil(Player* me, uint32 talentID, lua_State* L) {

	TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentID);
	if (!talentInfo)
		luaL_error(L, "AI.HasTalent: talent doesn't exist %d", talentID);

	TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
	if (!talentTabInfo)
		luaL_error(L, "AI.HasTalent: talent tab not found for talent %d", talentID);

	uint32 classMask = me->GetClassMask();
	if ((classMask & talentTabInfo->ClassMask) == 0)
		luaL_error(L, "AI.HasTalent: class mask and talent class mask do not match cls = %d, talent = %d", classMask, talentTabInfo->ClassMask);

	// search specified rank
	uint32 spellid = 0;
	for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank) {

		spellid = talentInfo->RankID[rank];
		if (spellid != 0) {

			SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellid);
			if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, me, false))
				continue;

			if (me->HasSpell(spellid))
				return rank;

		}
	}

	return -1;
}


void LuaBindsAI::Player_HasTalentUtil(Player* me, lua_State* L) {
	uint32 talentID = luaL_checkinteger(L, 2);

	TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentID);
	if (!talentInfo)
		luaL_error(L, "AI.HasTalent: talent doesn't exist %d", talentID);

	uint32 talentRank = luaL_checkinteger(L, 3);
	if (talentRank >= MAX_TALENT_RANK)
		luaL_error(L, "AI.HasTalent: talent rank cannot exceed %d", MAX_TALENT_RANK - 1);

	TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
	if (!talentTabInfo)
		luaL_error(L, "AI.HasTalent: talent tab not found for talent %d", talentID);

	uint32 classMask = me->GetClassMask();
	if ((classMask & talentTabInfo->ClassMask) == 0)
		luaL_error(L, "AI.HasTalent: class mask and talent class mask do not match cls = %d, talent = %d", classMask, talentTabInfo->ClassMask);

	// search specified rank
	uint32 spellid = talentInfo->RankID[talentRank];
	if (!spellid)                                       // ??? none spells in talent
		luaL_error(L, "AI.HasTalent: talent %d rank %d not found", talentID, talentRank);

	SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellid);
	if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, me, false))
		luaL_error(L, "AI.HasTalent: talent %d spell %d is not valid for player or doesn't exist", talentID, spellid);

	lua_pushboolean(L, me->HasSpell(spellid));
}





