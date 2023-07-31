#include "LuaLibPlayer.h"
#include "LuaLibWorldObj.h"
#include "LuaLibUnit.h"
#include "LuaUtils.h"
#include "Group.h"
#include "PlayerAI.h"
#include "LuaBotAI.h"
#include "Spell.h"


void LuaBindsAI::Player_CreateUD(Player* player, lua_State* L) {
	// create userdata on top of the stack pointing to a pointer of an AI object
	Player** playerud = static_cast<Player**>(lua_newuserdatauv(L, sizeof(Player*), 0));
	*playerud = player; // swap the AI object being pointed to to the current instance
	luaL_setmetatable(L, LuaBindsAI::PlayerMtName);
}


void LuaBindsAI::BindPlayer(lua_State* L) {
	Player_CreateMetatable(L);
}


Player** LuaBindsAI::Player_GetPlayerObject(lua_State* L, int idx) {
	return (Player**) luaL_checkudata(L, idx, LuaBindsAI::PlayerMtName);
}


int LuaBindsAI_Player_CompareEquality(lua_State* L) {
	WorldObject* obj1 = *LuaBindsAI::WObj_GetWObjObject(L);
	WorldObject* obj2 = *LuaBindsAI::WObj_GetWObjObject(L, 2);
	lua_pushboolean(L, obj1 == obj2);
	return 1;
}
void LuaBindsAI::Player_CreateMetatable(lua_State* L) {
	luaL_newmetatable(L, LuaBindsAI::PlayerMtName);
	lua_pushvalue(L, -1); // copy mt cos setfield pops
	lua_setfield(L, -1, "__index"); // mt.__index = mt
	luaL_setfuncs(L, Unit_BindLib, 0); // copy funcs
	luaL_setfuncs(L, Player_BindLib, 0);
	lua_pushcfunction(L, LuaBindsAI_Player_CompareEquality);
	lua_setfield(L, -2, "__eq");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "isWorldObject");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "isUnit");
	lua_pop(L, 1); // pop mt
}


int LuaBindsAI::Player_InBattleGround(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	lua_pushboolean(L, player->InBattleGround());
	return 1;
}

int LuaBindsAI::Player_TeleportTo(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	int mapId = luaL_checkinteger(L, 2);
	float x = luaL_checknumber(L, 3);
	float y = luaL_checknumber(L, 4);
	float z = luaL_checknumber(L, 5);
	lua_pushboolean(L, player->TeleportTo(mapId, x, y, z, player->GetOrientation()));
	return 1;
}


int LuaBindsAI::Player_AddItem(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	uint32 id = luaL_checkinteger(L, 2);
	uint32 count = luaL_checkinteger(L, 3);
	if (player->AddItem(id, count)) {
		lua_pushboolean(L, true);
		return 1;
	}
	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Player_EquipItem(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	uint32 id = luaL_checkinteger(L, 2);
	uint32 enchantId = luaL_checkinteger(L, 3);

	for (auto const& itr : sObjectMgr.GetItemPrototypeMap()) {

		ItemPrototype const* pProto = &itr.second;
		if (pProto->ItemId == id) {

			if (pProto->RequiredLevel > player->GetLevel()) {
				lua_pushboolean(L, false);
				return 1;
			}

			uint32 slot = player->FindEquipSlot(pProto, NULL_SLOT, true);
			if (slot != NULL_SLOT)
				if (Item* pItem2 = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
					player->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);

			player->SatisfyItemRequirements(pProto);
			lua_pushboolean(L, player->StoreNewItemInBestSlots(pProto->ItemId, 1, enchantId));
			return 1;

		}

	}
	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Player_GetAI(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);

	// is lua bot?
	if (player->AI() && player->AI()->IsLuaBot())
		if (LuaBotAI* botAI = dynamic_cast<LuaBotAI*>(player->AI())) {
			botAI->PushUD(L);
			return 1;
		}

	return 0;
}


int LuaBindsAI::Player_IsLuaBot(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);

	// is lua bot?
	if (PlayerAI* pAI = player->AI()) {
		lua_pushboolean(L, pAI->IsLuaBot());
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Player_IsReady(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);

	// is lua bot?
	if (PlayerAI* pAI = player->AI())
		if (pAI->IsLuaBot())
			if (LuaBotAI* botAI = dynamic_cast<LuaBotAI*>(pAI))
				if (!botAI->IsReady()) {
					lua_pushboolean(L, false);
					return 1;
				}

	lua_pushboolean(L, player->IsInWorld() && !player->IsBeingTeleported());
	return 1;
}


int LuaBindsAI::Player_GetComboPoints(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	lua_pushnumber(L, player->GetComboPoints());
	return 1;
}


// -----------------------------------------------------------
//                      Spells
// -----------------------------------------------------------
int LuaBindsAI::Player_CastSpellAtInventoryItem(lua_State* L) {
	Player* caster = *Player_GetPlayerObject(L);
	Player* target = *Player_GetPlayerObject(L, 2);
	int spellId = luaL_checkinteger(L, 3);
	int itemSlot = luaL_checkinteger(L, 4);

	// mostly code from HandleCastSpellOpcode

	SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
	if (!spellInfo)
		luaL_error(L, "Player.CastSpellAtInventoryItem: spell not found. Id %d", spellId);

	if (itemSlot < EQUIPMENT_SLOT_START || itemSlot >= EQUIPMENT_SLOT_END)
		luaL_error(L, "Player.CastSpellAtInventoryItem: invalid equipment slot. Got %d, must be [%d, %d]", itemSlot, EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END);

	Item* item;
	item = target->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);
	if (!item)
		luaL_error(L, "Player.CastSpellAtInventoryItem: item not found. Slot %d, target %s, caster %s", itemSlot, target->GetName(), caster->GetName());

	SpellCastTargets targets;
	targets.m_targetMask = SpellCastTargetFlags::TARGET_FLAG_ITEM;
	targets.setItemTarget(item);

	SpellEntry const* originalSpellInfo = spellInfo;
	Spell* spell = new Spell(caster, spellInfo, false, ObjectGuid(), nullptr, nullptr);

	// Spell has been down-ranked, remember what client wanted to cast.
	if (spellInfo != originalSpellInfo)
		spell->m_originalSpellInfo = originalSpellInfo;

	// Nostalrius : Ivina
	spell->SetClientStarted(true);
	SpellCastResult result = spell->prepare(std::move(targets));
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::Player_LearnSpell(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	uint32 spellID = luaL_checkinteger(L, 2);

	const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellID);
	if (!spell)
		luaL_error(L, "Player.LearnSpell: Spell doesn't exist %d", spellID);

	player->LearnSpell(spellID, false, false);
	return 0;
}


int LuaBindsAI::Player_HasSpell(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	uint32 spellID = luaL_checkinteger(L, 2);

	const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellID);
	if (!spell)
		luaL_error(L, "Player.HasSpell: Spell doesn't exist %d", spellID);

	lua_pushboolean(L, player->HasSpell(spellID));
	return 1;
}


int LuaBindsAI::Player_GetTalentTbl(lua_State* L) {
	Player* me = *Player_GetPlayerObject(L);

	// from cs_learn.cpp
	Player* player = me;
	uint32 classMask = player->GetClassMask();

	lua_newtable(L);
	int tblIdx = 1;

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

		uint32 spellId = 0;
		uint8 rankId = MAX_TALENT_RANK;
		for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
		{
			if (talentInfo->RankID[rank] != 0)
			{
				rankId = rank;
				spellId = talentInfo->RankID[rank];
				break;
			}
		}

		if (!spellId || rankId == MAX_TALENT_RANK)
			continue;

		SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
		if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo))
			continue;

		lua_newtable(L);
		lua_newtable(L);
		for (lua_Integer rank = 0; rank <= rankId; ++rank)
			if (talentInfo->RankID[rank] != 0) {
				lua_pushinteger(L, talentInfo->RankID[rank]);
				lua_seti(L, -2, rank + 1);
			}
		lua_setfield(L, -2, "RankID");
		lua_pushinteger(L, spellId);
		lua_setfield(L, -2, "spellID");
		lua_pushinteger(L, rankId);
		lua_setfield(L, -2, "maxRankID");
		lua_pushinteger(L, talentInfo->TalentID);
		lua_setfield(L, -2, "TalentID");
		lua_pushinteger(L, talentInfo->Col);
		lua_setfield(L, -2, "Col");
		lua_pushinteger(L, talentInfo->Row);
		lua_setfield(L, -2, "Row");
		lua_pushinteger(L, talentInfo->TalentTab);
		lua_setfield(L, -2, "TalentTab");
		lua_pushinteger(L, talentInfo->DependsOn);
		lua_setfield(L, -2, "DependsOn");
		lua_pushinteger(L, talentInfo->DependsOnRank);
		lua_setfield(L, -2, "DependsOnRank");
		lua_pushstring(L, spellInfo->SpellName[0].c_str());
		lua_setfield(L, -2, "spellName");
		lua_pushinteger(L, talentTabInfo->TalentTabID);
		lua_setfield(L, -2, "tabID");
		lua_seti(L, -2, tblIdx);
		++tblIdx;
	}

	return 1;
}


int LuaBindsAI::Player_GetTalentRank(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	uint32 talentID = luaL_checkinteger(L, 2);
	lua_pushinteger(L, Player_GetTalentRankUtil(player, talentID, L));
	return 1;
}


int LuaBindsAI::Player_HasTalent(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	Player_HasTalentUtil(player, L);
	return 1;
}


int LuaBindsAI::Player_RemoveSpellCooldown(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
		player->RemoveSpellCooldown(*spell);
	else
		luaL_error(L, "Unit.RemoveSpellCooldown spell doesn't exist. Id = %d", spellId);
	return 0;
}


// -----------------------------------------------------------
//                      PARTY RELATED
// -----------------------------------------------------------


Player* GetPartyLeader(Player* me) {
	Group* pGroup = me->GetGroup();
	if (!pGroup) return nullptr;
	ObjectGuid leaderguid = pGroup->GetLeaderGuid();
	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
		if (Player* pMember = itr->getSource())
			if (pMember->GetGUID() == leaderguid)
				return pMember;
	return nullptr;
}
int LuaBindsAI::Player_GetPartyLeader(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	Player* leader = GetPartyLeader(player);
	lua_pushplayerornil(L, leader);
	return 1;
}


// Builds a 1d table of all group attackers
int LuaBindsAI::Player_GetGroupAttackersTbl(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	lua_newtable(L);
	int tblIdx = 1;
	Group* pGroup = player->GetGroup();
	//printf("Fetching attacker tbl\n");
	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
		if (Player* pMember = itr->getSource()) {

			//if (pMember->GetObjectGuid() == player->GetObjectGuid()) continue;
			for (const auto pAttacker : pMember->GetAttackers())
				if (IsValidHostileTarget(pMember, pAttacker)) {
					//printf("Additing target to tbl %s attacking %s\n", pAttacker->GetName(), pMember->GetName());
					Unit_CreateUD(pAttacker, L); // pushes pAttacker userdata on top of stack
					lua_seti(L, -2, tblIdx); // stack[-2][tblIdx] = stack[-1], pops pAttacker
					tblIdx++;
				}

			if (Pet* pPet = pMember->GetPet())
				for (const auto pAttacker : pPet->GetAttackers())
					if (IsValidHostileTarget(pMember, pAttacker)) {
						//printf("Additing target to tbl %s attacking %s\n", pAttacker->GetName(), pMember->GetName());
						Unit_CreateUD(pAttacker, L); // pushes pAttacker userdata on top of stack
						lua_seti(L, -2, tblIdx); // stack[-2][tblIdx] = stack[-1], pops pAttacker
						tblIdx++;
					}

		}
	return 1;
}


int LuaBindsAI::Player_GetGroupMemberCount(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	if (Group* grp = player->GetGroup())
		lua_pushinteger(L, grp->GetMembersCount());
	else
		lua_pushinteger(L, 1);
	return 1;
}


int LuaBindsAI::Player_GetGroupTank(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	Group* pGroup = player->GetGroup();
	//printf("Fetching attacker tbl\n");
	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
		if (Player* pMember = itr->getSource()) {
			if (PlayerAI* pAI = pMember->AI()) {
				if (pAI->IsLuaBot() && ((LuaBotAI*) pAI)->m_role == ROLE_TANK) {
					Player_CreateUD(pMember, L);
					return 1;
				}
			}
			
		}
	return 0;
}


// Builds a table of all group members userdatas
int LuaBindsAI::Player_GetGroupTbl(lua_State* L) {

	Player* player = *Player_GetPlayerObject(L);
	int tblIdx = 1;


	bool bots_only = false;
	bool exclude_self = true;
	bool safe_only = false;
	if (lua_gettop(L) > 1)
		bots_only = luaL_checkboolean(L, 2);

	if (lua_gettop(L) > 2)
		exclude_self = luaL_checkboolean(L, 3);

	if (lua_gettop(L) > 3)
		safe_only = luaL_checkboolean(L, 4);

	lua_newtable(L);
	Group* pGroup = player->GetGroup();
	if (!pGroup)
		return 1;

	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
		if (Player* pMember = itr->getSource()) {

			bool isBot = pMember->AI() && pMember->AI()->IsLuaBot();

			if (pMember == player && exclude_self) continue;
			if (bots_only && !isBot) continue;

			if (safe_only) {
				if (!pMember->IsInWorld() || pMember->IsBeingTeleported()) continue;
				if (isBot)
					if (LuaBotAI* botAI = dynamic_cast<LuaBotAI*>(pMember->AI())) {
						if (!botAI->IsInitialized()) continue;
					}
					else
						continue;

			}

			Player_CreateUD(pMember, L);
			lua_seti(L, -2, tblIdx); // stack[1][tblIdx] = stack[-1], pops pMember
			tblIdx++;
		}

	return 1;
}


// Builds a 2d table of all group member threat lists
int LuaBindsAI::Player_GetGroupThreatTbl(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	lua_newtable(L);
	int tblIdx = 1;
	Group* pGroup = player->GetGroup();
	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
		if (Player* pMember = itr->getSource()) {
			//if (pMember == player) continue;

			// main table
			lua_newtable(L);

			// player table
			int threatIdx = 2;
			Player_CreateUD(pMember, L);
			lua_seti(L, -2, 1); // playerTbl[1] = pMember, pops pMember
			for (auto v : pMember->GetThreatManager().getThreatList()) {
				if (Unit* hostile = v->getSourceUnit()) {
					Unit_CreateUD(hostile, L);
					lua_seti(L, -2, threatIdx); // playerTbl[threatIdx] = hostile, pops unit
					threatIdx++;
				}
			}

			lua_seti(L, -2, tblIdx); // mainTbl[i]={...}, pops inner tbl
			tblIdx++;
		}
	return 1;
}


int LuaBindsAI::Player_GetSubGroup(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	if (!player->GetGroup())
		lua_pushinteger(L, 0);
	else
		lua_pushinteger(L, player->GetSubGroup());
	return 1;
}


int LuaBindsAI::Player_IsGroupLeader(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	if (player->GetGroup() && player->GetGUID() == GetPartyLeader(player)->GetGUID())
		lua_pushboolean(L, true);
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Player_IsInGroup(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	if (player->GetGroup())
		lua_pushboolean(L, true);
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Player_GetRole(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	if (PlayerAI* pAI = player->AI())
		if (pAI->IsLuaBot()) {
			lua_pushinteger(L, ((LuaBotAI*) pAI)->GetRole());
			return 1;
		}
	return 0;
}


// -----------------------------------------------------------
//                    DEATH RELATED
// -----------------------------------------------------------


int LuaBindsAI::Player_BuildPlayerRepop(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	player->BuildPlayerRepop();
	return 0;
}


int LuaBindsAI::Player_ResurrectPlayer(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	int hp = luaL_checknumber(L, 2);
	bool bSick = luaL_checkboolean(L, 3);
	player->ResurrectPlayer(hp, bSick);
	return 0;
}


int LuaBindsAI::Player_RepopAtGraveyard(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	player->RepopAtGraveyard();
	return 0;
}


int LuaBindsAI::Player_SpawnCorpseBones(lua_State* L) {
	Player* player = *Player_GetPlayerObject(L);
	player->SpawnCorpseBones();
	return 0;
}


// -----------------------------------------------------------
//                    END DEATH RELATED
// -----------------------------------------------------------





