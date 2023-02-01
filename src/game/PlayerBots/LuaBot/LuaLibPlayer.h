#ifndef MANGOS_LuaLibPlayer_H
#define MANGOS_LuaLibPlayer_H

#include "lua.hpp"

namespace LuaBindsAI {
	static const char* PlayerMtName = "LuaAI.Player";

	// Creates metatable for the AI userdata with name specified by AI::Unit_MTNAME
	void BindPlayer(lua_State* L);
	// Creates metatable for the AI userdata with name specified by AI::Unit_MTNAME.
	// Registers all the functions listed in LuaBindsBot::Unit_BindLib with that metatable.
	void Player_CreateMetatable(lua_State* L);
	Player** Player_GetPlayerObject(lua_State* L, int idx = 1);
	void Player_CreateUD(Player* player, lua_State* L);

	// 
	int Player_InBattleGround(lua_State* L);

	int Player_TeleportTo(lua_State* L);

	int Player_AddItem(lua_State* L);
	int Player_EquipItem(lua_State* L);

	int Player_GetAI(lua_State* L);
	int Player_IsLuaBot(lua_State* L);
	int Player_IsReady(lua_State* L);

	int Player_GetComboPoints(lua_State* L);
	// spells

	int Player_LearnSpell(lua_State* L);
	int Player_HasSpell(lua_State* L);
	int Player_RemoveSpellCooldown(lua_State* L);

	// party related

	int Player_GetGroupAttackersTbl(lua_State* L);
	int Player_GetGroupMemberCount(lua_State* L);
	int Player_GetGroupTank(lua_State* L);
	int Player_GetGroupTbl(lua_State* L);
	int Player_GetGroupThreatTbl(lua_State* L);
	int Player_GetPartyLeader(lua_State* L);
	int Player_GetSubGroup(lua_State* L);
	int Player_IsGroupLeader(lua_State* L);
	int Player_IsInGroup(lua_State* L);
	int Player_GetRole(lua_State* L);

	// death related

	int Player_BuildPlayerRepop(lua_State* L);
	int Player_RepopAtGraveyard(lua_State* L);
	int Player_ResurrectPlayer(lua_State* L);
	int Player_SpawnCorpseBones(lua_State* L);


	static const struct luaL_Reg Player_BindLib[]{

		{"InBattleGround", Player_InBattleGround},
		{"TeleportTo", Player_TeleportTo},

		{"AddItem", Player_AddItem},
		{"EquipItem", Player_EquipItem},

		{"GetAI", Player_GetAI},
		{"IsLuaBot", Player_IsLuaBot},
		{"IsReady", Player_IsReady},

		{"GetComboPoints", Player_GetComboPoints},
		//spells
		{"LearnSpell", Player_LearnSpell},
		{"HasSpell", Player_HasSpell},
		{"RemoveSpellCooldown", Player_RemoveSpellCooldown},

		// party related
		{"GetGroupAttackersTbl", Player_GetGroupAttackersTbl},
		{"GetGroupMemberCount", Player_GetGroupMemberCount},
		{"GetGroupTank", Player_GetGroupTank},
		{"GetGroupTbl", Player_GetGroupTbl},
		{"GetGroupThreatTbl", Player_GetGroupThreatTbl},
		{"GetPartyLeader", Player_GetPartyLeader},
		{"GetSubGroup", Player_GetSubGroup},
		{"IsGroupLeader", Player_IsGroupLeader},
		{"IsInGroup", Player_IsInGroup},
		{"GetRole", Player_GetRole},

		// death related
		{"BuildPlayerRepop", Player_BuildPlayerRepop},
		{"RepopAtGraveyard", Player_RepopAtGraveyard},
		{"ResurrectPlayer", Player_ResurrectPlayer},
		{"SpawnCorpseBones", Player_SpawnCorpseBones},

		{NULL, NULL}
	};


}

#endif