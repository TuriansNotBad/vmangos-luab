#include "Player.h"
#include "Bag.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "PlayerBotMgr.h"
#include "Opcodes.h"
#include "World.h"
#include "WorldPacket.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Chat.h"
#include <random>
#include "LuaUtils.h"
#include "LuaBotAI.h"
#include "LuaLibAI.h"
#include "LuaLibPlayer.h"
#include "LuaBindsBotCommon.h"
#include "CharacterDatabaseCache.h"


#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

const char* LuaBotAI::AI_MTNAME = "Object.AI";

enum PartyBotSpells
{
	PB_SPELL_FOOD = 1131,
	PB_SPELL_DRINK = 1137,
	PB_SPELL_AUTO_SHOT = 75,
	PB_SPELL_BOW_SKILL = 264,
	PB_SPELL_SHOOT_BOW = 2480,
	PB_SPELL_SHOOT_WAND = 5019,
	PB_SPELL_HONORLESS_TARGET = 2479,

	// from combat bot
	SPELL_SUMMON_IMP = 688,
	SPELL_SUMMON_VOIDWALKER = 697,
	SPELL_SUMMON_FELHUNTER = 691,
	SPELL_SUMMON_SUCCUBUS = 712,
	SPELL_TAME_BEAST = 13481,
	SPELL_REVIVE_PET = 982,
	SPELL_CALL_PET = 883,

};

enum PartyBotLuaPets {
	PET_WOLF = 565,
	PET_CAT = 681,
	PET_BEAR = 822,
	PET_CRAB = 831,
	PET_GORILLA = 1108,
	PET_BIRD = 1109,
	PET_BOAR = 1190,
	PET_BAT = 1554,
	PET_CROC = 1693,
	PET_SPIDER = 1781,
	PET_OWL = 1997,
	PET_STRIDER = 2322,
	PET_ZARICOTL = 2931,
	PET_SCORPID = 3127,
	PET_SERPENT = 3247,
	PET_RAPTOR = 3254,
	PET_TURTLE = 3461,
	PET_HYENA = 4127,

};

enum PartyBotLuaRequests
{
	PB_REQUEST_PULL = 0,
	PB_REQUEST_ATTACK = 1
};


#define PB_UPDATE_INTERVAL 100
#define PB_MIN_FOLLOW_DIST 3.0f
#define PB_MAX_FOLLOW_DIST 6.0f
#define PB_MIN_FOLLOW_ANGLE 0.0f
#define PB_MAX_FOLLOW_ANGLE 6.0f


inline bool file_exists(const std::string& name) {
	if (FILE* file = fopen(name.c_str(), "r")) {
		fclose(file);
		return true;
	}
	else {
		return false;
	}
}


std::string classToClassName(uint8 cls) {
	if (cls == CLASS_DRUID)
		return "druid";
	else if (cls == CLASS_HUNTER)
		return "hunter";
	else if (cls == CLASS_MAGE)
		return "mage";
	else if (cls == CLASS_PALADIN)
		return "paladin";
	else if (cls == CLASS_PRIEST)
		return "priest";
	else if (cls == CLASS_ROGUE)
		return "rogue";
	else if (cls == CLASS_SHAMAN)
		return "shaman";
	else if (cls == CLASS_WARLOCK)
		return "warlock";
	else if (cls == CLASS_WARRIOR)
		return "warrior";
	return "";
}


LuaBotAI::LuaBotAI(Player* pLeader, Player* pClone, uint8 race, uint8 class_, uint8 gender, uint8 level, uint32 mapId, uint32 instanceId, float x, float y, float z, float o, int logicId, std::string spec)
	:
	PartyBotAI(pLeader, pClone, ROLE_MELEE_DPS, race, class_, gender, level, mapId, instanceId, x, y, z, o),
	logicId(logicId),
	topGoal(0, 10.0, Goal::NOPARAMS, nullptr, nullptr),
	logicManager(logicId),
	eventRequest(-1),
	userTblRef(LUA_NOREF),
	m_spec(spec),
	m_roleID(LuaBotRoles::LBROLE_INVALID)
{
	topGoal.SetTerminated(true);
	ceaseUpdates = false;
	_queueGoname = false;
	_queueGonameName = "";
	userDataRef = LUA_NOREF;
	userDataRefPlayer = LUA_NOREF;

	// check if script file exists here
	//if (!file_exists(std::string("aiscripts/" + luaScriptName + ".lua"))) {
	//	sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "[LuaBotAI] AI script doesn't exist - %s", std::string("aiscripts/" + luaScriptName + ".lua").c_str());
	//	ceaseUpdates = true;
	//	return;
	//}
	// 
	// load lua if all good

	//LuaBindsAI::BindAll(L);
	//int result = luaL_dofile(L, std::string("aiscripts/" + luaScriptName + ".lua").c_str());
	//if (result != LUA_OK) {
	//	ceaseUpdates = true;
	//	printf("Lua error: %s\n", lua_tostring(L, -1));
	//	lua_pop(L, 1); // pop the error object
	//	return;
	//}
	// probably temporary, logic manager expects lua to be already set up before its created
	// which isn't the case when ai object owns lua thread
	//logicManager = LogicManager(logicId); // recreate manager after lua is set up
	L = sPlayerBotMgr.Lua();
	//CreateUD(L);
}


void LuaBotAI::Initialize() {

	Player* pPlayer = ObjectAccessor::FindPlayer(m_leaderGuid);
	if (pPlayer && pPlayer->GetTeamId() == me->GetTeamId())
		AddToPlayerGroup();

	if (m_race && m_class) // temporary character
	{
		if (m_level && m_level != me->GetLevel())
		{
			me->GiveLevel(m_level);
			me->InitTalentForLevel();
			me->SetUInt32Value(PLAYER_XP, 0);
		}

		if (!m_cloneGuid.IsEmpty())
		{
			CloneFromPlayer(sObjectAccessor.FindPlayer(m_cloneGuid));
			AutoAssignRole();
		}
		else
		{
			LearnPremadeSpecForClass();

			if (m_role == ROLE_INVALID)
				AutoAssignRole();

			AutoEquipGear(sWorld.getConfig(CONFIG_UINT32_PARTY_BOT_AUTO_EQUIP));

			// fix client bug causing some item slots to not be visible
			if (Player* pLeader = GetPartyLeader())
			{
				me->SetVisibility(VISIBILITY_OFF);
				pLeader->UpdateVisibilityOf(pLeader, me);
				me->SetVisibility(VISIBILITY_ON);
			}
		}
		// give bows to warriors
		if (m_class == CLASS_WARRIOR && m_role == ROLE_TANK) {
			if (!me->HasSpell(PB_SPELL_BOW_SKILL))
				me->LearnSpell(PB_SPELL_BOW_SKILL, false);
			if (!me->HasSpell(PB_SPELL_SHOOT_BOW))
				me->LearnSpell(PB_SPELL_SHOOT_BOW, false);
			AddItemToInventory(11304);
			EquipOrUseNewItem();
		}
		me->UpdateSkillsToMaxSkillsForLevel();
	}
	else // loaded from db
	{
		if (m_role == ROLE_INVALID)
			AutoAssignRole();

		if (me->IsGameMaster())
			me->SetGameMaster(false);

		me->TeleportTo(m_mapId, m_x, m_y, m_z, m_o);
	}
	ResetSpellData();
	PopulateSpellData();
	
	me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SPAWNING);
	//SummonPetIfNeeded();
	me->SetHealthPercent(100.0f);
	me->SetPowerPercent(me->GetPowerType(), 100.0f);

	uint32 newzone, newarea;
	me->GetZoneAndAreaId(newzone, newarea);
	me->UpdateZone(newzone, newarea);

	L = sPlayerBotMgr.Lua();
	if (userDataRef == LUA_NOREF)
		CreateUD(L);
	if (userDataRefPlayer == LUA_NOREF)
		CreatePlayerUD(L);
	if (userTblRef == LUA_NOREF)
		CreateUserTbl();
	logicManager.Init(L, this);
	m_initialized = true;
	AddAllSpellReagents();
}


void LuaBotAI::UpdateAI(uint32 const diff) {}


void LuaBotAI::UpdateAILua(uint32 const diff) {

	if (ceaseUpdates) return;

	m_updateTimer.Update(diff);
	if (m_updateTimer.Passed())
		m_updateTimer.Reset(50);
	else
		return;

	// catch up was queued
	if (_queueGoname) {
		_queueGoname = false;
		GonameCommand(_queueGonameName);
		_queueGonameName = "";
		return;
	}
	
	if (!me->IsInWorld() || me->IsBeingTeleported())
		return;

	if (!m_initialized) {
		Initialize();
		return;
	}

	// check uds

	if (userDataRef == userDataRefPlayer || userDataRef == userTblRef || userTblRef == userDataRefPlayer) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAI Core: Lua registry error... [%d, %d, %d]. Ceasing.\n", userDataRef, userDataRefPlayer, userTblRef);
		CeaseUpdates();
		return;
	}

	// don't gain xp
	me->SetUInt32Value(PLAYER_XP, 0);
	Player* pLeader = GetPartyLeader();

	// we leveled up, update.
	if (pLeader) {

		if (me->GetLevel() != pLeader->GetLevel() && !me->IsInCombat() && pLeader->GetLevel() <= 60) {

			m_level = pLeader->GetLevel();

			// destroy all gear so its regenerated
			for (int slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; slot++)
				if (me->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
					me->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);

			Reset(false);
			m_initialized = false;
			SetEventRequest(1000);

			return;

		}
	}

	if (pLeader) {
		if (!pLeader->IsInWorld() || pLeader->IsBeingTeleported())
			return;

		if (pLeader->InBattleGround() &&
			!me->InBattleGround())
		{
			if (m_receivedBgInvite)
			{
				SendBattlefieldPortPacket();
				m_receivedBgInvite = false;
				return;
			}

			// Remain idle until we can join battleground.
			return;
		}

		if (pLeader->IsTaxiFlying())
		{
			if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
			{
				me->GetMotionMaster()->Clear(false, true);
				me->GetMotionMaster()->MoveIdle();
			}
			return;
		}

		// keep pet happy and loyal
		if (Pet* pPet = me->GetPet()) {
			pPet->SetPower(Powers::POWER_HAPPINESS, pPet->GetMaxPower(Powers::POWER_HAPPINESS));
			pPet->SetLoyaltyLevel(LoyaltyLevel::BEST_FRIEND);
		}

	}
	else {

		if (me->GetGroupInvite()) {

			Group* group = me->GetGroupInvite();
			if (group->GetMembersCount() == 0)
				group->AddMember(group->GetLeaderGuid(), group->GetLeaderName());

			group->AddMember(me->GetObjectGuid(), me->GetName());

		}

	}

	//if (me->HasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
	//	return;

	//if (me->GetTargetGuid() == me->GetObjectGuid())
	//	me->ClearTarget();

	logicManager.Execute(L, this);
	if (!topGoal.GetTerminated()) {
		goalManager.Activate(L, this);
		goalManager.Update(L, this);
		goalManager.Terminate(L, this);
	}

	// a manager called error state
	if (ceaseUpdates) {
		goalManager = GoalManager();
		Reset(false);
	}

}


void LuaBotAI::Reset(bool droprefs) {

	// reset goals
	goalManager = GoalManager();
	topGoal = Goal(0, 10.0, Goal::NOPARAMS, &goalManager, nullptr); // delete all goal objects
	topGoal.SetTerminated(true);

	// stop
	if (me->GetMotionMaster()->GetCurrentMovementGeneratorType()) {
		me->GetMotionMaster()->Clear(false, true);
		me->GetMotionMaster()->MoveIdle();
	}

	// unmount
	if (me->IsMounted())
		me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

	// stand up
	if (me->GetStandState() != UnitStandStateType::UNIT_STAND_STATE_STAND)
		me->SetStandState(UnitStandStateType::UNIT_STAND_STATE_STAND);

	// unshapeshift
	if (me->HasAuraType(AuraType::SPELL_AURA_MOD_SHAPESHIFT))
		me->RemoveSpellsCausingAura(AuraType::SPELL_AURA_MOD_SHAPESHIFT);

	// reset speed
	me->SetSpeedRate(UnitMoveType::MOVE_RUN, 1.0f);

	// stop attacking
	me->AttackStop();

	if (droprefs) {
		userTblRef = LUA_NOREF;
		userDataRef = LUA_NOREF;
		userDataRefPlayer = LUA_NOREF;
	}
	else {
		// delete all refs
		Unref(L);
		UnrefPlayerUD(L);
		UnrefUserTbl(L);
	}
	m_initialized = false;

}


bool LuaBotAI::IsReady() {
	return IsInitialized() && me->IsInWorld() && !me->IsBeingTeleported();
}


void LuaBotAI::AttackAutoshot(Unit* pVictim, float chaseDist) {
	me->Attack(pVictim, false);
	if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE
		&& me->GetDistance(pVictim) > chaseDist + 5)
	{
		me->GetMotionMaster()->MoveChase(pVictim, chaseDist);
	}
	
	if (me->HasSpell(PB_SPELL_AUTO_SHOT) &&
		(me->GetCombatDistance(pVictim) > 5.0f) &&
		!me->IsNonMeleeSpellCasted())
	{
		switch (me->CastSpell(pVictim, PB_SPELL_AUTO_SHOT, false))
		{
		case SPELL_FAILED_NEED_AMMO:
		case SPELL_FAILED_NO_AMMO:
		{
			AddHunterAmmo();
			me->CastSpell(pVictim, PB_SPELL_AUTO_SHOT, false);
			break;
		}
		}
	}
}


void LuaBotAI::AttackStopAutoshot() {
	if (me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)) {
		me->InterruptSpell(CURRENT_AUTOREPEAT_SPELL, true);
	}
}


bool LuaBotAI::CanCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry, bool bAura, bool bGCD) const
{
	if (!me->IsSpellReady(pSpellEntry->Id))
		return false;
	
	if (bGCD && me->HasGCD(pSpellEntry))
		return false;

	if (pSpellEntry->TargetAuraState &&
		!pTarget->HasAuraState(AuraState(pSpellEntry->TargetAuraState)))
		return false;

	if (pSpellEntry->CasterAuraState &&
		!me->HasAuraState(AuraState(pSpellEntry->CasterAuraState)))
		return false;

	uint32 const powerCost = Spell::CalculatePowerCost(pSpellEntry, me);
	Powers const powerType = Powers(pSpellEntry->powerType);

	if (powerType == POWER_HEALTH)
	{
		if (me->GetHealth() <= powerCost)
			return false;
		return true;
	}

	if (me->GetPower(powerType) < powerCost)
		return false;

	if (pTarget->IsImmuneToSpell(pSpellEntry, false))
		return false;

	if (pSpellEntry->GetErrorAtShapeshiftedCast(me->GetShapeshiftForm()) != SPELL_CAST_OK)
		return false;

	if (bAura && pSpellEntry->IsSpellAppliesAura() && pTarget->HasAura(pSpellEntry->Id))
		return false;

	SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(pSpellEntry->rangeIndex);
	if (me != pTarget && pSpellEntry->EffectImplicitTargetA[0] != TARGET_UNIT_CASTER)
	{
		float const dist = me->GetCombatDistance(pTarget);

		if (dist > srange->maxRange)
			return false;
		if (srange->minRange && dist < srange->minRange)
			return false;
	}

	return true;
}


bool LuaBotAI::DrinkAndEat(float healthPer, float manaPer)
{
	if (me->GetVictim())
		return false;

	bool const needToEat = me->GetHealthPercent() < healthPer;
	bool const needToDrink = (me->GetPowerType() == POWER_MANA || me->GetClass() == CLASS_DRUID) && (me->GetPowerPercent(POWER_MANA) < manaPer);

	bool const isEating = me->HasAura(PB_SPELL_FOOD);
	bool const isDrinking = me->HasAura(PB_SPELL_DRINK);

	if (!isEating && needToEat)
	{
		if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
		{
			me->StopMoving();
			me->GetMotionMaster()->Clear(false, true);
			me->GetMotionMaster()->MoveIdle();
		}
		if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(PB_SPELL_FOOD))
		{
			me->CastSpell(me, pSpellEntry, true);
			me->RemoveSpellCooldown(*pSpellEntry);
		}
		//return true;
	}

	if (!isDrinking && needToDrink)
	{
		if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
		{
			me->StopMoving();
			me->GetMotionMaster()->Clear(false, true);
			me->GetMotionMaster()->MoveIdle();
		}
		if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(PB_SPELL_DRINK))
		{
			me->CastSpell(me, pSpellEntry, true);
			me->RemoveSpellCooldown(*pSpellEntry);
		}
		//return true;
	}

	bool _interruptEat = !isEating || (isEating && me->GetHealthPercent() > 99.0f);
	bool _interruptDrink = !isDrinking || (isDrinking && me->GetPowerPercent(POWER_MANA) > 99.0f) || me->GetPowerType() != POWER_MANA;
	bool _interruptOkay = _interruptEat && _interruptDrink;

	if ((isEating || isDrinking) && _interruptOkay)
		me->SetStandState(UNIT_STAND_STATE_STAND);

	return needToEat || needToDrink;
}


void LuaBotAI::GonameCommandQueue(std::string name) {
	_queueGoname = true;
	_queueGonameName = name;
}


void LuaBotAI::GonameCommand(std::string name) {
	// will crash if moving
	if (!me->IsStopped())
		me->StopMoving();
	me->GetMotionMaster()->Clear(false, true);
	me->GetMotionMaster()->MoveIdle();
	me->ClearTarget();
	topGoal = Goal(0, 0, Goal::NOPARAMS, nullptr, nullptr);
	topGoal.SetTerminated(true);
	char namecopy[128] = {};
	strcpy(namecopy, name.c_str());
	ChatHandler(me).HandleGonameCommand(namecopy);
}


void LuaBotAI::Mount(bool toMount, uint32 mountSpell) {
	//Player* pLeader = GetPartyLeader();
	if (toMount && false == me->IsMounted())
	{
		// Leave shapeshift before mounting.
		if (me->IsInDisallowedMountForm() &&
			me->GetDisplayId() != me->GetNativeDisplayId() &&
			me->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
			me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

		bool oldState = me->HasCheatOption(PLAYER_CHEAT_NO_CAST_TIME);
		me->SetCheatOption(PLAYER_CHEAT_NO_CAST_TIME, true);
		me->CastSpell(me, mountSpell, true);
		me->SetCheatOption(PLAYER_CHEAT_NO_CAST_TIME, oldState);
	}
	else if (true == me->IsMounted()) {
		me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
	}
}


Player* LuaBotAI::GetPartyLeader() const {
	Group* pGroup = me->GetGroup();
	if (!pGroup) return nullptr;
	ObjectGuid leaderguid = pGroup->GetLeaderGuid();
	for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
		if (Player* pMember = itr->getSource())
			if (pMember->GetGUID() == leaderguid)
				return pMember;
	return nullptr;
}


Goal* LuaBotAI::AddTopGoal(int goalId, double life, std::vector<GoalParamP>& goalParams, lua_State* L) {
	//topGoal.Unref(L);
	//topGoal.~Goal();
	topGoal = Goal(goalId, life, goalParams, &goalManager, L);
	goalManager.ClearActivationStack(); // top goal owns all of the goals, we could just nuke the entire manager
	goalManager.PushGoalOnActivationStack(&topGoal);
	return &topGoal;
}


// Lua bits
void LuaBotAI::CreateUD(lua_State* L) {
	// create userdata on top of the stack pointing to a pointer of an AI object
	LuaBotAI** aiud = static_cast<LuaBotAI**>(lua_newuserdatauv(L, sizeof(LuaBotAI*), 0));
	*aiud = this; // swap the AI object being pointed to to the current instance
	luaL_setmetatable(L, AI_MTNAME);
	// save this userdata in the registry table.
	userDataRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops
}


void LuaBotAI::PushUD(lua_State* L) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, userDataRef);
}


void LuaBotAI::SummonPetIfNeeded(uint32 petId)
{
	if (me->GetClass() == CLASS_HUNTER)
	{
		if (me->GetCharmGuid())
			return;

		if (me->GetLevel() < 10)
			return;

		if (me->GetPetGuid() || sCharacterDatabaseCache.GetCharacterPetByOwner(me->GetGUIDLow()))
		{
			if (Pet* pPet = me->GetPet())
			{
				if (!pPet->IsAlive()) {
					uint32 mana = me->GetPower(POWER_MANA);
					me->CastSpell(pPet, SPELL_REVIVE_PET, true);
					me->SetPower(POWER_MANA, mana);
				}
			}
			else
				me->CastSpell(me, SPELL_CALL_PET, true);

			return;
		}

		if (Creature* pCreature = me->SummonCreature(petId,
			me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f,
			TEMPSUMMON_TIMED_COMBAT_OR_DEAD_DESPAWN, 3000, false, 3000))
		{
			pCreature->SetLevel(me->GetLevel());
			me->CastSpell(pCreature, SPELL_TAME_BEAST, true);
		}
	}
	else if (me->GetClass() == CLASS_WARLOCK)
	{
		if (me->GetPetGuid() || me->GetCharmGuid())
			if (Pet* pPet = me->GetPet()) {
				if (pPet->IsAlive())
					return;
			}
			else
				return;

		if (petId != SPELL_SUMMON_IMP 
			&& petId != SPELL_SUMMON_VOIDWALKER 
			&& petId != SPELL_SUMMON_FELHUNTER
			&& petId != SPELL_SUMMON_SUCCUBUS)
		{
			sLog.Out(LogType::LOG_BASIC, LogLevel::LOG_LVL_ERROR, "SummonPetIfNeeded invalid warlock spell ID %d", petId);
			return;
		}

		me->CastSpell(me, petId, true);
	}

}


void LuaBotAI::Unref(lua_State* L) {
	if (userDataRef != LUA_NOREF && userDataRef != LUA_REFNIL) {
		luaL_unref(L, LUA_REGISTRYINDEX, userDataRef);
		userDataRef = LUA_NOREF; // old ref no longer valid
	}
}


void LuaBotAI::CreatePlayerUD(lua_State* L) {
	LuaBindsAI::Player_CreateUD(me, L);
	// save this userdata in the registry table.
	userDataRefPlayer = luaL_ref(L, LUA_REGISTRYINDEX); // pops
}


void LuaBotAI::PushPlayerUD(lua_State* L) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, userDataRefPlayer);
}


void LuaBotAI::UnrefPlayerUD(lua_State* L) {
	if (userDataRefPlayer != LUA_NOREF && userDataRefPlayer != LUA_REFNIL) {
		luaL_unref(L, LUA_REGISTRYINDEX, userDataRefPlayer);
		userDataRefPlayer = LUA_NOREF; // old ref no longer valid
	}
}

void LuaBotAI::UnrefUserTbl(lua_State* L) {
	if (userTblRef != LUA_NOREF && userTblRef != LUA_REFNIL) {
		luaL_unref(L, LUA_REGISTRYINDEX, userTblRef);
		userTblRef = LUA_NOREF; // old ref no longer valid
	}
}

// other

void LuaBotAI::Print() {
	printf("LuaBotAI object. Class = %s, userDataRef = %d\n", classToClassName(m_class).c_str(), userDataRef);
}


// ========================================================================
// Bot meddling. Equip. Spells.
// ========================================================================


uint32 LuaBotAI::GetSpellChainFirst(uint32 spellID) {

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellChainFirst: spell %d not found.", spellID);
		return 0;
	}

	auto info_first = sSpellMgr.GetFirstSpellInChain(spellID);
	if (!info_first)
		return spellID;

	return info_first;

}


uint32 LuaBotAI::GetSpellChainPrev(uint32 spellID) {

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellChainPrev: spell %d not found.", spellID);
		return 0;
	}

	auto info_prev = sSpellMgr.GetPrevSpellInChain(spellID);
	if (!info_prev)
		return spellID;

	return info_prev;

}


std::string LuaBotAI::GetSpellName(uint32 spellID) {

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellName: spell %d not found.", spellID);
		return "";
	}

	return info_orig->SpellName[0];

}


uint32 LuaBotAI::GetSpellRank(uint32 spellID) {
	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellRank: spell %d not found.", spellID);
		return 0;
	}

	auto chain = sSpellMgr.GetSpellChainNode(spellID);
	if (!chain)
		return 1;

	return chain->rank;
}


uint32 LuaBotAI::GetSpellMaxRankForLevel(uint32 spellID, uint32 level) {

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig) {
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
	while (level < info_next->spellLevel) {

		// we've ran out of ranks
		if (info_next->Id == info_last)
			return info_last;

		// update result
		info_final = info_next;

		info_next = sSpellMgr.GetSpellEntry(sSpellMgr.GetPrevSpellInChain(info_next->Id));
		// weird error?
		if (!info_next) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellMaxRankForLevel: spell %d failed to find spell of next rank.", spellID);
			return 0;
		}

		if (info_next->Id == info_final->Id)
			return info_next->Id;

	}

	return info_next->Id;

}


uint32 LuaBotAI::GetSpellMaxRankForMe(uint32 spellID) {
	return GetSpellMaxRankForLevel(spellID, me->GetLevel());
}


uint32 LuaBotAI::GetSpellOfRank(uint32 spellID, uint32 rank) {

	auto info_orig = sSpellMgr.GetSpellEntry(spellID);
	// spell not found
	if (!info_orig) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d not found.", spellID);
		return 0;
	}

	auto chain = sSpellMgr.GetSpellChainNode(spellID);
	// looks like there is only one rank
	if (!chain) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d has no spell chain.", spellID);
		return 0;
	}

	auto info_last = chain->first;
	auto info_final = info_orig;
	auto info_next = info_orig;
	while (true) {


		// update result
		info_final = info_next;

		auto chain = sSpellMgr.GetSpellChainNode(info_final->Id);
		if (!chain) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d has no spell chain.", spellID);
			return 0;
		}

		// found
		if (chain->rank == rank)
			return info_final->Id;

		// we've ran out of ranks
		if (info_next->Id == info_last) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d ran out of ranks. Requested rank %d", spellID, rank);
			return 0;
		}

		info_next = sSpellMgr.GetSpellEntry(chain->prev);
		// weird error?
		if (!info_next) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetSpellOfRank: spell %d failed to find spell of next rank.", spellID);
			return 0;
		}

		if (info_next->Id == info_final->Id)
			return info_next->Id;

	}

	return info_next->Id;


}


void LuaBotAI::AddItemToInventory(uint32 itemId, uint32 count, int32 randomPropertyId)
{
	ItemPosCountVec dest;
	uint8 msg = me->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count);
	if (msg == EQUIP_ERR_OK)
	{
		if (randomPropertyId < 1)
			randomPropertyId = Item::GenerateItemRandomPropertyId(itemId);
		if (Item* pItem = me->StoreNewItem(dest, itemId, true, randomPropertyId))
			pItem->SetCount(count);
	}
}


bool LuaBotAI::EquipCopyFromMaster() {

	Player* owner = ObjectAccessor::FindPlayer(m_leaderGuid);
	if (!owner) return false;

	if (owner->GetClass() != me->GetClass()) return false;

	for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i) {

		me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);

		Item* item = owner->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
		if (!item || !item->GetProto()) continue;

		EquipItem(item->GetEntry());

	}

	// fix client bug causing some item slots to not be visible
	if (Player* pLeader = GetPartyLeader())
	{
		me->SetVisibility(VISIBILITY_OFF);
		pLeader->UpdateVisibilityOf(pLeader, me);
		me->SetVisibility(VISIBILITY_ON);
	}

	return true;

}


void LuaBotAI::EquipDestroyAll() {

	for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
		me->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);

	for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; i++)
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

	// fix client bug causing some item slots to not be visible
	if (Player* pLeader = GetPartyLeader())
	{
		me->SetVisibility(VISIBILITY_OFF);
		pLeader->UpdateVisibilityOf(pLeader, me);
		me->SetVisibility(VISIBILITY_ON);
	}

}


uint32 LuaBotAI::EquipFindItemByName(const std::string& name) {
	
	for (auto const& itr : sObjectMgr.GetItemPrototypeMap()) {

		ItemPrototype item = itr.second;
		if (std::string(item.Name1).find(name) != std::string::npos)
			return item.ItemId;

	}


	return 0;

}


void LuaBotAI::EquipItem(uint32 itemID, int32 randomPropertyId) {
	AddItemToInventory(itemID, 1, randomPropertyId);
	EquipOrUseNewItem();
	// fix client bug causing some item slots to not be visible
	if (Player* pLeader = GetPartyLeader())
	{
		me->SetVisibility(VISIBILITY_OFF);
		pLeader->UpdateVisibilityOf(pLeader, me);
		me->SetVisibility(VISIBILITY_ON);
	}
}


void LuaBotAI::EquipEnchant(uint32 enchantID, EnchantmentSlot slot, EquipmentSlots itemSlot, int duration, int charges) {

	Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);
	if (!item) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "EquipEnchant: Attempt to enchant an empty slot {}", itemSlot);
		return;
	}

	item->ClearEnchantment(slot);
	item->SetEnchantment(slot, enchantID, duration, charges);
	me->_ApplyItemMods(item, slot & 255, false);
	me->_ApplyItemMods(item, slot & 255, true);


}


uint32 LuaBotAI::EquipGetEnchantId(EnchantmentSlot slot, EquipmentSlots itemSlot) {

	Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);
	if (!item) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "EquipGetEnchantId: Attempt to get enchant id of an empty slot {}", itemSlot);
		return 0;
	}

	return item->GetEnchantmentId(slot);

}


int32 LuaBotAI::EquipGetRandomProp(EquipmentSlots itemSlot) {

	Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);
	if (!item) {
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "EquipGetRandomProp: Attempt to get random property id of an empty slot {}", itemSlot);
		return 0;
	}

	return item->GetItemRandomPropertyId();

}



