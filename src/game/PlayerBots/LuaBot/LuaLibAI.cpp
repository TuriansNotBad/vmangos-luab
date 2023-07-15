
#include "LuaUtils.h"
#include "LuaLibAI.h"
#include "LuaBotAI.h"
#include "LuaLibUnit.h"
#include "MovementGenerator.h"
#include "PlayerBotMgr.h"
#include "Spell.h"

enum PartyBotSpells
{
	PB_SPELL_FOOD = 1131,
	PB_SPELL_DRINK = 1137,
	PB_SPELL_AUTO_SHOT = 75,
	PB_SPELL_SHOOT_WAND = 5019,
	PB_SPELL_HONORLESS_TARGET = 2479,
};


#define PB_UPDATE_INTERVAL 1000
#define PB_MIN_FOLLOW_DIST 3.0f
#define PB_MAX_FOLLOW_DIST 6.0f
#define PB_MIN_FOLLOW_ANGLE 0.0f
#define PB_MAX_FOLLOW_ANGLE 6.0f


void LuaBindsAI::BindAI( lua_State* L ) {
	AI_CreateMetatable( L );
}


LuaBotAI** LuaBindsAI::AI_GetAIObject( lua_State* L ) {
	return (LuaBotAI**) luaL_checkudata( L, 1, LuaBotAI::AI_MTNAME );
}


void LuaBindsAI::AI_CreateMetatable( lua_State* L ) {
	luaL_newmetatable( L, LuaBotAI::AI_MTNAME );
	lua_pushvalue( L, -1 ); // copy mt cos setfield pops
	lua_setfield( L, -1, "__index" ); // mt.__index = mt
	luaL_setfuncs( L, AI_BindLib, 0 ); // copy funcs
	lua_pop( L, 1 ); // pop mt
}


int LuaBindsAI::AI_AddTopGoal(lua_State* L) {

	int nArgs = lua_gettop(L);

	if (nArgs < 3) {
		luaL_error(L, "AI.AddTopGoal - invalid number of arguments. 3 min, %d given", nArgs);
		//return 0;
	}

	LuaBotAI** ai = AI_GetAIObject(L);
	int goalId = luaL_checknumber(L, 2);

	Goal* topGoal = (*ai)->GetTopGoal();

	if (goalId != topGoal->GetGoalId() || topGoal->GetTerminated()) {
		double life = luaL_checknumber(L, 3);

		// (*goal)->Print();

		std::vector<GoalParamP> params;
		Goal_GrabParams(L, nArgs, params);
		// printf( "%d\n", params.size() );

		Goal** goalUserdata = Goal_CreateGoalUD(L, (*ai)->AddTopGoal(goalId, life, params, L)); // ud on top of the stack
		// duplicate userdata for return result
		lua_pushvalue(L, -1);
		// save userdata
		(*goalUserdata)->SetRef(luaL_ref(L, LUA_REGISTRYINDEX)); // pops the object as well
		(*goalUserdata)->CreateUsertable();
	}
	else {
		// leave topgoal's userdata as return result for lua
		lua_rawgeti(L, LUA_REGISTRYINDEX, topGoal->GetRef());
	}

	return 1;

}


int LuaBindsAI::AI_HasTopGoal(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int id = luaL_checkinteger(L, 2);
	if (ai->GetTopGoal()->GetTerminated())
		lua_pushboolean(L, false);
	else
		lua_pushboolean(L, ai->GetTopGoal()->GetGoalId() == id);
	return 1;
}


int LuaBindsAI::AI_AddBot(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);

	Player* pLeader = sObjectAccessor.FindPlayer(ai->m_leaderGuid);
	if (!pLeader) return 0;

	uint8 race = luaL_checkinteger(L, 2);
	uint8 cls = luaL_checkinteger(L, 3);
	uint8 gender = luaL_checkinteger(L, 4);
	std::string spec = luaL_checkstring(L, 5);
	int logicID = luaL_checkinteger(L, 6);

	float x, y, z;
	pLeader->GetNearPoint(pLeader, x, y, z, 0, 5.0f, frand(0.0f, 6.0f));
	LuaBotAI* newBotAI = new LuaBotAI(pLeader, nullptr, race, cls, gender, pLeader->GetLevel(), pLeader->GetMapId(), pLeader->GetMap()->GetInstanceId(), x, y, z, pLeader->GetOrientation(), logicID, spec);
	if (sPlayerBotMgr.AddBot(newBotAI))
		if (Group* g = pLeader->GetGroup())
			if (g->GetMembersCount() == 5 && !g->isRaidGroup())
				g->ConvertToRaid();
	return 0;
}


int LuaBindsAI::AI_AddBotAt(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);

	Player* pLeader = sObjectAccessor.FindPlayer(ai->m_leaderGuid);
	if (!pLeader) return 0;

	uint8 race = luaL_checkinteger(L, 2);
	uint8 cls = luaL_checkinteger(L, 3);
	uint8 gender = luaL_checkinteger(L, 4);
	std::string spec = luaL_checkstring(L, 5);
	int logicID = luaL_checkinteger(L, 6);

	float x = luaL_checknumber(L, 7);
	float y = luaL_checknumber(L, 8);
	float z = luaL_checknumber(L, 9);
	LuaBotAI* newBotAI = new LuaBotAI(pLeader, nullptr, race, cls, gender, pLeader->GetLevel(), pLeader->GetMapId(), pLeader->GetMap()->GetInstanceId(), x, y, z, pLeader->GetOrientation(), logicID, spec);
	if (!sPlayerBotMgr.AddBot(newBotAI))
		delete newBotAI;

	return 0;
}


int LuaBindsAI::AI_RemoveBot(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* unit = *Unit_GetUnitObject(L, 2);
	sPlayerBotMgr.DeleteBot(unit->GetObjectGuid());
	return 0;
}


int LuaBindsAI::AI_DrinkAndEat(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	float healthPer = luaL_checknumber(L, 2);
	float manaPer = luaL_checknumber(L, 3);
	lua_pushboolean(L, ai->DrinkAndEat(healthPer, manaPer));
	return 1;
}


// -----------------------------------------------------------
//                      General Info
// -----------------------------------------------------------


int LuaBindsAI::AI_AreOthersOnSameTarget(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* unit = *Unit_GetUnitObject(L, 2);

	ObjectGuid guid = unit->GetObjectGuid();

	Group* pGroup = ai->me->GetGroup();
	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
	{
		if (Player* pMember = itr->getSource())
		{
			// Not self.
			if (pMember == ai->me)
				continue;

			if (pMember->GetTargetGuid() == guid)
			{
				lua_pushboolean(L, true);
				return 1;
			}
		}
	}
	

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::AI_GetClass(lua_State* L) {
	LuaBotAI** ai = AI_GetAIObject(L);
	lua_pushinteger(L, (*ai)->m_class);
	return 1;
}


int LuaBindsAI::AI_GetRole(lua_State* L) {
	LuaBotAI** ai = AI_GetAIObject(L);
	lua_pushinteger(L, (*ai)->GetRole());
	return 1;
}


int LuaBindsAI::AI_GetSpec(lua_State* L) {
	LuaBotAI** ai = AI_GetAIObject(L);
	lua_pushstring(L, (*ai)->GetSpec().c_str());
	return 1;
}


int LuaBindsAI::AI_GetStat(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int stat = luaL_checkinteger(L, 2);
	if (stat < 0 || stat >= MAX_STATS)
		luaL_error(L, "AI.GetStat: stat id must be within [0, %d] range", MAX_STATS - 1);
	lua_pushnumber(L, ai->me->GetStat(Stats(stat)));
	return 1;
}


int LuaBindsAI::AI_SetRole(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int role = luaL_checkinteger(L, 2);
	if (role <= LuaBotRoles::LBROLE_INVALID || role > LuaBotRoles::LBROLE_MAXROLE)
		luaL_error(L, "AI.SetRole: invalid role. Allowed values [1, %d]", LuaBotRoles::LBROLE_MAXROLE);
	ai->SetRole(LuaBotRoles(role));
	return 0;
}


int LuaBindsAI::AI_SetSpec(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	std::string spec = luaL_checkstring(L, 2);
	ai->SetSpec(spec);
	return 0;
}


// -----------------------------------------------------------
//                      Talents
// -----------------------------------------------------------


int LuaBindsAI::AI_GiveAllTalents(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	// From CharacterCommands.cpp
	Player* pPlayer = ai->me;
	uint32 classMask = pPlayer->GetClassMask();

	for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
	{
		TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
		if (!talentInfo)
			continue;

		TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
		if (!talentTabInfo)
			continue;

		if ((classMask & talentTabInfo->ClassMask) == 0)
			continue;

		// search highest talent rank
		uint32 spellid = 0;

		for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
		{
			if (talentInfo->RankID[rank] != 0)
			{
				spellid = talentInfo->RankID[rank];
				break;
			}
		}

		if (!spellid)                                       // ??? none spells in talent
			continue;

		SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellid);
		if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, pPlayer, false))
			continue;

		// learn highest rank of talent and learn all non-talent spell ranks (recursive by tree)
		pPlayer->LearnSpellHighRank(spellid);
	}
	return 0;
}


int LuaBindsAI::AI_LearnTalent(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	uint32 talentID = luaL_checkinteger(L, 2);

	TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentID);
	if (!talentInfo)
		luaL_error(L, "AI.LearnTalent: talent doesn't exist %d", talentID);

	uint32 talentRank = luaL_checkinteger(L, 3);
	if (talentRank >= MAX_TALENT_RANK)
		luaL_error(L, "AI.LearnTalent: talent rank cannot exceed %d", MAX_TALENT_RANK - 1);

	TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
	if (!talentTabInfo)
		luaL_error(L, "AI.LearnTalent: talent tab not found for talent %d", talentID);

	uint32 classMask = ai->me->GetClassMask();
	if ((classMask & talentTabInfo->ClassMask) == 0)
		luaL_error(L, "AI.LearnTalent: class mask and talent class mask do not match cls = %d, talent = %d", classMask, talentTabInfo->ClassMask);

	// search specified rank
	uint32 spellid = talentInfo->RankID[talentRank];
	if (!spellid)                                       // ??? none spells in talent
		luaL_error(L, "AI.LearnTalent: talent %d rank %d not found", talentID, talentRank);

	SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellid);
	if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, ai->me, false))
		luaL_error(L, "AI.LearnTalent: talent %d spell %d is not valid for player or doesn't exist", talentID, spellid);

	// learn highest rank of talent and learn all non-talent spell ranks (recursive by tree)
	ai->me->LearnTalent(talentID, talentRank);

	return 0;
}


int LuaBindsAI::AI_HasTalent(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Player_HasTalentUtil(ai->me, L);
	return 1;
}


int LuaBindsAI::AI_ResetTalents(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	lua_pushboolean(L, ai->me->ResetTalents(true));
	return 1;
}


// -----------------------------------------------------------
//                      Spells
// -----------------------------------------------------------


int LuaBindsAI::AI_GetSpellChainFirst(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellChainFirst(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellChainFirst: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellChainPrev(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellChainPrev(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellChainPrev: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellMaxRankForLevel(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	int level = luaL_checkinteger(L, 3);
	uint32 result = ai->GetSpellMaxRankForLevel(spellID, level);
	// this could be an error
	if (result == 0 && !sSpellMgr.GetSpellEntry(spellID))
		luaL_error(L, "AI.GetSpellMaxRankForLevel: spell doesn't exist. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellMaxRankForMe(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellMaxRankForMe(spellID);
	// this could be an error
	if (result == 0 && !sSpellMgr.GetSpellEntry(spellID))
		luaL_error(L, "AI.GetSpellMaxRankForMe: spell doesn't exist. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellName(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	std::string result = ai->GetSpellName(spellID);
	if (result.size() == 0)
		luaL_error(L, "AI.GetSpellName: spell not found. %d", spellID);
	lua_pushstring(L, result.c_str());
	return 1;
}


int LuaBindsAI::AI_GetSpellOfRank(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	int rank = luaL_checkinteger(L, 3);
	uint32 result = ai->GetSpellOfRank(spellID, rank);
	if (result == 0)
		luaL_error(L, "AI.GetSpellOfRank: error, check logs. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellRank(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellRank(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellRank: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_IsCastingHeal(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Spell* pCurrentSpell = ai->me->GetCurrentSpell(CurrentSpellTypes::CURRENT_GENERIC_SPELL);
	if (!pCurrentSpell)
		pCurrentSpell = ai->me->GetCurrentSpell(CurrentSpellTypes::CURRENT_CHANNELED_SPELL);
	if (!pCurrentSpell) {
		lua_pushboolean(L, false);
		return 1;
	}
	if (auto pSpellInfo = pCurrentSpell->m_spellInfo)
		lua_pushboolean(L, pSpellInfo->IsHealSpell());
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::AI_IsSpellReady(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellID))
		lua_pushboolean(L, ai->me->IsSpellReady(*spell));
	else
		luaL_error(L, "AI.IsSpellReady spell doesn't exist. Id = %d", spellID);
	return 1;
}


int LuaBindsAI::AI_IsValidDispelTarget(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	int spellID = luaL_checkinteger(L, 3);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellID))
		lua_pushboolean(L, ai->IsValidDispelTarget(target, spell));
	else
		luaL_error(L, "AI.IsValidDispelTarget spell doesn't exist. Id = %d", spellID);
	return 1;
}


// -----------------------------------------------------------
//                      Equip RELATED
// -----------------------------------------------------------

int LuaBindsAI::AI_EquipCopyFromMaster(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	lua_pushboolean(L, ai->EquipCopyFromMaster());
	return 1;
}


int LuaBindsAI::AI_EquipItem(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int itemID = luaL_checkinteger(L, 2);
	if (lua_gettop(L) > 2) {
		int randomPropertyID = luaL_checkinteger(L, 3);
		ai->EquipItem(itemID, randomPropertyID);
	}
	else
		ai->EquipItem(itemID);
	return 0;
}


int LuaBindsAI::AI_EquipRandomGear(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	// ai->EquipDestroyAll();
	ai->EquipRandomGearInEmptySlots();
	return 0;
}


int LuaBindsAI::AI_EquipDestroyAll(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	ai->EquipDestroyAll();
	return 0;
}


int LuaBindsAI::AI_EquipEnchant(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int enchantID = luaL_checkinteger(L, 2);
	int islot = luaL_checkinteger(L, 3);
	int iitemSlot = luaL_checkinteger(L, 4);
	int duration = luaL_checkinteger(L, 5);
	int charges = luaL_checkinteger(L, 6);

	if (iitemSlot < EQUIPMENT_SLOT_START || iitemSlot >= EQUIPMENT_SLOT_END)
		luaL_error(L, "AI.EquipEnchant: Invalid equipment slot. Allowed values - [%d, %d). Got %d", EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END, iitemSlot);

	if (islot < PERM_ENCHANTMENT_SLOT || islot > MAX_ENCHANTMENT_SLOT)
		luaL_error(L, "AI.EquipEnchant: Invalid enchantment slot. Allowed values - [%d, %d). Got %d", PERM_ENCHANTMENT_SLOT, MAX_ENCHANTMENT_SLOT, islot);

	ai->EquipEnchant(enchantID, EnchantmentSlot(islot), EquipmentSlots(iitemSlot), duration, charges);
	return 0;
}


int LuaBindsAI::AI_EquipGetEnchantId(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int islot = luaL_checkinteger(L, 2);
	int iitemSlot = luaL_checkinteger(L, 3);
	lua_pushinteger(L, ai->EquipGetEnchantId(EnchantmentSlot(islot), EquipmentSlots(iitemSlot)));
	return 1;
}


int LuaBindsAI::AI_EquipGetRandomProp(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	int iitemSlot = luaL_checkinteger(L, 2);
	lua_pushinteger(L, ai->EquipGetRandomProp(EquipmentSlots(iitemSlot)));
	return 1;
}


int LuaBindsAI::AI_EquipFindItemByName(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	std::string name(luaL_checkstring(L, 2));
	lua_pushinteger(L, ai->EquipFindItemByName(name));
	return 1;
}


// -----------------------------------------------------------
//                      Combat RELATED
// -----------------------------------------------------------


int LuaBindsAI::AI_AddAmmo(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	ai->AddHunterAmmo();
	return 0;
}


int LuaBindsAI::AI_AttackAutoshot(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pVictim = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	float chaseDist = luaL_checknumber(L, 3);
	ai->AttackAutoshot(pVictim, chaseDist);
	return 0;
}


int LuaBindsAI::AI_AttackSet(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pVictim = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	bool meleeAttack = luaL_checkboolean(L, 3);
	lua_pushboolean(L, ai->me->Attack(pVictim, meleeAttack));
	return 1;
}


int LuaBindsAI::AI_AttackSetChase(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pVictim = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	bool meleeAttack = luaL_checkboolean(L, 3);
	float chaseDist = luaL_checknumber(L, 4);
	bool attack = ai->me->Attack(pVictim, meleeAttack);
	lua_pushboolean(L, attack);
	if (attack)
		ai->me->GetMotionMaster()->MoveChase(pVictim, 1.0f);
	return 1;
}


int LuaBindsAI::AI_AttackStopAutoshot(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	ai->AttackStopAutoshot();
	return 0;
}


int LuaBindsAI::AI_AttackStart(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pVictim = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	Player* me = ai->me;

	if (me->IsMounted())
		me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

	if (me->Attack(pVictim, true))
	{
		if (ai->m_role == ROLE_RANGE_DPS &&
			me->GetPowerPercent(POWER_MANA) > 10.0f &&
			me->GetCombatDistance(pVictim) > 8.0f)
			me->SetCasterChaseDistance(25.0f);
		else if (me->HasDistanceCasterMovement())
			me->SetCasterChaseDistance(0.0f);

		me->GetMotionMaster()->MoveChase(pVictim, 1.0f, ai->m_role == ROLE_MELEE_DPS ? 3.0f : 0.0f);
		lua_pushboolean(L, true);
		return 1;
	}
	lua_pushboolean(L, false);
	return 1;

}


int LuaBindsAI::AI_AttackStop(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	bool targetSwitch = luaL_checkboolean(L, 2);
	lua_pushboolean(L, ai->me->AttackStop(targetSwitch));
	return 1;
}


int LuaBindsAI::AI_CanTryToCastSpell(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pTarget = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	int spellId = luaL_checkinteger(L, 3);
	bool bAura = luaL_checkboolean(L, 4);
	bool bGCD = luaL_checkboolean(L, 5);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
		lua_pushboolean(L, ai->CanCastSpell(pTarget, spell, bAura, bGCD));
	else
		luaL_error(L, "AI.CanTryToCastSpell spell doesn't exist. Id = %d", spellId);
	return 1;
}


int LuaBindsAI::AI_CanUseCrowdControl(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pTarget = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	int spellId = luaL_checkinteger(L, 3);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
		lua_pushboolean(L, ai->CanUseCrowdControl(spell, pTarget));
	else
		luaL_error(L, "AI.CanUseCrowdControl spell doesn't exist. Id = %d", spellId);
	return 1;
}


int LuaBindsAI::AI_CrowdControlMarkedTargets(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	ai->CrowdControlMarkedTargets();
	return 0;
}


int LuaBindsAI::AI_DoCastSpell(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pTarget = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	int spellId = luaL_checkinteger(L, 3);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
		lua_pushinteger(L, ai->DoCastSpell(pTarget, spell));
	else
		luaL_error(L, "AI.DoCastSpell spell doesn't exist. Id = %d", spellId);
	return 1;
}


int LuaBindsAI::AI_GetAttackersInRangeCount(lua_State* L) {
	LuaBotAI** ai = AI_GetAIObject(L);
	float r = luaL_checknumber(L, 2);
	lua_pushinteger(L, (*ai)->GetAttackersInRangeCount(r));
	return 1;
}




int LuaBindsAI::AI_GetMarkedTarget( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	int markId = luaL_checkinteger( L, 2 );
	if ( markId < RaidTargetIcon::RAID_TARGET_ICON_STAR || markId > RaidTargetIcon::RAID_TARGET_ICON_SKULL )
		luaL_error( L, "AI.GetMarkedTarget - invalid mark id. Acceptable values - [0, 7]. Got %d", markId );
	lua_pushunitornil( L, ( *ai )->GetMarkedTarget( (RaidTargetIcon) markId ) );
	return 1;
}



int LuaBindsAI::AI_GetGameTime(lua_State* L) {
	lua_pushinteger(L, sWorld.GetGameTime());
	return 1;
}


int LuaBindsAI::AI_RunAwayFromTarget(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	Unit* pTarget = *LuaBindsAI::Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, ai->me->GetMotionMaster()->MoveDistance(pTarget, 15.0f));
	return 1;
}


int LuaBindsAI::AI_SelectPartyAttackTarget( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	lua_pushunitornil( L, ( *ai )->SelectPartyAttackTarget() );
	return 1;
}


int LuaBindsAI::AI_SelectResurrectionTarget( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	lua_pushplayerornil( L, ( *ai )->SelectResurrectionTarget() );
	return 1;
}


int LuaBindsAI::AI_SelectShieldTarget( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	lua_pushplayerornil( L, ( *ai )->SelectShieldTarget() );
	return 1;
}


int LuaBindsAI::AI_SummonPetIfNeeded(lua_State* L) {
	LuaBotAI** ai = AI_GetAIObject(L);
	uint32 petId = luaL_checkinteger(L, 2);
	(*ai)->SummonPetIfNeeded(petId);
	return 0;
}


// -----------------------------------------------------------
//                      Event RELATED
// -----------------------------------------------------------

int LuaBindsAI::AI_GetEventRequest(lua_State* L) {
	LuaBotAI** ai = AI_GetAIObject(L);
	lua_pushinteger(L, (*ai)->GetEventRequest());
	return 1;
}


int LuaBindsAI::AI_GetUserTbl(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetUserTblRef());
	return 1;
}


int LuaBindsAI::AI_EquipOrUseNewItem(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	ai->EquipOrUseNewItem();
	return 0;
}


// -----------------------------------------------------------
//                      Movement RELATED
// -----------------------------------------------------------


int LuaBindsAI::AI_GoName( lua_State* L ) {
	LuaBotAI* ai = *AI_GetAIObject( L );
	char name[128] = {};
	strcpy( name, luaL_checkstring( L, 2 ) );
	ai->GonameCommandQueue(name);
	return 0;
}


int LuaBindsAI::AI_Mount( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	bool toMount = luaL_checkboolean( L, 2 );
	uint32 mountSpell = luaL_checknumber( L, 3 );
	( *ai )->Mount( toMount, mountSpell );
	return 0;
}


// -----------------------------------------------------------
//                    XP / LEVEL RELATED
// -----------------------------------------------------------


int LuaBindsAI::AI_InitTalentForLevel( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	( *ai )->me->InitTalentForLevel();
	return 0;
}


int LuaBindsAI::AI_GiveLevel( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	int level = luaL_checkinteger( L, 2 );
	( *ai )->me->GiveLevel( level );
	( *ai )->me->InitTalentForLevel();
	( *ai )->me->SetUInt32Value( PLAYER_XP, 0 );
	( *ai )->me->UpdateSkillsToMaxSkillsForLevel();
	( *ai )->ResetSpellData();
	( *ai )->PopulateSpellData();
	( *ai )->AddAllSpellReagents();
	( *ai )->me->SetHealthPercent( 100.0f );
	( *ai )->me->SetPowerPercent( ( *ai )->me->GetPowerType(), 100.0f );
	return 0;
}


int LuaBindsAI::AI_SetXP( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	int value = luaL_checkinteger( L, 2 );
	( *ai )->me->SetUInt32Value( PLAYER_XP, value );
	return 0;
}


// -----------------------------------------------------------
//                    DEATH RELATED
// -----------------------------------------------------------


int LuaBindsAI::AI_ShouldAutoRevive( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	lua_pushboolean( L, ( *ai )->ShouldAutoRevive() );
	return 1;
}


// -----------------------------------------------------------
//                    END DEATH RELATED
// -----------------------------------------------------------


int LuaBindsAI::AI_GetPlayer(lua_State* L) {
	LuaBotAI* ai = *AI_GetAIObject(L);
	ai->PushPlayerUD(L);
	return 1;
}


int LuaBindsAI::AI_Print( lua_State* L ) {
	LuaBotAI** ai = AI_GetAIObject( L );
	( *ai )->Print();
	return 0;
}
