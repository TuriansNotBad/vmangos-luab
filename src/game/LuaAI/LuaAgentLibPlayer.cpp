
#include "LuaAgentLibPlayer.h"
#include "LuaAgentLibWorldObj.h"
#include "LuaAgentLibUnit.h"
#include "LuaAgentUtils.h"
#include "Spell.h"
#include "Group.h"
#include "Bag.h"


namespace
{
	inline const TalentEntry* Player_TalentGetInfo(lua_State* L, Player* me, uint32 talentID)
	{
		TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentID);
		if (!talentInfo)
			luaL_error(L, "Player_TalentGetInfo: talent doesn't exist %d", talentID);

		TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
		if (!talentTabInfo)
			luaL_error(L, "Player_TalentGetInfo: talent tab not found for talent %d", talentID);

		uint32 classMask = me->GetClassMask();
		if ((classMask & talentTabInfo->ClassMask) == 0)
			luaL_error(L, "Player_TalentGetInfo: class mask and talent class mask do not match cls = %d, talent = %d", classMask, talentTabInfo->ClassMask);

		return talentInfo;
	}


	inline uint32 Player_TalentGetRankSpellId(lua_State* L, Player* me, const TalentEntry* talentInfo, uint32 talentID, uint32 rank)
	{
		if (rank >= MAX_TALENT_RANK)
			luaL_error(L, "Player_TalentGetRankSpellId: talent rank cannot exceed %d", MAX_TALENT_RANK - 1);

		// search specified rank
		uint32 spellid = talentInfo->RankID[rank];
		if (!spellid)                                       // ??? none spells in talent
			luaL_error(L, "Player_TalentGetRankSpellId: talent %d rank %d not found", talentID, rank);

		SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellid);
		if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, me, false))
			luaL_error(L, "Player_TalentGetRankSpellId: talent %d spell %d is not valid for player or doesn't exist", talentID, spellid);

		return spellid;
	}


	inline bool Player_CanLootItemCheck(Player* me, Loot& loot, LootItem* item, QuestItem* qitem, QuestItem* ffaitem)
	{
		if (!item || !item->AllowedForPlayer(me, loot.GetLootTarget()) || item->is_looted) return false;
		if (!qitem && item->is_blocked) return false;

		if (me->GetLootGuid().IsCreature() && !item->is_underthreshold && !qitem && !ffaitem)
			if (me->GetGroup() && me->GetGroup()->GetLootMethod() == MASTER_LOOT) return false;

		ItemPosCountVec dest;
		InventoryResult msg = me->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item->itemid, item->count);
		return msg == EQUIP_ERR_OK;
	}


	inline LootItem* Player_GetLootableItemInSlot(Player* me, int itemid, Loot& loot, int slot)
	{
		QuestItem* qitem = nullptr;
		QuestItem* ffaitem = nullptr;
		QuestItem* conditem = nullptr;
		LootItem* item = loot.LootItemInSlot(slot, me->GetGUIDLow(), &qitem, &ffaitem, &conditem);
		if (itemid <= 0 || item->itemid == itemid)
			if (Player_CanLootItemCheck(me, loot, item, qitem, ffaitem))
				return item;
		return nullptr;
	}


	inline void Player_ReleaseLoot(Player* me, ObjectGuid guid)
	{
		WorldPacket p(CMSG_LOOT_RELEASE);
		p << guid;
		me->GetSession()->HandleLootReleaseOpcode(p);
	}
}


void LuaBindsAI::Player_CreateUD(Player* player, lua_State* L) {
	// create userdata on top of the stack pointing to a pointer of an AI object
	Player** playerud = static_cast<Player**>(lua_newuserdatauv(L, sizeof(Player*), 0));
	*playerud = player; // swap the AI object being pointed to to the current instance
	luaL_setmetatable(L, LuaBindsAI::PlayerMtName);
}


void LuaBindsAI::BindPlayer(lua_State* L) {
	Player_CreateMetatable(L);
}


Player* LuaBindsAI::Player_GetPlayerObject(lua_State* L, int idx) {
	return *((Player**) luaL_checkudata(L, idx, LuaBindsAI::PlayerMtName));
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


int LuaBindsAI::Player_HasSpell(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	lua_Integer spellID = luaL_checkinteger(L, 2);
	if (!sSpellMgr.GetSpellEntry(spellID))
		luaL_error(L, "Player_HasSpell: spell %d not found", spellID);
	lua_pushboolean(L, me->HasSpell(spellID));
	return 1;
}


int LuaBindsAI::Player_LearnSpell(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	lua_Integer spellID = luaL_checkinteger(L, 2);
	if (!sSpellMgr.GetSpellEntry(spellID))
		luaL_error(L, "Player_LearnSpell: spell %d not found", spellID);
	if (!me->HasSpell(spellID))
		me->LearnSpell(spellID, false);
	return 0;
}


int LuaBindsAI::Player_UseItem(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	ObjectGuid guid = Guid_GetGuidObject(L, 2)->guid;
	lua_Integer itemId = luaL_checkinteger(L, 3);
	lua_Integer spellSlot = luaL_checkinteger(L, 4);

	const ItemPrototype* item = sObjectMgr.GetItemPrototype(itemId);
	if (itemId < 1 || !item)
		luaL_error(L, "Player_UseItem: item %I not found", itemId);
	if (spellSlot < 0 || spellSlot > 4)
		luaL_error(L, "Player_UseItem: invalid spell slot. Allowed [0, 4], got %I", spellSlot);

	const SpellEntry* spell = sSpellMgr.GetSpellEntry(item->Spells[spellSlot].SpellId);
	if (!spell)
		luaL_error(L, "Player_UseItem: item %I has no spell in slot %I, spell id was %u", itemId, spellSlot, item->Spells[spellSlot].SpellId);

	SpellCastTargets targets;
	if (guid.IsUnit())
	{
		targets.m_targetMask = SpellCastTargetFlags::TARGET_FLAG_UNIT;
		if (Unit* unit = me->GetMap()->GetUnit(guid))
			targets.setUnitTarget(unit);
		else
			return 0;
	}
	else if (guid.IsGameObject())
	{
		targets.m_targetMask = SpellCastTargetFlags::TARGET_FLAG_OBJECT;
		if (GameObject* go = me->GetMap()->GetGameObject(guid))
			targets.setGOTarget(go);
		else
			return 0;
	}
	else
		luaL_error(L, "Player_UseItem: provided target guid is of unsupported type: %s", guid.GetString().c_str());

	for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
		if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			if (pItem->GetEntry() == itemId)
			{
				WorldPacket p(CMSG_USE_ITEM);
				p << uint8(INVENTORY_SLOT_BAG_0) << uint8(i) << uint8(spellSlot) << targets;
				me->GetSession()->HandleUseItemOpcode(p);
				return 0;
			}
	
	for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
		if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			if (pItem->GetEntry() == itemId)
			{
				WorldPacket p(CMSG_USE_ITEM);
				p << uint8(INVENTORY_SLOT_BAG_0) << uint8(i) << uint8(spellSlot) << targets;
				me->GetSession()->HandleUseItemOpcode(p);
				return 0;
			}

	for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
		if (Bag* pBag = (Bag*) me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
				if (Item* pItem = pBag->GetItemByPos(j))
					if (pItem->GetEntry() == itemId)
					{
						WorldPacket p(CMSG_USE_ITEM);
						p << uint8(j) << uint8(i) << uint8(spellSlot) << targets;
						me->GetSession()->HandleUseItemOpcode(p);
						return 0;
					}
	
	luaL_error(L, "Player_UseItem: item %I not found in player inventory %s", itemId, me->GetName());
	return 0;
}


int LuaBindsAI::Player_GetGroupMemberCount(lua_State* L) {
	Player* player = Player_GetPlayerObject(L);
	if (Group* grp = player->GetGroup())
		lua_pushinteger(L, grp->GetMembersCount());
	else
		lua_pushinteger(L, 1);
	return 1;
}


int LuaBindsAI::Player_IsInSameSubGroup(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	Player* other = Player_GetPlayerObject(L, 2);
	lua_pushboolean(L, me->IsInSameGroupWith(other));
	return 1;
}


int LuaBindsAI::Player_GetLootMode(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	if (Group* group = me->GetGroup())
	{
		lua_pushinteger(L, group->GetLootMethod());
		return 1;
	}
	lua_pushinteger(L, LootMethod::FREE_FOR_ALL);
	return 1;
}


int LuaBindsAI::Player_CanLootCorpse(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	Unit* other = Unit_GetUnitObject(L, 2);
	lua_Integer itemid = luaL_checkinteger(L, 3);

	bool result = false;

	if (other->IsCreature() && other->IsDead())
	{
		Creature* c = static_cast<Creature*>(other);
		if (c->GetLootRecipientGuid() == me->GetObjectGuid())
		{
			Loot& loot = c->loot;
			result = loot.gold > 0;

			if (!result)
			{
				for (uint8 itemSlot = 0; itemSlot < loot.items.size(); ++itemSlot)
				{
					if (Player_GetLootableItemInSlot(me, itemid, loot, itemSlot))
					{
						result = true;
						break;
					}
				}
			}
		}
	}
	lua_pushboolean(L, result);
	return 1;
}


int LuaBindsAI::Player_LootCorpse(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	Unit* other = Unit_GetUnitObject(L, 2);
	lua_Integer itemid = luaL_checkinteger(L, 3); // loot all items if <= 0
	if (other->IsCreature() && other->IsDead())
	{
		lua_Integer found = 0;
		Creature* c = static_cast<Creature*>(other);
		if (c->GetLootRecipientGuid() == me->GetObjectGuid())
		{
			Loot& loot = c->loot;
			for (uint8 itemSlot = 0; itemSlot < loot.items.size(); ++itemSlot)
			{
				LootItem& lootItem = loot.items[itemSlot];
				ItemPrototype const* itemProto = sObjectMgr.GetItemPrototype(lootItem.itemid);
				if (!itemProto)
				{
					sLog.Out(LOG_BASIC, LOG_LVL_DEBUG, "Player_GetLootList: missing item prototype for item with id: %d", lootItem.itemid);
					continue;
				}
				if ((itemid <= 0 || lootItem.itemid == itemid) && !lootItem.is_looted)
				{
					me->SendLoot(c->GetObjectGuid(), LootType::LOOT_CORPSE);
					if (me->GetLootGuid() == c->GetObjectGuid())
					{
						if (!found) found = 1;
						if (loot.gold)
						{
							WorldPacket p(CMSG_LOOT_MONEY);
							me->GetSession()->HandleLootMoneyOpcode(p);
						}
						{
							WorldPacket p(CMSG_AUTOSTORE_LOOT_ITEM);
							p << itemSlot;
							me->GetSession()->HandleAutostoreLootItemOpcode(p);
						}
					}
					Player_ReleaseLoot(me, c->GetObjectGuid());
					// if only a specific item was requested we're done
					if (itemid > 0)
					{
						lua_pushinteger(L, found);
						return 1;
					}
				}
			}
			// done looting, send if found
			lua_pushinteger(L, found);
			return 1;
		}
		// not our loot
		lua_pushinteger(L, -1);
		return 1;
	}
	// not dead/not creature
	lua_pushinteger(L, -2);
	return 1;
}


int LuaBindsAI::Player_HasItemCount(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	lua_Integer itemId = luaL_checkinteger(L, 2);
	lua_Integer count = luaL_checkinteger(L, 3);
	if (itemId <= 0 || count <= 0)
		luaL_error(L, "Player_HasItemCount: item id and count must be > 0, got %I, %I", itemId, count);
	lua_pushboolean(L, me->HasItemCount(itemId, count));
	return 1;
}


int LuaBindsAI::Player_GetTalentTbl(lua_State* L) {
	Player* me = Player_GetPlayerObject(L);

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


int LuaBindsAI::Player_GetTalentRank(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	uint32 talentID = luaL_checkinteger(L, 2);
	const TalentEntry* talentInfo = Player_TalentGetInfo(L, me, talentID);

	// search for rank
	uint32 spellid = 0;
	int result = -1;
	for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
	{
		spellid = talentInfo->RankID[rank];
		if (spellid != 0)
		{
			SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellid);
			if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, me, false))
				continue;

			if (me->HasSpell(spellid))
			{
				result = rank;
				break;
			}
		}
	}

	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::Player_HasTalent(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	uint32 talentID = luaL_checkinteger(L, 2);
	uint32 spellid = Player_TalentGetRankSpellId(L, me, Player_TalentGetInfo(L, me, talentID), talentID, luaL_checkinteger(L, 3));
	lua_pushboolean(L, me->HasSpell(spellid));
	return 1;
}


int LuaBindsAI::Player_LearnTalent(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	uint32 talentID = luaL_checkinteger(L, 2);
	uint32 talentRank = luaL_checkinteger(L, 3);
	uint32 spellid = Player_TalentGetRankSpellId(L, me, Player_TalentGetInfo(L, me, talentID), talentID, talentRank);

	// learn highest rank of talent and learn all non-talent spell ranks (recursive by tree)
	me->LearnTalent(talentID, talentRank);
	return 0;
}


int LuaBindsAI::Player_ResetTalents(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	lua_pushboolean(L, me->ResetTalents(true));
	return 1;
}


int LuaBindsAI::Player_SendCastSpellUnit(lua_State* L)
{
	Player* me = Player_GetPlayerObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_Integer spellId = luaL_checkinteger(L, 3);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
	{
		std::unique_ptr<WorldPacket> packet = std::make_unique<WorldPacket>(CMSG_CAST_SPELL);
		*packet << spell->Id;
		SpellCastTargets targets;
		targets.setUnitTarget(target);
		*packet << targets;
		me->GetSession()->QueuePacket(std::move(packet));
	}
	else
		luaL_error(L, "Player_SendCastSpellUnit: spell %d doesn't exist", spellId);
	return 0;
}


int LuaBindsAI::Player_GetComboPoints(lua_State* L)
{
	Player* player = Player_GetPlayerObject(L);
	lua_pushinteger(L, player->GetComboPoints());
	return 1;
}


int LuaBindsAI::Player_ResurrectPlayer(lua_State* L) {
	Player* player = Player_GetPlayerObject(L);
	int hp = luaL_checknumber(L, 2);
	bool bSick = luaL_checkboolean(L, 3);
	player->ResurrectPlayer(hp, bSick);
	return 0;
}


int LuaBindsAI::Player_SetGameMaster(lua_State* L) {
	Player* player = Player_GetPlayerObject(L);
	bool on = luaL_checkboolean(L, 2);
	player->SetGameMaster(on);
	return 0;
}


int LuaBindsAI::Player_TeleportTo(lua_State* L) {
	Player* player = Player_GetPlayerObject(L);
	int mapId = luaL_checkinteger(L, 2);
	float x = luaL_checknumber(L, 3);
	float y = luaL_checknumber(L, 4);
	float z = luaL_checknumber(L, 5);
	lua_pushboolean(L, player->TeleportTo(mapId, x, y, z, player->GetOrientation()));
	return 1;
}
