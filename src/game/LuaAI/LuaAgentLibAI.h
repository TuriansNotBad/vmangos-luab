#ifndef MANGOS_LuaAgentLibAI_H
#define MANGOS_LuaAgentLibAI_H

#include "lua.hpp"

class LuaAgent;

namespace LuaBindsAI {

	// Creates metatable for the AI userdata with name specified by AI::AI_MTNAME
	void BindAI(lua_State* L);
	// Creates metatable for the AI userdata with name specified by AI::AI_MTNAME.
	// Registers all the functions listed in LuaBindsAI::AI_BindLib with that metatable.
	void AI_CreateMetatable(lua_State* L);
	LuaAgent* AI_GetAIObject(lua_State* L, int idx = 1);

	int AI_HasReset(lua_State* L);

	int AI_AddTopGoal(lua_State* L);
	int AI_GetTopGoal(lua_State* L);
	int AI_HasTopGoal(lua_State* L);
	int AI_GetUserTbl(lua_State* L);

	int AI_GetForm(lua_State* L);
	int AI_SetForm(lua_State* L);
	int AI_GetDesiredLevel(lua_State* L);
	int AI_SetDesiredLevel(lua_State* L);
	int AI_GetStdThreat(lua_State* L);
	int AI_SetStdThreat(lua_State* L);
	int AI_GetHealTarget(lua_State* L);
	int AI_SetHealTarget(lua_State* L);
	int AI_GetCCTarget(lua_State* L);
	int AI_SetCCTarget(lua_State* L);

	// chase control

	int AI_GetChaseTarget(lua_State* L);
	int AI_GetChaseAngle(lua_State* L);
	int AI_GetChaseAngleT(lua_State* L);
	int AI_GetChaseUseAngle(lua_State* L);
	int AI_GetChaseDist(lua_State* L);
	int AI_GetChaseMinT(lua_State* L);
	int AI_GetChaseMaxT(lua_State* L);
	int AI_GetChaseSimple(lua_State* L);
	int AI_SetChaseAngle(lua_State* L);
	int AI_SetChaseAngleT(lua_State* L);
	int AI_SetChaseUseAngle(lua_State* L);
	int AI_SetChaseAngleValues(lua_State* L);
	int AI_SetChaseDist(lua_State* L);
	int AI_SetChaseMinT(lua_State* L);
	int AI_SetChaseMaxT(lua_State* L);
	int AI_SetChaseValues(lua_State* L);
	int AI_SetChaseSimple(lua_State* L);

	int AI_GetAngleForTanking(lua_State* L);
	int AI_GetPosForTanking(lua_State* L);
	int AI_IsCLineAvailable(lua_State* L);
	int AI_IsFollowing(lua_State* L);
	int AI_IsFalling(lua_State* L);
	int AI_IsMovingTo(lua_State* L);
	int AI_IsUsingAbsAngle(lua_State* L);
	int AI_Jump(lua_State* L);
	int AI_SetAbsAngle(lua_State* L);
	int AI_UnsetAbsAngle(lua_State* L);

	int AI_GoName(lua_State* L);

	// equip

	int AI_EquipCopyFromMaster(lua_State* L);
	int AI_EquipItem(lua_State* L);
	int AI_EquipDestroyAll(lua_State* L);
	int AI_EquipGetEnchantId(lua_State* L);
	int AI_EquipGetRandomProp(lua_State* L);
	int AI_EquipHasItemInSlot(lua_State* L);
	int AI_EquipPrint(lua_State* L);
	int AI_EquipSlotEmpty(lua_State* L);
	int AI_EquipSlotItemId(lua_State* L);
	int AI_UpdateVisibilityForMaster(lua_State* L);
	int AI_SetAmmo(lua_State* L);
	int AI_GetAmmo(lua_State* L);

	// commands

	int AI_CmdAddProgress(lua_State* L);
	int AI_CmdGetProgress(lua_State* L);
	int AI_CmdSetProgress(lua_State* L);
	int AI_CmdGetArgs(lua_State* L);
	int AI_CmdGetState(lua_State* L);
	int AI_CmdGetType(lua_State* L);
	int AI_CmdGetQMode(lua_State* L);
	int AI_CmdComplete(lua_State* L);
	int AI_CmdIsRequirementMet(lua_State* L);
	int AI_CmdSetInProgress(lua_State* L);
	int AI_CmdFail(lua_State* L);
	int AI_CmdPrintAll(lua_State* L);

	int AI_GetSpec(lua_State* L);
	int AI_GetRole(lua_State* L);
	int AI_GetPartyIntelligence(lua_State* L);
	int AI_GetPlayer(lua_State* L);
	int AI_GetMaster(lua_State* L);
	int AI_GetMasterGuid(lua_State* L);

	int AI_SetRole(lua_State* L);

	// spell

	int AI_GetSpellChainFirst(lua_State* L);
	int AI_GetSpellChainPrev(lua_State* L);
	int AI_GetSpellLevel(lua_State* L);
	int AI_GetSpellRank(lua_State* L);
	int AI_GetSpellOfRank(lua_State* L);
	int AI_GetSpellMaxRankForLevel(lua_State* L);
	int AI_GetSpellMaxRankForMe(lua_State* L);

	// chat

	int AI_ChatPopNextWhisper(lua_State* L);
	int AI_ChatSendInvToMaster(lua_State* L);
	int AI_ChatSendWhisper(lua_State* L);

	// trade

	int AI_TradeIsInProgress(lua_State* L);
	int AI_TradeInitiate(lua_State* L);
	int AI_TradeAccept(lua_State* L);
	int AI_TradeBegin(lua_State* L);
	int AI_TradeCancel(lua_State* L);
	int AI_TradeAddItem(lua_State* L);
	int AI_TradeItemIsInTrade(lua_State* L);
	int AI_TradeIsAccepted(lua_State* L);

	static const struct luaL_Reg AI_BindLib[]{
		{"HasReset", AI_HasReset},

		{"AddTopGoal", AI_AddTopGoal},
		{"GetTopGoal", AI_GetTopGoal},
		{"HasTopGoal", AI_HasTopGoal},
		{"GetData", AI_GetUserTbl},
		
		{"GetDesiredLevel", AI_GetDesiredLevel},
		{"SetDesiredLevel", AI_SetDesiredLevel},
		{"GetForm", AI_GetForm},
		{"SetForm", AI_SetForm},
		{"GetStdThreat", AI_GetStdThreat},
		{"SetStdThreat", AI_SetStdThreat},
		{"GetHealTarget", AI_GetHealTarget},
		{"SetHealTarget", AI_SetHealTarget},
		{"GetCCTarget", AI_GetCCTarget},
		{"SetCCTarget", AI_SetCCTarget},

		{"GetChaseTarget", AI_GetChaseTarget},
		{"GetChaseAngle", AI_GetChaseAngle},
		{"GetChaseAngleT", AI_GetChaseAngleT},
		{"GetChaseUseAngle", AI_GetChaseUseAngle},
		{"GetChaseDist", AI_GetChaseDist},
		{"GetChaseMinT", AI_GetChaseMinT},
		{"GetChaseMaxT", AI_GetChaseMaxT},
		{"GetChaseSimple", AI_GetChaseSimple},
		{"SetChaseAngleValues", AI_SetChaseAngleValues},
		{"SetChaseAngle", AI_SetChaseAngle},
		{"SetChaseAngleT", AI_SetChaseAngleT},
		{"SetChaseUseAngle", AI_SetChaseUseAngle},
		{"SetChaseDist", AI_SetChaseDist},
		{"SetChaseMinT", AI_SetChaseMinT},
		{"SetChaseMaxT", AI_SetChaseMaxT},
		{"SetChaseValues", AI_SetChaseValues},
		{"SetChaseSimple", AI_SetChaseSimple},

		{"GetPosForTanking", AI_GetPosForTanking},
		{"GetAngleForTanking", AI_GetAngleForTanking},
		{"IsCLineAvailable", AI_IsCLineAvailable},
		{"IsFalling", AI_IsFalling},
		{"IsFollowing", AI_IsFollowing},
		{"IsMovingTo", AI_IsMovingTo},
		{"IsUsingAbsAngle", AI_IsUsingAbsAngle},
		{"Jump", AI_Jump},
		{"SetAbsAngle", AI_SetAbsAngle},
		{"UnsetAbsAngle", AI_UnsetAbsAngle},

		{"GoName", AI_GoName},

		// equip
		{"EquipCopyFromMaster", AI_EquipCopyFromMaster},
		{"EquipItem", AI_EquipItem},
		{"EquipDestroyAll", AI_EquipDestroyAll},
		{"EquipGetEnchantId", AI_EquipGetEnchantId},
		{"EquipGetRandomProp", AI_EquipGetRandomProp},
		{"EquipHasItemInSlot", AI_EquipHasItemInSlot},
		{"EquipPrint", AI_EquipPrint},
		{"EquipSlotEmpty", AI_EquipSlotEmpty},
		{"EquipSlotItemId", AI_EquipSlotItemId},
		{"UpdateVisibilityForMaster", AI_UpdateVisibilityForMaster},
		{"GetAmmo", AI_GetAmmo},
		{"SetAmmo", AI_SetAmmo},

		// commands
		{"CmdAddProgress", AI_CmdAddProgress},
		{"CmdGetProgress", AI_CmdGetProgress},
		{"CmdSetProgress", AI_CmdSetProgress},
		{"CmdArgs", AI_CmdGetArgs},
		{"CmdState", AI_CmdGetState},
		{"CmdType", AI_CmdGetType},
		{"CmdIsQMode", AI_CmdGetQMode},
		{"CmdComplete", AI_CmdComplete},
		{"CmdIsRequirementMet", AI_CmdIsRequirementMet},
		{"CmdSetInProgress", AI_CmdSetInProgress},
		{"CmdFail", AI_CmdFail},
		{"CmdPrintAll", AI_CmdPrintAll},

		{"GetPartyIntelligence", AI_GetPartyIntelligence},
		{"GetRole", AI_GetRole},
		{"GetSpec", AI_GetSpec},
		{"GetMaster", AI_GetMaster},
		{"GetMasterGuid", AI_GetMasterGuid},
		{"GetPlayer", AI_GetPlayer},

		{"SetRole", AI_SetRole},

		// spell
		{"GetSpellChainFirst", AI_GetSpellChainFirst},
		{"GetSpellChainPrev", AI_GetSpellChainPrev},
		{"GetSpellLevel", AI_GetSpellLevel},
		{"GetSpellRank", AI_GetSpellRank},
		{"GetSpellOfRank", AI_GetSpellOfRank},
		{"GetSpellMaxRankForLevel", AI_GetSpellMaxRankForLevel},
		{"GetSpellMaxRankForMe", AI_GetSpellMaxRankForMe},
		
		// chat
		{"ChatPopNextWhisper", AI_ChatPopNextWhisper},
		{"ChatSendInvToMaster", AI_ChatSendInvToMaster},
		{"ChatSendWhisper", AI_ChatSendWhisper},
		
		// trade
		{"TradeIsInProgress", AI_TradeIsInProgress},
		{"TradeInitiate", AI_TradeInitiate},
		{"TradeAccept", AI_TradeAccept},
		{"TradeBegin", AI_TradeBegin},
		{"TradeCancel", AI_TradeCancel},
		{"TradeAddItem", AI_TradeAddItem},
		{"TradeItemIsInTrade", AI_TradeItemIsInTrade},
		{"TradeIsAccepted", AI_TradeIsAccepted},
		
		{NULL, NULL}
	};

}

#endif
