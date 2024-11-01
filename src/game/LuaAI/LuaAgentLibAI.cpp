
#include "LuaAgentLibPlayer.h"
#include "LuaAgentLibUnit.h"
#include "LuaAgentLibAI.h"
#include "LuaAgentUtils.h"
#include "LuaAgent.h"
#include "LuaAgentLibWorldObj.h"
#include "LuaAgentMovementGenerator.h"
#include "PointMovementGenerator.h"
#include "Hierarchy/LuaAgentPartyInt.h"
#include "Libs/CLine.h"


namespace
{
	inline void AI_CmdSetStateHelper(lua_State* L, LuaAgent* ai, AgentCmd::State state)
	{
		AgentCmd* cmd = ai->CommandsGetFirst();
		if (!cmd)
			luaL_error(L, "AI_CmdSetState: no commands in queue");
		cmd->SetState(AgentCmd::State(state));
	}
	inline LuaAIChaseMovementGenerator<Player>* AI_GetChaseGen(Player* agent)
	{
		if (agent->GetMotionMaster()->GetCurrentMovementGeneratorType() == MovementGeneratorType::CHASE_MOTION_TYPE)
			if (auto constGen = dynamic_cast<const LuaAIChaseMovementGenerator<Player>*>(agent->GetMotionMaster()->GetCurrent()))
				return const_cast<LuaAIChaseMovementGenerator<Player>*>(constGen);
		return nullptr;
	}
}


void LuaBindsAI::BindAI(lua_State* L)
{
	AI_CreateMetatable( L );
}


LuaAgent* LuaBindsAI::AI_GetAIObject(lua_State* L, int idx)
{
	return *((LuaAgent**) luaL_checkudata(L, idx, LuaAgent::AI_MTNAME));
}


void LuaBindsAI::AI_CreateMetatable(lua_State* L)
{
	luaL_newmetatable(L, LuaAgent::AI_MTNAME);
	lua_pushvalue(L, -1); // copy mt cos setfield pops
	lua_setfield(L, -1, "__index"); // mt.__index = mt
	luaL_setfuncs(L, AI_BindLib, 0); // copy funcs
	lua_pop(L, 1); // pop mt
}


int LuaBindsAI::AI_HasReset(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushboolean(L, ai->HasReset());
	return 1;
}


int LuaBindsAI::AI_AddTopGoal(lua_State* L)
{
	int nArgs = lua_gettop(L);

	if (nArgs < 3)
		luaL_error(L, "AI.AddTopGoal - invalid number of arguments. 3 min, %d given", nArgs);

	LuaAgent* ai = AI_GetAIObject(L);
	int goalId = luaL_checknumber(L, 2);

	Goal* topGoal = ai->GetTopGoal();

	if (goalId != topGoal->GetGoalId() || topGoal->GetTerminated())
	{
		double life = luaL_checknumber(L, 3);

		std::vector<GoalParamP> params;
		Goal_GrabParams(L, nArgs, params);

		Goal* goalUserdata = Goal_CreateGoalUD(L, ai->AddTopGoal(goalId, life, params, L)); // ud on top of the stack
		// duplicate userdata for return result
		lua_pushvalue(L, -1);
		// save userdata
		goalUserdata->SetRef(luaL_ref(L, LUA_REGISTRYINDEX)); // pops the object as well
		goalUserdata->CreateUsertable();
	}
	else
		// leave topgoal's userdata as return result for lua
		lua_rawgeti(L, LUA_REGISTRYINDEX, topGoal->GetRef());

	return 1;
}


int LuaBindsAI::AI_GetTopGoal(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (Goal* goal = ai->GetTopGoal())
		if (!goal->GetTerminated())
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, goal->GetRef());
			return 1;
		}
	return 0;
}


int LuaBindsAI::AI_HasTopGoal(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer id = luaL_checkinteger(L, 2);
	bool result = false;
	if (Goal* goal = ai->GetTopGoal())
		if (!goal->GetTerminated() && goal->GetGoalId() == id)
			result = true;
	lua_pushboolean(L, result);
	return 1;
}


int LuaBindsAI::AI_GetUserTbl(lua_State* L) {
	LuaAgent* ai = AI_GetAIObject(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ai->GetUserTblRef());
	return 1;
}


int LuaBindsAI::AI_GetDesiredLevel(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushinteger(L, ai->GetDesiredLevel());
	return 1;
}


int LuaBindsAI::AI_SetDesiredLevel(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer value = luaL_checkinteger(L, 2);
	if (value < 1)  value = 1;
	if (value > 60) value = 60;
	ai->SetDesiredLevel(value);
	return 0;
}


int LuaBindsAI::AI_GetForm(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushinteger(L, ai->GetForm());
	return 1;
}


int LuaBindsAI::AI_SetForm(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer value = luaL_checkinteger(L, 2);
	if (value < 0 || value > ShapeshiftForm::FORM_SPIRITOFREDEMPTION)
		luaL_error(L, "AI_SetForm: value must be in range [0, %d], got %I", ShapeshiftForm::FORM_SPIRITOFREDEMPTION, value);
	ai->SetForm(ShapeshiftForm(value));
	return 0;
}


int LuaBindsAI::AI_GetStdThreat(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushnumber(L, ai->GetStdThreat());
	return 1;
}


int LuaBindsAI::AI_SetStdThreat(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number value = luaL_checknumber(L, 2);
	if (value < 0)
		luaL_error(L, "AI_SetStdThreat: value must be > 0, got %f", value);
	ai->SetStdThreat(value);
	return 0;
}


int LuaBindsAI::AI_GetHealTarget(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushunitornil(L, ai->GetPlayer()->GetMap()->GetUnit(ai->GetHealTarget()));
	return 1;
}


int LuaBindsAI::AI_SetHealTarget(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (lua_isnil(L, 2))
		ai->SetHealTarget(ObjectGuid());
	else
	{
		LuaObjectGuid* guid = Guid_GetGuidObject(L, 2);
		ai->SetHealTarget(guid->guid);
	}
	return 0;
}


int LuaBindsAI::AI_GetCCTarget(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushunitornil(L, ai->GetPlayer()->GetMap()->GetUnit(ai->GetCCTarget()));
	return 1;
}


int LuaBindsAI::AI_SetCCTarget(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (lua_isnil(L, 2))
		ai->SetCCTarget(ObjectGuid());
	else
	{
		LuaObjectGuid* guid = Guid_GetGuidObject(L, 2);
		ai->SetCCTarget(guid->guid);
	}
	return 0;
}


// -----------------------------------------------------------
//                      Chase Control
// -----------------------------------------------------------


int LuaBindsAI::AI_GetChaseTarget(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushunitornil(L, gen->GetTarget());
	else
		lua_pushnil(L);
	return 1;
}


int LuaBindsAI::AI_GetChaseAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushnumber(L, gen->GetAngle());
	else
		lua_pushnumber(L, 0.f);
	return 1;
}


int LuaBindsAI::AI_GetChaseAngleT(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushnumber(L, gen->GetAngleT());
	else
		lua_pushnumber(L, 0.f);
	return 1;
}


int LuaBindsAI::AI_GetChaseUseAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushboolean(L, gen->IsUsingAngle());
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::AI_GetChaseDist(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushnumber(L, gen->GetOffset());
	else
		lua_pushnumber(L, 0.f);
	return 1;
}


int LuaBindsAI::AI_GetChaseMinT(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushnumber(L, gen->GetOffsetMin());
	else
		lua_pushnumber(L, 0.f);
	return 1;
}


int LuaBindsAI::AI_GetChaseMaxT(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		lua_pushnumber(L, gen->GetOffsetMax());
	else
		lua_pushnumber(L, 0.f);
	return 1;
}


int LuaBindsAI::AI_GetChaseSimple(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushboolean(L, ai->IsSimpleChaseMode());
	return 1;
}


int LuaBindsAI::AI_SetChaseAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number v = luaL_checknumber(L, 2);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		gen->SetAngle(v);
	return 0;
}


int LuaBindsAI::AI_SetChaseAngleT(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number v = luaL_checknumber(L, 2);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		gen->SetAngleT(v);
	return 0;
}


int LuaBindsAI::AI_SetChaseUseAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	bool v = luaL_checkboolean(L, 2);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		gen->SetUseAngle(v);
	return 0;
}


int LuaBindsAI::AI_SetChaseAngleValues(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	auto gen = AI_GetChaseGen(ai->GetPlayer());
	if (!gen) return 0;
	if (lua_type(L, 2) == LUA_TNUMBER)
		gen->SetAngle(lua_tonumber(L, 2));
	if (lua_type(L, 3) == LUA_TNUMBER)
		gen->SetAngleT(lua_tonumber(L, 3));
	if (lua_type(L, 4) == LUA_TBOOLEAN)
		gen->SetUseAngle(lua_toboolean(L, 4));
	return 0;
}


int LuaBindsAI::AI_SetChaseDist(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number v = luaL_checknumber(L, 2);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		gen->SetOffset(v);
	return 0;
}


int LuaBindsAI::AI_SetChaseMinT(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number v = luaL_checknumber(L, 2);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		gen->SetOffsetMin(v);
	return 0;
}


int LuaBindsAI::AI_SetChaseMaxT(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number v = luaL_checknumber(L, 2);
	if (auto gen = AI_GetChaseGen(ai->GetPlayer()))
		gen->SetOffsetMax(v);
	return 0;
}


int LuaBindsAI::AI_SetChaseValues(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	auto gen = AI_GetChaseGen(ai->GetPlayer());
	if (!gen) return 0;
	if (lua_type(L, 2) == LUA_TNUMBER)
		gen->SetOffset(lua_tonumber(L, 2));
	if (lua_type(L, 3) == LUA_TNUMBER)
		gen->SetOffsetMin(lua_tonumber(L, 3));
	if (lua_type(L, 4) == LUA_TNUMBER)
		gen->SetOffsetMax(lua_tonumber(L, 4));
	return 0;
}


int LuaBindsAI::AI_SetChaseSimple(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	bool v = luaL_checkboolean(L, 2);
	ai->SetSimpleChaseMode(v);
	return 0;
}


// -----------------------------------------------------------
//                        Motion
// -----------------------------------------------------------


int LuaBindsAI::AI_GetPosForTanking(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	if (PartyIntelligence* pi = ai->GetPartyIntelligence())
	{
		if (!pi->HasCLineFor(ai->GetPlayer()))
			return 0;
		DungeonData::EncounterData* encounter = nullptr;
		if (DungeonData* data = pi->GetDungeonData())
			encounter = data->GetEncounter(target->GetName());
		if (!encounter || !encounter->forceTankPos)
			return 0;
		lua_pushnumber(L, encounter->tankPos.x);
		lua_pushnumber(L, encounter->tankPos.y);
		lua_pushnumber(L, encounter->tankPos.z);
		return 3;
	}
	return 0;
}


int LuaBindsAI::AI_GetAngleForTanking(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	bool allowFlip = luaL_checkboolean(L, 3);
	bool forceFlip = luaL_checkboolean(L, 4);
	if (PartyIntelligence* pi = ai->GetPartyIntelligence())
	{
		bool flipped;
		lua_pushnumber(L, pi->GetAngleForTank(ai, target, flipped, allowFlip, forceFlip));
		lua_pushboolean(L, flipped);
	}
	else
	{
		lua_pushnumber(L, ai->GetPlayer()->GetOrientation());
		lua_pushboolean(L, false);
	}
	return 2;
}


int LuaBindsAI::AI_IsCLineAvailable(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (PartyIntelligence* pi = ai->GetPartyIntelligence())
		lua_pushboolean(L, pi->HasCLineFor(ai->GetPlayer()));
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::AI_IsFalling(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushboolean(L, ai->IsFalling());
	return 1;
}


int LuaBindsAI::AI_IsFollowing(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	Player* agent = ai->GetPlayer();

	if (agent->GetMotionMaster()->GetCurrentMovementGeneratorType() != MovementGeneratorType::FOLLOW_MOTION_TYPE)
		lua_pushboolean(L, false);
	else
		if (auto constFollowGen = dynamic_cast<const LuaAIFollowMovementGenerator<Player>*>(agent->GetMotionMaster()->GetCurrent()))
			lua_pushboolean(L, constFollowGen->GetTarget() && constFollowGen->GetTarget()->GetObjectGuid() == target->GetObjectGuid());
		else
			lua_pushboolean(L, false);

	return 1;
}


int LuaBindsAI::AI_IsMovingTo(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number x = luaL_checknumber(L, 2);
	lua_Number y = luaL_checknumber(L, 3);
	lua_Number z = luaL_checknumber(L, 4);
	Player* agent = ai->GetPlayer();

	if (agent->GetMotionMaster()->GetCurrentMovementGeneratorType() != MovementGeneratorType::POINT_MOTION_TYPE)
		lua_pushboolean(L, false);
	else
		if (auto constFollowGen = dynamic_cast<const PointMovementGenerator<Player>*>(agent->GetMotionMaster()->GetCurrent()))
		{
			float dx, dy, dz;
			constFollowGen->GetDestination(dx, dy, dz);
			float d2 = (dx - x) * (dx - x) + (dy - y) * (dy - y) + (dz - z) * (dz - z);
			lua_pushboolean(L, d2 < 1);
		}
		else
			lua_pushboolean(L, false);

	return 1;
}


int LuaBindsAI::AI_IsUsingAbsAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	Player* agent = ai->GetPlayer();

	if (agent->GetMotionMaster()->GetCurrentMovementGeneratorType() == MovementGeneratorType::CHASE_MOTION_TYPE)
		if (auto constGen = dynamic_cast<const LuaAIChaseMovementGenerator<Player>*>(agent->GetMotionMaster()->GetCurrent()))
		{
			auto gen = const_cast<LuaAIChaseMovementGenerator<Player>*>(constGen);
			lua_pushboolean(L, gen->IsUsingAbsAngle());
			return 1;
		}

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::AI_Jump(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number fwdSpeed = luaL_checknumber(L, 2);
	lua_Number upSpeed = luaL_checknumber(L, 3);
	Unit* me = (Unit*) ai->GetPlayer();
	me->KnockBackFrom(me, -fwdSpeed, upSpeed);
	return 0;
}


int LuaBindsAI::AI_SetAbsAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Number A = luaL_checknumber(L, 2);
	Player* agent = ai->GetPlayer();

	if (agent->GetMotionMaster()->GetCurrentMovementGeneratorType() != MovementGeneratorType::CHASE_MOTION_TYPE)
		return 0;
	else
		if (auto constGen = dynamic_cast<const LuaAIChaseMovementGenerator<Player>*>(agent->GetMotionMaster()->GetCurrent()))
		{
			auto gen = const_cast<LuaAIChaseMovementGenerator<Player>*>(constGen);
			gen->UseAbsAngle(A);
		}


	return 0;
}


int LuaBindsAI::AI_UnsetAbsAngle(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	Player* agent = ai->GetPlayer();

	if (agent->GetMotionMaster()->GetCurrentMovementGeneratorType() != MovementGeneratorType::CHASE_MOTION_TYPE)
		return 0;
	else
		if (auto constGen = dynamic_cast<const LuaAIChaseMovementGenerator<Player>*>(agent->GetMotionMaster()->GetCurrent()))
		{
			auto gen = const_cast<LuaAIChaseMovementGenerator<Player>*>(constGen);
			gen->RemoveAbsAngle();
		}

	return 0;
}


int LuaBindsAI::AI_GoName(lua_State* L) {
	LuaAgent* ai = AI_GetAIObject(L);
	char name[128] = {};
	strcpy(name, luaL_checkstring(L, 2));
	ai->GonameCommandQueue(name);
	return 0;
}


// -----------------------------------------------------------
//                      Equipment
// -----------------------------------------------------------


int LuaBindsAI::AI_EquipCopyFromMaster(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushboolean(L, ai->EquipCopyFromMaster());
	return 1;
}


int LuaBindsAI::AI_EquipItem(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int itemID = luaL_checkinteger(L, 2);
	int enchantId = 0;
	int randomPropertyId = 0;
	if (lua_gettop(L) > 2)
	{
		enchantId = luaL_checkinteger(L, 3);
		if (lua_gettop(L) > 3)
			randomPropertyId = luaL_checkinteger(L, 4);
	}
	lua_pushboolean(L, ai->EquipItem(itemID, enchantId, randomPropertyId));
	return 1;
}


int LuaBindsAI::AI_EquipDestroyAll(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	ai->EquipDestroyAll();
	return 0;
}


int LuaBindsAI::AI_EquipGetEnchantId(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int islot = luaL_checkinteger(L, 2);
	int iitemSlot = luaL_checkinteger(L, 3);
	lua_pushinteger(L, ai->EquipGetEnchantId(EnchantmentSlot(islot), EquipmentSlots(iitemSlot)));
	return 1;
}


int LuaBindsAI::AI_EquipGetRandomProp(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int iitemSlot = luaL_checkinteger(L, 2);
	lua_pushinteger(L, ai->EquipGetRandomProp(EquipmentSlots(iitemSlot)));
	return 1;
}


int LuaBindsAI::AI_EquipHasItemInSlot(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer bag = luaL_checkinteger(L, 2);
	lua_Integer slot = luaL_checkinteger(L, 3);
	bool tradeCheck = luaL_checkboolean(L, 4);
	if (bag < 0 || slot < 0)
		luaL_error(L, "AI_EquipHasItemInSlot: bag and slot must be positive %lld %lld", bag, slot);
	lua_pushboolean(L, ai->EquipHasItemInSlot(bag, slot, tradeCheck));
	return 1;
}


int LuaBindsAI::AI_EquipPrint(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	ai->EquipPrint();
	return 0;
}


int LuaBindsAI::AI_EquipSlotEmpty(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushboolean(L, ai->GetPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, luaL_checkinteger(L, 2)) == nullptr);
	return 1;
}


int LuaBindsAI::AI_EquipSlotItemId(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (Item* item = ai->GetPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, luaL_checkinteger(L, 2)))
		lua_pushinteger(L, item->GetEntry());
	else
		lua_pushinteger(L, 0);
	return 1;
}


int LuaBindsAI::AI_UpdateVisibilityForMaster(lua_State* L)
{
	AI_GetAIObject(L)->UpdateVisibilityForMaster();
	return 0;
}


int LuaBindsAI::AI_GetAmmo(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushinteger(L, ai->GetAmmo());
	return 1;
}


int LuaBindsAI::AI_SetAmmo(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer ammo = luaL_checkinteger(L, 2);
	if (!sObjectMgr.GetItemPrototype(ammo))
		luaL_error(L, "AI_SetAmmo: item %d doesn't exist", ammo);
	ai->SetAmmo(ammo);
	return 0;
}


// -----------------------------------------------------------
//                      Commands
// -----------------------------------------------------------


int LuaBindsAI::AI_CmdAddProgress(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	if (!cmd)
		luaL_error(L, "AI_CmdAddProgress: no commands in queue");
	return cmd->AddProgress(L, 2);
}


int LuaBindsAI::AI_CmdGetProgress(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	if (!cmd)
		luaL_error(L, "AI_CmdGetProgress: no commands in queue");
	return cmd->GetProgress(L);
}


int LuaBindsAI::AI_CmdSetProgress(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	if (!cmd)
		luaL_error(L, "AI_CmdSetProgress: no commands in queue");
	cmd->MinReqProgress(L, 2);
	return 0;
}


int LuaBindsAI::AI_CmdGetArgs(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	if (!cmd)
		luaL_error(L, "AI_CmdGetArgs: no commands in queue");
	return cmd->Push(L);
}


int LuaBindsAI::AI_CmdGetState(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	lua_pushinteger(L, cmd ? lua_Integer(cmd->GetState()) : -1);
	return 1;
}


int LuaBindsAI::AI_CmdGetType(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	lua_pushinteger(L, lua_Integer(cmd ? cmd->GetType() : AgentCmdType::None));
	return 1;
}


int LuaBindsAI::AI_CmdGetQMode(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushinteger(L, ai->CommandsGetQMode());
	return 1;
}


int LuaBindsAI::AI_CmdComplete(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (!ai->CommandsGetFirst())
		luaL_error(L, "AI_CmdComplete: no commands in queue");
	ai->CommandsPopFront();
	return 0;
}


int LuaBindsAI::AI_CmdFail(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (!ai->CommandsGetFirst())
		luaL_error(L, "AI_CmdFail: no commands in queue");
	ai->CommandsClear();
	return 0;
}


int LuaBindsAI::AI_CmdIsRequirementMet(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AgentCmd* cmd = ai->CommandsGetFirst();
	lua_pushboolean(L, cmd ? cmd->MinReqMet() : true);
	return 1;
}


int LuaBindsAI::AI_CmdSetInProgress(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	AI_CmdSetStateHelper(L, ai, AgentCmd::State::InProgress);
	return 0;
}


int LuaBindsAI::AI_CmdPrintAll(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	ai->CommandsPrint();
	return 0;
}


// -----------------------------------------------------------
//                      Spells Chains
// -----------------------------------------------------------


int LuaBindsAI::AI_GetSpellChainFirst(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellChainFirst(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellChainFirst: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellChainPrev(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellChainPrev(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellChainPrev: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellMaxRankForLevel(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	int level = luaL_checkinteger(L, 3);
	uint32 result = ai->GetSpellMaxRankForLevel(spellID, level);
	// this could be an error
	if (result == 0 && !sSpellMgr.GetSpellEntry(spellID))
		luaL_error(L, "AI.GetSpellMaxRankForLevel: spell doesn't exist. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellMaxRankForMe(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellMaxRankForMe(spellID);
	// this could be an error
	if (result == 0 && !sSpellMgr.GetSpellEntry(spellID))
		luaL_error(L, "AI.GetSpellMaxRankForMe: spell doesn't exist. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellLevel(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellLevel(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellLevel: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellOfRank(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	int rank = luaL_checkinteger(L, 3);
	uint32 result = ai->GetSpellOfRank(spellID, rank);
	if (result == 0)
		luaL_error(L, "AI.GetSpellOfRank: error, check logs. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


int LuaBindsAI::AI_GetSpellRank(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	int spellID = luaL_checkinteger(L, 2);
	uint32 result = ai->GetSpellRank(spellID);
	if (result == 0)
		luaL_error(L, "AI.GetSpellRank: spell not found. %d", spellID);
	lua_pushinteger(L, result);
	return 1;
}


// -----------------------------------------------------------
//                         Chat
// -----------------------------------------------------------


int LuaBindsAI::AI_ChatPopNextWhisper(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	std::queue<std::string>& queue = ai->ChatGetMsgQ();
	if (queue.size())
	{
		lua_pushstring(L, queue.front().c_str());
		queue.pop();
	}
	else
		lua_pushnil(L);
	return 1;
}


int LuaBindsAI::AI_ChatSendInvToMaster(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	bool ignoreEquipped = luaL_checkboolean(L, 2);
	ai->ChatSendInvToMaster(ignoreEquipped);
	return 0;
}


int LuaBindsAI::AI_ChatSendWhisper(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	ObjectGuid guid = Guid_GetGuidObject(L, 2)->guid;
	const char* text = luaL_checkstring(L, 3);

	if (Player* recipient = ObjectAccessor::FindPlayer(guid))
		ai->ChatSendWhisper(recipient, text);

	return 0;
}


// -----------------------------------------------------------
//                       Trading
// -----------------------------------------------------------


int LuaBindsAI::AI_TradeIsInProgress(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_pushboolean(L, ai->GetPlayer()->GetTradeData() != nullptr);
	return 1;
}


int LuaBindsAI::AI_TradeInitiate(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	ObjectGuid guid = Guid_GetGuidObject(L, 2)->guid;
	ai->TradeInitiate(guid);
	return 0;
}


int LuaBindsAI::AI_TradeAccept(lua_State* L)
{
	AI_GetAIObject(L)->TradeAccept();
	return 0;
}


int LuaBindsAI::AI_TradeBegin(lua_State* L)
{
	AI_GetAIObject(L)->TradeBegin();
	return 0;
}


int LuaBindsAI::AI_TradeCancel(lua_State* L)
{
	AI_GetAIObject(L)->TradeCancel();
	return 0;
}


int LuaBindsAI::AI_TradeAddItem(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer tradeSlot = luaL_checkinteger(L, 2);
	lua_Integer bag = luaL_checkinteger(L, 3);
	lua_Integer slot = luaL_checkinteger(L, 4);
	if (tradeSlot < 0 || bag < 0 || slot < 0)
		luaL_error(L, "AI_TradeAddItem: tradeSlot, bag, or slot was < 0 %lld, %lld, %lld", tradeSlot, bag, slot);
	ai->TradeAddItem(tradeSlot, bag, slot);
	return 0;
}


int LuaBindsAI::AI_TradeItemIsInTrade(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	lua_Integer bag = luaL_checkinteger(L, 2);
	lua_Integer slot = luaL_checkinteger(L, 3);
	if (bag < 0 || slot < 0)
		luaL_error(L, "AI_TradeItemIsInTrade: bag or slot was < 0 %lld, %lld", bag, slot);
	if (!bag) bag = INVENTORY_SLOT_BAG_0;
	Item* item = ai->GetPlayer()->GetItemByPos(bag, slot);
	TradeData* data = ai->GetPlayer()->GetTradeData();
	if (item && data)
		lua_pushboolean(L, data->HasItem(item->GetObjectGuid()));
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::AI_TradeIsAccepted(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L);
	if (TradeData* data = ai->GetPlayer()->GetTradeData())
		lua_pushboolean(L, data->IsAccepted());
	else
		lua_pushboolean(L, false);
	return 1;
}


// -----------------------------------------------------------
//                      Internals
// -----------------------------------------------------------


int LuaBindsAI::AI_GetPartyIntelligence(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	if (PartyIntelligence* pi = agent->GetPartyIntelligence())
		pi->PushUD(L);
	else
		lua_pushnil(L);
	return 1;
}


int LuaBindsAI::AI_GetRole(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	lua_pushinteger(L, int(agent->GetRole()));
	return 1;
}


int LuaBindsAI::AI_GetSpec(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	lua_pushstring(L, agent->GetSpec().c_str());
	return 1;
}


int LuaBindsAI::AI_GetMaster(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	Player* master = sObjectAccessor.FindPlayer(agent->GetMasterGuid());
	if (master && (!master->IsInWorld() || master->IsBeingTeleported()))
		master = nullptr;
	lua_pushplayerornil(L, master);
	return 1;
}


int LuaBindsAI::AI_GetMasterGuid(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	const ObjectGuid& masterGuid = agent->GetMasterGuid();
	masterGuid.IsEmpty() ? lua_pushnil(L) : Guid_CreateUD(L, masterGuid);
	return 1;
}


int LuaBindsAI::AI_GetPlayer(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	agent->PushPlayerUD(L);
	return 1;
}


int LuaBindsAI::AI_SetRole(lua_State* L)
{
	LuaAgent* agent = AI_GetAIObject(L);
	lua_Integer role = luaL_checkinteger(L, 2);
	if (role < 0 || role > lua_Integer(LuaAgentRoles::Max))
		luaL_error(L, "AI_SetRole: role %d doesn't exist. Allowed values [0, %d]", role, LuaAgentRoles::Max);
	agent->SetRole(LuaAgentRoles(role));
	return 0;
}
