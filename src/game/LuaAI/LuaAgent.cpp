
#include "LuaAgent.h"
#include "LuaAgentMgr.h"
#include "LuaAgentLibPlayer.h"
#include "lua.hpp"
#include "Bag.h"
#include "Chat.h"

const char* LuaAgent::AI_MTNAME = "Object.AI";

LuaAgent::LuaAgent(Player* me, ObjectGuid masterGuid, int logicID) :
	me(me),
	m_masterGuid(masterGuid),
	m_logic(logicID),
	m_spec(""),
	m_roleId(LuaAgentRoles::Invalid),

	m_updateInterval(50),
	m_updateSpeedInterval(10000),
	m_bCeaseUpdates(false),
	m_bInitialized(false),
	m_bCmdQueueMode(false),
	m_bHasReset(false),
	m_bSimpleChaseMode(false),
	m_bShouldGenGear(true),
	m_bHasGeneratedGear(false),

	m_userDataRef(LUA_NOREF),
	m_userDataRefPlayer(LUA_NOREF),
	m_userTblRef(LUA_NOREF),

	m_logicManager(logicID),
	m_goalManager(),
	m_topGoal(-1, 0, Goal::NOPARAMS, nullptr, nullptr),

	m_party(nullptr),

	m_stdThreat(10.f),
	m_ammo(0u),
	m_form(ShapeshiftForm::FORM_NONE),
	m_desiredLevel(-1),

	m_queueGoname(false),
	m_queueGonameName(""),

	m_chatMsgQ(),
	m_chatMaxQueueSz(10)
{
	m_updateTimer.Reset(2000);
	m_updateSpeedTimer.Reset(m_updateSpeedInterval);
	m_topGoal.SetTerminated(true);
}


LuaAgent::~LuaAgent() noexcept
{
	if (lua_State* L = sLuaAgentMgr.Lua())
	{
		Unref(L);
		UnrefPlayerUD(L);
		UnrefUserTbl(L);
	}
}


void LuaAgent::Update(uint32 diff)
{

	if (m_bCeaseUpdates || m_logic == -1)
		return;

	// catch up was queued
	if (m_queueGoname) {
		m_queueGoname = false;
		GonameCommand(m_queueGonameName);
		m_queueGonameName = "";
		return;
	}

	// not safe to update
	if (!me || !IsReady() || me->IsTaxiFlying())
		return;

	Fall();

	if (!IsInitialized())
	{
		Init();
		return;
	}

	if (GetDesiredLevel() != -1 && !me->IsInCombat() && me->GetLevel() != GetDesiredLevel())
	{
		me->GiveLevel(GetDesiredLevel());
		me->InitTalentForLevel();
		me->SetUInt32Value(PLAYER_XP, 0);
		Reset(false);
		return;
	}

	me->SetUInt32Value(PLAYER_XP, 0);

	// Not initialized
	if (m_userDataRef == LUA_NOREF || m_userDataRefPlayer == LUA_NOREF || m_userTblRef == LUA_NOREF)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgent: Attempt to update bot with uninitialized reference... [%d, %d, %d]. Ceasing.\n", m_userDataRef, m_userDataRefPlayer, m_userTblRef);
		SetCeaseUpdates();
		return;
	}

	// Did we corrupt the registry
	if (m_userDataRef == m_userDataRefPlayer || m_userDataRef == m_userTblRef || m_userTblRef == m_userDataRefPlayer)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAI Core: Lua registry error... [%d, %d, %d]. Ceasing.\n", m_userDataRef, m_userDataRefPlayer, m_userTblRef);
		SetCeaseUpdates();
		return;
	}

	m_updateSpeedTimer.Update(diff);
	if ((me->GetSpeedRate(UnitMoveType::MOVE_RUN) < 0.99f && !me->HasAuraType(AuraType::SPELL_AURA_MOD_DECREASE_SPEED)) || m_updateSpeedTimer.Passed())
	{
		m_updateSpeedTimer.Reset(m_updateSpeedInterval);
		me->UpdateSpeed(UnitMoveType::MOVE_RUN, false);
		me->UpdateSpeed(UnitMoveType::MOVE_SWIM, false);
	}

	lua_State* L = sLuaAgentMgr.Lua();
	m_logicManager.Execute(L, this);
	if (!m_topGoal.GetTerminated()) {
		m_goalManager.Activate(L, this);
		m_goalManager.Update(L, this);
		m_goalManager.Terminate(L, this);
	}

	if (m_bCmdQueueMode)
		m_bCmdQueueMode = false;

	// a manager called error state
	if (GetCeaseUpdates()) {
		m_goalManager = GoalManager();
		Reset(false);
	}
}


void LuaAgent::Reset(bool dropRefs)
{
	SetHealTarget(ObjectGuid());
	SetCCTarget(ObjectGuid());

	// clear goals
	m_topGoal = Goal(-1, 0, Goal::NOPARAMS, nullptr, nullptr);
	m_topGoal.SetTerminated(true);

	m_goalManager = GoalManager();
	m_logicManager = LogicManager(m_logic);

	// stop moving
	me->GetMotionMaster()->Clear();
	me->GetMotionMaster()->MoveIdle();

	// unmount
	if (me->IsMounted())
		me->RemoveSpellsCausingAura(AuraType::SPELL_AURA_MOUNTED);

	// stand up
	if (me->GetStandState() != UnitStandStateType::UNIT_STAND_STATE_STAND)
		me->SetStandState(UnitStandStateType::UNIT_STAND_STATE_STAND);

	// unshapeshift
	if (me->HasAuraType(AuraType::SPELL_AURA_MOD_SHAPESHIFT))
		me->RemoveSpellsCausingAura(AuraType::SPELL_AURA_MOD_SHAPESHIFT);

    me->SetWalk(false, true);

	// reset speed
	me->UpdateSpeed(UnitMoveType::MOVE_RUN, false);
	me->UpdateSpeed(UnitMoveType::MOVE_SWIM, false);

	// stop attacking
	me->AttackStop();

	lua_State* L = sLuaAgentMgr.Lua();
	if (dropRefs || !L) {
		m_userTblRef = LUA_NOREF;
		m_userDataRef = LUA_NOREF;
		m_userDataRefPlayer = LUA_NOREF;
	}
	else {
		if (!GetCeaseUpdates()) m_logicManager.Reset(L, this);
		// delete all refs
		Unref(L);
		UnrefPlayerUD(L);
		UnrefUserTbl(L);
		m_bHasReset = true;
	}
	m_bInitialized = false;
	m_queueGoname = false;
	m_queueGonameName = "";
	CommandsSetQMode(false);
	SetStdThreat(10.f);
	SetForm(ShapeshiftForm::FORM_NONE);
	SetHealTarget(ObjectGuid());
	SetCCTarget(ObjectGuid());
	SetAmmo(0u);
	m_updateTimer.Reset(500);
}


void LuaAgent::Init()
{
	lua_State* L = sLuaAgentMgr.Lua();

	commands.clear();
	if (m_userDataRef == LUA_NOREF)
		CreateUD(L);
	if (m_userDataRefPlayer == LUA_NOREF)
		CreatePlayerUD(L);
	if (m_userTblRef == LUA_NOREF)
		CreateUserTbl();

	me->UpdateSkillsToMaxSkillsForLevel();
	me->SetWaterBreathingIntervalMultiplier(9000.f);

	m_logicManager.Init(L, this);
	m_bInitialized = true;
}


void LuaAgent::UpdateSession(uint32 diff)
{
	if (me->IsBeingTeleported())
		if (me->IsBeingTeleportedNear())
		{
			WorldPacket p = WorldPacket(MSG_MOVE_TELEPORT_ACK);
			p << me->GetObjectGuid();
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_9_4
			p << me->GetLastCounterForMovementChangeType(TELEPORT);
#endif
			p << (uint32) time(nullptr); // time - not currently used
			me->GetSession()->HandleMoveTeleportAckOpcode(p);
			m_updateTimer.Reset(2000);
		}
		else if (me->IsBeingTeleportedFar())
		{
			me->GetSession()->HandleMoveWorldportAckOpcode();
			m_updateTimer.Reset(2000);
		}

	WorldSession* session = me->GetSession();
	for (int i = 0; i < PACKET_PROCESS_MAX_TYPE; i++)
	{
		WorldSessionFilter filter(session);
		filter.SetProcessType(PacketProcessing(i));
		session->Update(filter);
	}
}


void LuaAgent::OnPacketReceived(const WorldPacket& pck)
{
	if (!me || pck.empty())
		return;

	switch (pck.GetOpcode())
	{
	case SMSG_GROUP_INVITE:
	{
		WorldPacket pck(pck);
		std::string name;
		pck >> name;

		Player* leader = sObjectAccessor.FindPlayerByNameNotInWorld(name.c_str());
		if (!leader)
			return;

		if (!leader->GetGroup() && sLuaAgentMgr.IsGroupAllInProgress())
			// must assign group in the same frame
			me->GetSession()->HandleGroupAcceptOpcode(WorldPacket(CMSG_GROUP_ACCEPT));
		else
			me->GetSession()->QueuePacket(std::move(std::make_unique<WorldPacket>(CMSG_GROUP_ACCEPT)));

		break;
	}
	case SMSG_MOVE_KNOCK_BACK:
	{
		if (IsFalling())
			return;
		WorldPacket pck(pck);
		ObjectGuid guid;
		uint32 mCounter = 0u;
		float vcos, vsin, speedxy, speedz;
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_8_4
		guid = pck.readPackGUID();
#else
		pck >> guid;
#endif
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_9_4
		pck >> mCounter;
#endif
		pck >> vcos >> vsin >> speedxy >> speedz;
		MovementInfo movementInfo(me->m_movementInfo);
		movementInfo.moveFlags = 40961u;
		movementInfo.jump.cosAngle = vcos;
		movementInfo.jump.sinAngle = vsin;
		movementInfo.jump.xyspeed = speedxy;
		movementInfo.jump.zspeed = speedz;
		movementInfo.jump.startClientTime = WorldTimer::getMSTime();
		WorldPacket send(CMSG_MOVE_KNOCK_BACK_ACK);
		send << guid;
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_9_4
		send << mCounter;
#endif
		send << movementInfo;
		FallBegin(vcos, vsin, speedxy, speedz, WorldTimer::getMSTime());
		me->SetSplineDonePending(false);
		me->GetSession()->HandleMoveKnockBackAck(send);
		break;
	}
	case SMSG_NEW_WORLD:
	{
		std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(MSG_MOVE_WORLDPORT_ACK);
		me->GetSession()->QueuePacket(std::move(send));
		break;
	}
	case SMSG_PETITION_SHOW_SIGNATURES:
	{
		WorldPacket pck(pck);
		ObjectGuid itemGuid, ownerGuid;
		uint32 petitionGuid;
		uint8 signs;
		pck >> itemGuid >> ownerGuid >> petitionGuid >> signs;
		std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(CMSG_PETITION_SIGN);
		*send << itemGuid << uint8(0);
		me->GetSession()->QueuePacket(std::move(send));
		break;
	}
	case SMSG_MESSAGECHAT:
		if (m_chatMaxQueueSz)
		{
			WorldPacket pck(pck);
			OnChatMessage(pck);
		}
		break;
	}
}


void LuaAgent::OnLoggedIn()
{
	WorldPacket send(CMSG_SET_ACTIVE_MOVER);
	send << me->GetObjectGuid();
	me->GetSession()->HandleSetActiveMoverOpcode(send);
}


bool LuaAgent::IsReady()
{
	return me->IsInWorld() && !me->IsBeingTeleported() && !me->GetSession()->PlayerLoading();
}


Goal* LuaAgent::AddTopGoal(int goalId, double life, std::vector<GoalParamP>& goalParams, lua_State* L)
{
	m_topGoal = Goal(goalId, life, goalParams, &m_goalManager, L);
	m_goalManager = GoalManager(); // top goal owns all of the goals, we could just nuke the entire manager
	m_goalManager.PushGoalOnActivationStack(&m_topGoal);
	return &m_topGoal;
}


bool LuaAgent::EquipItem(uint32 itemId, uint32 enchantId, int32 randomPropertyId)
{
	return me->StoreNewItemInBestSlots(itemId, 1u, enchantId, randomPropertyId);
}


bool LuaAgent::EquipCopyFromMaster()
{
	Player* owner = ObjectAccessor::FindPlayer(m_masterGuid);
	if (!owner || owner->GetClass() != me->GetClass()) return false;

	for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
	{
		Item* item = owner->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
		if (!item || !item->GetProto()) continue;
		me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
		EquipItem(item->GetEntry(), item->GetEnchantmentId(EnchantmentSlot::PERM_ENCHANTMENT_SLOT), item->GetItemRandomPropertyId());
	}

	return true;
}


void LuaAgent::EquipDestroyAll(bool equipped)
{
	for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
		me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);

	if (equipped)
	{
		for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
			me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
		return;
	}

	for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
		if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);

	for (uint8 i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
		if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);

	for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
		if (Bag* pBag = (Bag*) me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
				if (Item* pItem = pBag->GetItemByPos(j))
					me->DestroyItem(i, j, true);
}


uint32 LuaAgent::EquipGetEnchantId(EnchantmentSlot slot, EquipmentSlots itemSlot)
{
	Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);
	return item ? item->GetEnchantmentId(slot) : 0;
}


int32 LuaAgent::EquipGetRandomProp(EquipmentSlots itemSlot)
{
	Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);
	return item ? item->GetItemRandomPropertyId() : 0;
}


bool LuaAgent::EquipHasItemInSlot(uint32 bag, uint32 slot, bool tradeCheck)
{
	if (!bag) bag = INVENTORY_SLOT_BAG_0;
	if (!tradeCheck) return me->GetItemByPos(bag, slot) != nullptr;
	if (Item* item = me->GetItemByPos(bag, slot)) return item->CanBeTraded();
	return false;
}


void LuaAgent::EquipPrint()
{
	for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
		if (Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			printf("%d %s %d\n", item->GetEntry(), item->GetProto()->Name1, item->GetItemRandomPropertyId());
}


void LuaAgent::UpdateVisibilityForMaster()
{
	if (Player* master = sObjectAccessor.FindPlayer(GetMasterGuid()))
	{
		me->SetVisibility(VISIBILITY_OFF);
		master->UpdateVisibilityOf(master, me);
		me->SetVisibility(VISIBILITY_ON);
	}
}


// Commands


void LuaAgent::CommandsPrint()
{
	for (int i = 0; i < commands.size(); ++i)
	{
		auto& cmd = commands[i];
		printf("%d - Type: %d, State: %d\n", i, (int) cmd->GetType(), (int) cmd->GetState());
	}
}


// Lua bits


void LuaAgent::CreateUserTbl()
{
	lua_State* L = sLuaAgentMgr.Lua();
	if (m_userTblRef == LUA_NOREF)
	{
		lua_newtable(L);
		m_userTblRef = luaL_ref(L, LUA_REGISTRYINDEX);
	}
}


void LuaAgent::CreateUD(lua_State* L)
{
	// create userdata on top of the stack pointing to a pointer of an AI object
	LuaAgent** aiud = static_cast<LuaAgent**>(lua_newuserdatauv(L, sizeof(LuaAgent*), 0));
	*aiud = this; // swap the AI object being pointed to to the current instance
	luaL_setmetatable(L, AI_MTNAME);
	// save this userdata in the registry table.
	m_userDataRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops
}


void LuaAgent::PushUD(lua_State* L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_userDataRef);
}


void LuaAgent::Unref(lua_State* L)
{
	if (m_userDataRef != LUA_NOREF && m_userDataRef != LUA_REFNIL)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, m_userDataRef);
		m_userDataRef = LUA_NOREF;
	}
}


void LuaAgent::CreatePlayerUD(lua_State* L)
{
	LuaBindsAI::Player_CreateUD(me, L);
	// save this userdata in the registry table.
	m_userDataRefPlayer = luaL_ref(L, LUA_REGISTRYINDEX); // pops
}


void LuaAgent::PushPlayerUD(lua_State* L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_userDataRefPlayer);
}


void LuaAgent::UnrefPlayerUD(lua_State* L)
{
	if (m_userDataRefPlayer != LUA_NOREF && m_userDataRefPlayer != LUA_REFNIL)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, m_userDataRefPlayer);
		m_userDataRefPlayer = LUA_NOREF;
	}
}

void LuaAgent::UnrefUserTbl(lua_State* L)
{
	if (m_userTblRef != LUA_NOREF && m_userTblRef != LUA_REFNIL)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, m_userTblRef);
		m_userTblRef = LUA_NOREF;
	}
}

// ========================================================================
// Spells Chains
// ========================================================================


uint32 LuaAgent::GetSpellChainFirst(uint32 spellID)
{

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellChainFirst: spell %d not found.", spellID);
		return 0;
	}

	auto info_first = sSpellMgr.GetFirstSpellInChain(spellID);
	if (!info_first)
		return spellID;

	return info_first;

}


uint32 LuaAgent::GetSpellChainPrev(uint32 spellID)
{

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellChainPrev: spell %d not found.", spellID);
		return 0;
	}

	auto info_prev = sSpellMgr.GetPrevSpellInChain(spellID);
	if (!info_prev)
		return spellID;

	return info_prev;

}


uint32 LuaAgent::GetSpellLevel(uint32 spellID)
{
	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellName: spell %d not found.", spellID);
		return 0;
	}

	return info_orig->spellLevel;
}


uint32 LuaAgent::GetSpellRank(uint32 spellID)
{
	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellRank: spell %d not found.", spellID);
		return 0;
	}

	auto chain = sSpellMgr.GetSpellChainNode(spellID);
	if (!chain)
		return 1;

	return chain->rank;
}


uint32 LuaAgent::GetSpellMaxRankForLevel(uint32 spellID, uint32 level)
{

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellMaxRankForLevel: spell %d not found.", spellID);
		return 0;
	}

	auto chain = sSpellMgr.GetSpellChainNode(spellID);
	// looks like there is only one rank
	if (!chain)
		return spellID;

	auto info_last = chain->first;
	auto info_final = info_orig;
	auto info_next = info_orig;
	while (level < info_next->spellLevel)
	{

		// we've ran out of ranks
		if (info_next->Id == info_last)
			return info_last;

		// update result
		info_final = info_next;

		info_next = sSpellMgr.GetSpellEntry(sSpellMgr.GetPrevSpellInChain(info_next->Id));
		// weird error?
		if (!info_next)
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellMaxRankForLevel: spell %d failed to find spell of next rank.", spellID);
			return 0;
		}

		if (info_next->Id == info_final->Id)
			return info_next->Id;

	}

	return info_next->Id;

}


uint32 LuaAgent::GetSpellMaxRankForMe(uint32 spellID)
{
	return GetSpellMaxRankForLevel(spellID, me->GetLevel());
}


uint32 LuaAgent::GetSpellOfRank(uint32 spellID, uint32 rank)
{

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d not found.", spellID);
		return 0;
	}

	auto chain = sSpellMgr.GetSpellChainNode(spellID);
	// looks like there is only one rank
	if (!chain)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d has no spell chain.", spellID);
		return 0;
	}

	auto info_last = chain->first;
	auto info_final = info_orig;
	auto info_next = info_orig;
	while (true)
	{

		// update result
		info_final = info_next;

		auto chain = sSpellMgr.GetSpellChainNode(info_final->Id);
		if (!chain)
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d has no spell chain.", spellID);
			return 0;
		}

		// found
		if (chain->rank == rank)
			return info_final->Id;

		// we've ran out of ranks
		if (info_next->Id == info_last)
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d ran out of ranks. Requested rank %d", spellID, rank);
			return 0;
		}

		info_next = sSpellMgr.GetSpellEntry(chain->prev);
		// weird error?
		if (!info_next)
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d failed to find spell of next rank.", spellID);
			return 0;
		}

		if (info_next->Id == info_final->Id)
			return info_next->Id;

	}

	return info_next->Id;

}


void LuaAgent::GonameCommandQueue(std::string name) {
	m_queueGoname = true;
	m_queueGonameName = name;
}


void LuaAgent::GonameCommand(std::string name) {
	// will crash if moving
	if (!me->IsStopped())
		me->StopMoving();
	me->GetMotionMaster()->Clear(false, true);
	me->GetMotionMaster()->MoveIdle();
	me->ClearTarget();
	m_fall = FallInfo();
	//m_topGoal = Goal(0, 0, Goal::NOPARAMS, nullptr, nullptr);
	//m_topGoal.SetTerminated(true);
	char namecopy[128] = {};
	strcpy(namecopy, name.c_str());
	ChatHandler(me).HandleGonameCommand(namecopy);
}


// ========================================================================
// Chat
// ========================================================================


void LuaAgent::OnChatMessage(WorldPacket& pck)
{
	uint8 msgtype;
	uint32 language;

	pck >> msgtype >> language;

	switch (msgtype)
	{
	case CHAT_MSG_WHISPER:
		ObjectGuid senderGuid;
		pck >> senderGuid;
		if (senderGuid == GetMasterGuid())
		{
			uint32 msglen;
			uint8 chatTag;
			pck >> msglen;
			char* msg = pck.ReadCString();
			pck >> chatTag;

			if (m_chatMsgQ.size() >= m_chatMaxQueueSz) m_chatMsgQ.pop();
			m_chatMsgQ.push(msg);
		}
		break;
	}

}


void LuaAgent::ChatSendWhisper(Player* recipient, const std::string& text)
{
	if (!recipient) return;
	WorldPacket data;
	ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language::LANG_UNIVERSAL, 0, me->GetObjectGuid(), me->GetName());
	recipient->GetSession()->SendPacket(&data);
}


void LuaAgent::ChatSendInvToMaster(bool ignoreEquipped)
{
	Player* recipient = ObjectAccessor::FindPlayer(GetMasterGuid());
	if (!recipient) return;

	for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
		if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
		{
			if (ignoreEquipped)
			{
				if (pItem->IsEquipped()) continue;
				if (pItem->IsBag() && i >= INVENTORY_SLOT_BAG_START && i < INVENTORY_SLOT_BAG_END) continue;
			}
			ChatSendWhisper(recipient, "0 " + std::to_string(i) + ". " + pItem->GetProto()->Name1);
		}

	for (uint8 i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
		if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			ChatSendWhisper(recipient, "0 " + std::to_string(i) + ". " + pItem->GetProto()->Name1);

	for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
		if (Bag* pBag = (Bag*) me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
			for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
				if (Item* pItem = pBag->GetItemByPos(j))
					ChatSendWhisper(recipient, std::to_string(i) + " " + std::to_string(j) + ". " + pItem->GetProto()->Name1);
}


// ========================================================================
// Trading
// ========================================================================


void LuaAgent::TradeInitiate(ObjectGuid guid)
{
	if (!me->GetTradeData())
	{
		std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(CMSG_INITIATE_TRADE);
		*send << guid;
		me->GetSession()->QueuePacket(std::move(send));
	}
}


void LuaAgent::TradeBegin()
{
	if (me->GetTradeData())
	{
		std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(CMSG_BEGIN_TRADE);
		me->GetSession()->QueuePacket(std::move(send));
	}
}


void LuaAgent::TradeAccept()
{
	if (TradeData* data = me->GetTradeData())
	{
		if (!data->IsAccepted())
		{
			std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(CMSG_ACCEPT_TRADE);
			*send << uint32(1);
			me->GetSession()->QueuePacket(std::move(send));
		}
	}
}


void LuaAgent::TradeCancel()
{
	if (TradeData* data = me->GetTradeData())
	{
		std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(CMSG_CANCEL_TRADE);
		me->GetSession()->QueuePacket(std::move(send));
	}
}


void LuaAgent::TradeAddItem(uint32 tradeSlot, uint32 bag, uint32 slot)
{
	if (TradeData* data = me->GetTradeData())
	{
		if (!bag) bag = INVENTORY_SLOT_BAG_0;
		Item* item = me->GetItemByPos(bag, slot);
		if (!item || data->HasItem(item->GetObjectGuid()) || data->GetItem(TradeSlots(tradeSlot)) || !item->CanBeTraded()) return;
		std::unique_ptr<WorldPacket> send = std::make_unique<WorldPacket>(CMSG_SET_TRADE_ITEM);
		*send << uint8(tradeSlot);
		*send << uint8(bag);
		*send << uint8(slot);
		me->GetSession()->QueuePacket(std::move(send));
	}
}


// ========================================================================
// Fall
// ========================================================================


namespace
{
	double gravity = 19.29110527038574;

	// Velocity bounds that makes fall speed limited
	float terminalVelocity = 60.148003f;
	float terminalSavefallVelocity = 7.f;

	float const terminal_length = float(terminalVelocity * terminalVelocity) / (2.f * gravity);
	float const terminal_savefall_length = (terminalSavefallVelocity * terminalSavefallVelocity) / (2.f * gravity);
	float const terminalFallTime = float(terminalVelocity / gravity); // the time that needed to reach terminalVelocity

	float computeFallElevation(float t_passed, bool isSafeFall, float start_velocity)
	{
		float termVel;
		float result;
		double g = -gravity;

		if (isSafeFall)
			termVel = terminalSavefallVelocity;
		else
			termVel = terminalVelocity;

		if (start_velocity > termVel)
			start_velocity = termVel;

		float terminal_time = terminalFallTime - start_velocity / gravity; // the time that needed to reach terminalVelocity

		if (t_passed > terminal_time)
		{
			result = terminalVelocity * (t_passed - terminal_time) +
				start_velocity * terminal_time + g * terminal_time * terminal_time * 0.5f;
		}
		else
			result = t_passed * (start_velocity + t_passed * g * 0.5f);

		return result;
	}
}


void LuaAgent::FallBegin(float vcos, float vsin, float speedxy, float speedz, uint32 beginT)
{
	if (IsFalling())
		return;
	m_fall.vcos = vcos;
	m_fall.vsin = vsin;
	m_fall.speedxy = speedxy;
	m_fall.speedz = speedz;
	m_fall.falling = true;
	m_fall.lastT = beginT;
	m_fall.beginT = beginT;
	m_fall.startX = me->GetPositionX();
	m_fall.startY = me->GetPositionY();
	m_fall.startZ = me->GetPositionZ();

	m_fall.lastX = m_fall.startX;
	m_fall.lastY = m_fall.startY;
	m_fall.lastZ = m_fall.startZ;
	// printf("Fall start at (%f %f %f)\n", m_fall.startX, m_fall.startY, m_fall.startZ);
	me->StopMoving();
}

void LuaAgent::Fall()
{
	if (!IsFalling())
		return;

	float x, y, z;
	float diffT = WorldTimer::getMSTimeDiffToNow(m_fall.beginT) / 1000.0f;

	if (diffT < 0.05)
		return;

	x = m_fall.startX;
	y = m_fall.startY;
	z = m_fall.startZ;
	if (!x || !y || !z || diffT > 10000.0f)
		FallEnd(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
	x += m_fall.vcos * m_fall.speedxy * diffT;
	y += m_fall.vsin * m_fall.speedxy * diffT;
	float groundZ = me->GetMap()->GetHeight(x, y, me->GetPositionZ());
	z += computeFallElevation(diffT, me->m_movementInfo.moveFlags & MOVEFLAG_SAFE_FALL, -m_fall.speedz);

	if (z <= groundZ)
		z = groundZ;

	bool mm_fail;
	{
		float wasX = m_fall.lastX, wasY = m_fall.lastY, wasZ = m_fall.lastZ;
		float x2 = x, y2 = y, z2 = z;
		mm_fail = !me->IsWithinLOSAtPosition(wasX, wasY, wasZ, x, y, z);
		if (mm_fail)
		{
			x = wasX;
			y = wasY;
			float wasGroundZ = me->GetMap()->GetHeight(wasX, wasY, wasZ);
			if (z == groundZ)
				z = wasGroundZ;
			else if (z <= wasGroundZ)
				z = wasGroundZ;
			groundZ = wasGroundZ;
		}
	}

	m_fall.lastX = x;
	m_fall.lastY = y;
	m_fall.lastZ = z;

	//printf("%d (%f %f %f) (%f %f %f)\n", mm_fail, m_fall.startX, m_fall.startY, m_fall.startZ, x, y, z);
	//printf("%f %f %f %f\n", m_fall.vcos * m_fall.speedxy * diffT, m_fall.vcos, m_fall.speedxy, diffT);
	//printf("%f %f %f %f\n", m_fall.vsin * m_fall.speedxy * diffT, m_fall.vsin, m_fall.speedxy, diffT);

	if (z == groundZ) {
		FallEnd(x, y, z);
		return;
	}

	//if (WorldTimer::getMSTime() - m_fall.lastT < 400u)
	//	return;
	//m_fall.lastT = WorldTimer::getMSTime();

	//MovementInfo movementInfo(me->m_movementInfo);
	//movementInfo.moveFlags = 40961u;
	//movementInfo.ctime = WorldTimer::getMSTime();
	//movementInfo.jump.cosAngle = m_fall.vcos;
	//movementInfo.jump.sinAngle = m_fall.vsin;
	//movementInfo.jump.xyspeed = m_fall.speedxy;
	//movementInfo.jump.zspeed = m_fall.speedz;
	//movementInfo.jump.startClientTime = m_fall.beginT;
	//movementInfo.pos.x = x;
	//movementInfo.pos.y = y;
	//movementInfo.pos.z = z;
	//std::unique_ptr<WorldPacket>send = std::make_unique<WorldPacket>(MSG_MOVE_HEARTBEAT);
	//*send << movementInfo;
	//send->FillPacketTime(movementInfo.ctime);
	//me->GetSession()->HandleMovementOpcodes(*send);
}


void LuaAgent::FallEnd(float x, float y, float z)
{
	if (!IsFalling())
		return;
	MovementInfo movementInfo(me->m_movementInfo);
	movementInfo.moveFlags = 0u;
	movementInfo.ctime = WorldTimer::getMSTime();
	movementInfo.pos.x = x;
	movementInfo.pos.y = y;
	movementInfo.pos.z = z;
	//printf("Fall stop at (%f %f %f)\n", x, y, z);
	{
		std::unique_ptr<WorldPacket>send = std::make_unique<WorldPacket>(MSG_MOVE_STOP);
		*send << movementInfo;
		send->FillPacketTime(movementInfo.ctime);
		me->GetSession()->HandleMovementOpcodes(*send);
	}
	{
		std::unique_ptr<WorldPacket>send = std::make_unique<WorldPacket>(MSG_MOVE_FALL_LAND);
		*send << movementInfo;
		send->FillPacketTime(movementInfo.ctime);
		me->GetSession()->QueuePacket(std::move(send));
	}
	m_fall = FallInfo();
}
