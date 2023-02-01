#ifndef MANGOS_LuaLibAI_H
#define MANGOS_LuaLibAI_H

#include "lua.hpp"
class LuaBotAI;

namespace LuaBindsAI {

	// Creates metatable for the AI userdata with name specified by AI::AI_MTNAME
	void BindAI(lua_State* L);
	// Creates metatable for the AI userdata with name specified by AI::AI_MTNAME.
	// Registers all the functions listed in LuaBindsBot::AI_BindLib with that metatable.
	void AI_CreateMetatable(lua_State* L);
	LuaBotAI** AI_GetAIObject(lua_State* L);
	int AI_AddTopGoal(lua_State* L);
	int AI_HasTopGoal(lua_State* L);

	int AI_AddBot(lua_State* L);
	int AI_AddBotAt(lua_State* L);
	int AI_RemoveBot(lua_State* L);

	int AI_DrinkAndEat(lua_State* L);
	int AI_Print(lua_State* L);
	int AI_GetPlayer(lua_State* L);

	// gen info

	int AI_AreOthersOnSameTarget(lua_State* L);
	int AI_GetClass(lua_State* L);
	int AI_GetRole(lua_State* L);
	int AI_GetSpec(lua_State* L);
	int AI_GetStat(lua_State* L);
	int AI_SetRole(lua_State* L);
	int AI_SetSpec(lua_State* L);

	// spell

	int AI_GetSpellChainFirst(lua_State* L);
	int AI_GetSpellChainPrev(lua_State* L);
	int AI_GetSpellName(lua_State* L);
	int AI_GetSpellRank(lua_State* L);
	int AI_GetSpellOfRank(lua_State* L);
	int AI_GetSpellMaxRankForLevel(lua_State* L);
	int AI_GetSpellMaxRankForMe(lua_State* L);
	int AI_IsCastingHeal(lua_State* L);
	int AI_IsSpellReady(lua_State* L);
	int AI_IsValidDispelTarget(lua_State* L);

	// talents

	int AI_GetTalentTbl(lua_State* L);
	int AI_GiveAllTalents(lua_State* L);
	int AI_HasTalent(lua_State* L);
	int AI_LearnTalent(lua_State* L);
	int AI_ResetTalents(lua_State* L);

	// equip

	int AI_EquipCopyFromMaster(lua_State* L);
	int AI_EquipRandomGear(lua_State* L);
	int AI_EquipItem(lua_State* L);
	int AI_EquipDestroyAll(lua_State* L);
	int AI_EquipEnchant(lua_State* L);
	int AI_EquipFindItemByName(lua_State* L);

	// combat related

	int AI_AddAmmo(lua_State* L);
	int AI_AttackAutoshot(lua_State* L);
	int AI_AttackStart(lua_State* L);
	int AI_AttackSet(lua_State* L);
	int AI_AttackSetChase(lua_State* L);
	int AI_AttackStop(lua_State* L);
	int AI_AttackStopAutoshot(lua_State* L);

	int AI_CanTryToCastSpell(lua_State* L);
	int AI_CanUseCrowdControl(lua_State* L);
	int AI_CrowdControlMarkedTargets(lua_State* L);
	int AI_DoCastSpell(lua_State* L);

	int AI_GetAttackersInRangeCount(lua_State* L);
	int AI_GetMarkedTarget(lua_State* L);
	int AI_GetGameTime(lua_State* L);
	int AI_RunAwayFromTarget(lua_State* L);
	int AI_SelectPartyAttackTarget(lua_State* L);
	int AI_SelectResurrectionTarget(lua_State* L);
	int AI_SelectShieldTarget(lua_State* L);
	int AI_SummonPetIfNeeded(lua_State* L);

	// events
	int AI_GetEventRequest(lua_State* L);
	int AI_GetUserTbl(lua_State* L);

	// equip
	int AI_EquipOrUseNewItem(lua_State* L);

	// movement related
	int AI_GoName(lua_State* L);
	int AI_Mount(lua_State* L);

	// xp/level related
	int AI_InitTalentForLevel(lua_State* L);
	int AI_GiveLevel(lua_State* L);
	int AI_SetXP(lua_State* L);

	// death related
	int AI_ShouldAutoRevive(lua_State* L);


	static const struct luaL_Reg AI_BindLib[]{
		{"AddBot", AI_AddBot},
		{"AddBotAt", AI_AddBotAt},
		{"RemoveBot", AI_RemoveBot},

		{"AddTopGoal", AI_AddTopGoal},
		{"HasTopGoal", AI_HasTopGoal},

		{"DrinkAndEat", AI_DrinkAndEat},
		{"Print", AI_Print},
		{"GetPlayer", AI_GetPlayer},

		// gen info
		{"AreOthersOnSameTarget", AI_AreOthersOnSameTarget},
		{"GetClass", AI_GetClass},
		{"GetRole", AI_GetRole},
		{"GetSpec", AI_GetSpec},
		{"GetStat", AI_GetStat},
		{"SetRole", AI_SetRole},
		{"SetSpec", AI_SetSpec},

		// spells
		{"GetSpellChainFirst", AI_GetSpellChainFirst},
		{"GetSpellChainPrev", AI_GetSpellChainPrev},
		{"GetSpellName", AI_GetSpellName},
		{"GetSpellRank", AI_GetSpellRank},
		{"GetSpellOfRank", AI_GetSpellOfRank},
		{"GetSpellMaxRankForLevel", AI_GetSpellMaxRankForLevel},
		{"GetSpellMaxRankForMe", AI_GetSpellMaxRankForMe},
		{"IsCastingHeal", AI_IsCastingHeal},
		{"IsSpellReady", AI_IsSpellReady},
		{"IsValidDispelTarget", AI_IsValidDispelTarget},

		// equip
		{"EquipCopyFromMaster", AI_EquipCopyFromMaster},
		{"EquipRandomGear", AI_EquipRandomGear},
		{"EquipItem", AI_EquipItem},
		{"EquipDestroyAll", AI_EquipDestroyAll},
		{"EquipEnchant", AI_EquipEnchant},
		{"EquipFindItemByName", AI_EquipFindItemByName},

		// talents
		{"GetTalentTbl", AI_GetTalentTbl},
		{"GiveAllTalents", AI_GiveAllTalents},
		{"HasTalent", AI_HasTalent},
		{"LearnTalent", AI_LearnTalent},
		{"ResetTalents", AI_ResetTalents},

		// combat related
		{"AddAmmo", AI_AddAmmo},
		{"AttackAutoshot", AI_AttackAutoshot},
		{"AttackSet", AI_AttackSet},
		{"AttackSetChase", AI_AttackSetChase},
		{"AttackStart", AI_AttackStart},
		{"AttackStop", AI_AttackStop},
		{"AttackStopAutoshot", AI_AttackStopAutoshot},


		{"CanTryToCastSpell", AI_CanTryToCastSpell},
		{"CanUseCrowdControl", AI_CanUseCrowdControl},
		{"CrowdControlMarkedTargets", AI_CrowdControlMarkedTargets},
		{"DoCastSpell", AI_DoCastSpell},
		{"GetAttackersInRangeCount", AI_GetAttackersInRangeCount},
		{"GetMarkedTarget", AI_GetMarkedTarget},
		{"GetGameTime", AI_GetGameTime},
		{"RunAwayFromTarget", AI_RunAwayFromTarget},
		{"SelectPartyAttackTarget", AI_SelectPartyAttackTarget},
		{"SelectResurrectionTarget", AI_SelectResurrectionTarget},
		{"SelectShieldTarget", AI_SelectShieldTarget},
		{"SummonPetIfNeeded", AI_SummonPetIfNeeded},

		// events
		{"GetUserTbl", AI_GetUserTbl},
		{"GetEventRequest", AI_GetEventRequest},

		// equip
		{"EquipOrUseNewItem", AI_EquipOrUseNewItem},

		// movement related
		{"GoName", AI_GoName},
		{"Mount", AI_Mount},

		// xp/level related
		{"GiveLevel",AI_GiveLevel},
		{"InitTalentForLevel", AI_InitTalentForLevel},
		{"SetXP", AI_SetXP},

		// death related
		{"ShouldAutoRevive", AI_ShouldAutoRevive},

		{NULL, NULL}
	};

}

#endif
