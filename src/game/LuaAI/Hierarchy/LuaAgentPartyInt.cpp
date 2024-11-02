
#include "LuaAgentPartyInt.h"
#include "LuaAI/LuaAgentUtils.h"
#include "LuaAI/LuaAgentMgr.h"
#include "LuaAI/LuaAgent.h"
#include "LuaAI/LuaAgentLibAI.h"
#include "LuaAI/LuaAgentLibWorldObj.h"
#include "LuaAI/LuaAgentLibUnit.h"
#include "LuaAI/Libs/CLine.h"
#include "LuaAI/Libs/ObserveRegion.h"


const char* PartyIntelligence::PI_MTNAME = "Object.PartyInt";


namespace
{
	void AddAttackersFrom(lua_State* L, Unit* unit, lua_Integer& idx, std::set<ObjectGuid>& _added)
	{
		for (const auto pAttacker : unit->GetAttackers())
		{
			if (_added.find(pAttacker->GetObjectGuid()) != _added.end())
				continue;
			//printf("Additing target to tbl %s attacking %s\n", pAttacker->GetName(), pMember->GetName());
			if (!unit->CanAttack(pAttacker, true))
				continue;
			LuaBindsAI::Unit_CreateUD(pAttacker, L); // pushes pAttacker userdata on top of stack
			lua_seti(L, -2, idx); // stack[-2][tblIdx] = stack[-1], pops pAttacker
			++idx;
			_added.insert(pAttacker->GetObjectGuid());
		}
	}
}


bool PartyIntelligence::CCInfo::Check(PartyIntelligence* intelligence)
{
	Player* agent = intelligence->GetAgent(agentGuid);
	if (!agent || !agent->GetLuaAI() || !agent->GetLuaAI()->IsReady() || !agent->IsAlive() )
		return false;
	Unit* unit = agent->GetMap()->GetUnit(guid);
	if (!unit || !unit->IsAlive())
		return false;
	return true;
}


PartyIntelligence::PartyIntelligence(std::string name, ObjectGuid owner) :
	m_name(name),
	m_userDataRef(LUA_NOREF),
	m_bCeaseUpdates(false),
	m_owner(owner),
	m_updateInterval(50),
	m_dungeon(nullptr),
	m_timers{}
{
	m_init = m_name + "_Init";
	m_update = m_name + "_Update";
	m_updateTimer.Reset(m_updateInterval);
	m_triggerMgr = std::make_unique<LuaAI::TriggerMgr>();
}


PartyIntelligence::~PartyIntelligence() noexcept
{
	if (lua_State* L = sLuaAgentMgr.Lua())
		Unref(L);
}


void PartyIntelligence::Init(lua_State* L)
{
	CreateUD(L);
	lua_getglobal(L, m_init.c_str());
	PushUD(L);
	if (lua_dopcall(L, 1, 0) != LUA_OK)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error party init: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
		SetCeaseUpdates(true);
		return;
	}
}


void PartyIntelligence::Reset(lua_State* L, bool dropRefs)
{
	SetCeaseUpdates(false);
	if (!L || dropRefs)
		m_userDataRef = LUA_NOREF;
	else
		Unref(L);
	m_agentInfos.clear();
	m_agents.clear();
	m_cc.clear();
	m_dungeon = nullptr;
	m_triggerMgr->Clear();
}


void PartyIntelligence::LoadInfoFromLuaTbl(lua_State* L)
{
	if (!m_agentInfos.empty())
		luaL_error(L, "PartyIntelligence::LoadInfoFromLuaTbl attempt to reinitialize info");
	if (!lua_istable(L, -1))
		luaL_error(L, "PartyIntelligence::LoadInfoFromLuaTbl expected table at the top of the stack");

	lua_Integer length = luaL_len(L, -1);
	for (lua_Integer i = 1; i <= length; ++i)
	{
		if (lua_geti(L, -1, i) == LUA_TTABLE)
		{
			lua_geti(L, -1, 1);
			lua_geti(L, -2, 2);
			lua_geti(L, -3, 3);
			std::string name = luaL_checkstring(L, -3);
			lua_Integer logic = luaL_checkinteger(L, -2);
			std::string spec = luaL_checkstring(L, -1);
			lua_pop(L, 3);
			m_agentInfos.emplace_back(name, logic, spec);
		}
		else
			luaL_error(L, "PartyIntelligence::LoadInfoFromLuaTbl value at idx %lld is not a table for party %s", i, m_name.c_str());
		lua_pop(L, 1); // pop table
	}
}


void PartyIntelligence::Update(uint32 diff, lua_State* L)
{
	if (m_userDataRef == LUA_NOREF || m_bCeaseUpdates)
		return;

	// process login
	if (m_agents.size() != m_agentInfos.size())
	{
		if (m_agents.size() > m_agentInfos.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "PartyIntelligence::Update m_agents = %d m_agentInfos = %d", m_agents.size(), m_agentInfos.size());
			SetCeaseUpdates(true);
			return;
		}
		LoadAgents();
		return;
	}

	// dungeondata update
	if (Player* owner = sObjectAccessor.FindPlayer(m_owner))
	{
		if (!m_dungeon || m_dungeon->mapId != owner->GetMapId())
			if (DungeonData* data = sLuaAgentMgr.GetDungeonData(owner->GetMapId()))
				m_dungeon = data;
			else
				m_dungeon = nullptr;

		// Update triggers for owner
		m_triggerMgr->UpdateForUnit(L, owner, this);
	}

	UpdateCC();

	lua_getglobal(L, m_update.c_str());
	PushUD(L);
	if (lua_dopcall(L, 1, 0) != LUA_OK)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Error party update: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
		SetCeaseUpdates(true);
		return;
	}
}


Player* PartyIntelligence::GetAgent(const ObjectGuid& guid)
{
	LuaAgentMap::const_iterator it = m_agents.find(guid);
	return (it == m_agents.end()) ? nullptr : it->second;
}


void PartyIntelligence::LoadAgents()
{
	for (auto& info : m_agentInfos)
	{
		ObjectGuid& agentGuid = sObjectMgr.GetPlayerGuidByName(info.name);
		if (agentGuid.IsEmpty())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "PartyIntelligence::LoadAgents player with name %s doesn't exist", info.name.c_str());
			SetCeaseUpdates(true);
			return;
		}
		if (GetAgent(agentGuid))
			continue;
		LuaAgentMgr::CheckResult result = sLuaAgentMgr.AddAgent(info.name, m_owner, info.logicId, info.spec);
		switch (result)
		{
		case LuaAgentMgr::CHAR_OK:
			break;
		case LuaAgentMgr::CHAR_ALREADY_EXISTS:
		{
			// if is already a bot add as our own
			Player* agent = sLuaAgentMgr.GetAgent(agentGuid);
			LuaAgent* agentAI = agent->GetLuaAI();
			if (!agentAI || agentAI->GetMasterGuid() != m_owner)
			{
				sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "PartyIntelligence::LoadAgents player with name %s is already owned", info.name.c_str());
				SetCeaseUpdates(true);
				return;
			}
			m_agents[agentGuid] = agent;
			agentAI->SetPartyIntelligence(this);
			break;
		}
		case LuaAgentMgr::CHAR_ALREADY_IN_QUEUE:
		{
			// if already queued as bot if owners match we can carry on
			const LuaAgentInfoHolder* loginInfo = sLuaAgentMgr.GetLoginInfo(agentGuid);
			if (loginInfo && loginInfo->masterGuid == m_owner)
				break;
		}
		default:
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "PartyIntelligence::LoadAgents player with name %s can not be added. Reason = %d", info.name.c_str(), result);
			SetCeaseUpdates(true);
			return;
		}
		}
	}
}


void PartyIntelligence::RemoveAgent(const ObjectGuid& guid)
{
	if (Player* agent = GetAgent(guid))
	{
		for (auto& it = m_agentInfos.begin(); it != m_agentInfos.end(); ++it)
		{
			if (it->name == agent->GetName())
			{
				it = m_agentInfos.erase(it);
				break;
			}
		}
		m_agents.erase(guid);
		sLuaAgentMgr.LogoutAgent(guid);
	}
}


void PartyIntelligence::RemoveAll()
{
	for (auto& it : m_agents)
		sLuaAgentMgr.LogoutAgent(it.first);
	m_agentInfos.clear();
	m_agents.clear();
}


float PartyIntelligence::GetAngleForTank(LuaAgent* ai, Unit* target, bool& flipped, bool allowFlip, bool forceFlip)
{
	flipped = false;
	Player* agent = ai->GetPlayer();
	if (!HasCLineFor(agent))
		return agent->GetOrientation();

	int resultS;
	float resultD, resultpct;
	G3D::Vector3 result;
	G3D::Vector3 from(agent->GetPositionX(), agent->GetPositionY(), agent->GetPositionZ());

	CLine& line = m_dungeon->lines[m_dungeon->ClosestP(from, result, resultD, resultS, resultpct)];

	G3D::Vector3& AB = line.pts[resultS + 1].pos - line.pts[resultS].pos;

	float a = std::atan2(AB.y, AB.x);
	a = (a >= 0.f) ? a : (2.f * M_PI_F + a); // 0 .. 360

	bool shouldFlip = forceFlip;
	if (!shouldFlip && allowFlip && !m_owner.IsEmpty())
		if (Player* owner = sObjectAccessor.FindPlayer(m_owner))
		{
			G3D::Vector3 ownerPos(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ());
			G3D::Vector3 targetPos(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
			G3D::Vector3& ownerToTarget = targetPos - ownerPos;
			// flip direction if owner is opposite side
			if (AB.dot(ownerToTarget) < 0.f)
				shouldFlip = true;
		}

	if (shouldFlip)
	{
		a = (a < M_PI_F) ? (a + M_PI_F) : (a - M_PI_F);
		flipped = true;
	}
		

	return a;
}


void PartyIntelligence::CreateUD(lua_State* L)
{
	// create userdata on top of the stack pointing to a pointer of a PI object
	PartyIntelligence** piud = static_cast<PartyIntelligence**>(lua_newuserdatauv(L, sizeof(PartyIntelligence*), 1));
	*piud = this; // swap the PI object being pointed to to the current instance
	luaL_setmetatable(L, PI_MTNAME);
	lua_newtable(L);
	lua_setiuservalue(L, -2, 1);
	// save this userdata in the registry table.
	m_userDataRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops
}


void PartyIntelligence::PushUD(lua_State* L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_userDataRef);
}


void PartyIntelligence::PushUV(lua_State* L)
{
	lua_getiuservalue(L, -1, 1);
}


void PartyIntelligence::Unref(lua_State* L)
{
	if (m_userDataRef != LUA_NOREF && m_userDataRef != LUA_REFNIL)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, m_userDataRef);
		m_userDataRef = LUA_NOREF;
	}
}


bool PartyIntelligence::HasCLineFor(Unit* agent)
{
	if (m_dungeon ? m_dungeon->mapId == agent->GetMapId() : false)
		return m_dungeon->lines.size() > 0;
	return false;
}


void PartyIntelligence::InitTriggersForMap(lua_State* L, int mapId)
{
	m_triggerMgr->Clear();
	m_triggerMgr->InitForMap(L, mapId);
}


void PartyIntelligence::UpdateCC()
{
	for (auto& it = m_cc.begin(); it != m_cc.end();)
		if (!it->second.Check(this))
			it = m_cc.erase(it);
		else
			++it;
}


double PartyIntelligence::GetTimer(int index)
{
	return double(m_timers[index] - clock()) / (double) CLOCKS_PER_SEC;
}


void PartyIntelligence::SetTimer(int index, double time)
{
	m_timers[index] = clock() + CLOCKS_PER_SEC * time;
}


bool PartyIntelligence::HasTimerFinished(int index)
{
	return clock() >= m_timers[index];
}


// ---------------------------------------------------------
//    LUA BINDS
// ---------------------------------------------------------


PartyIntelligence* LuaBindsAI::PartyInt_GetPIObject(lua_State* L)
{
	return *((PartyIntelligence**) luaL_checkudata(L, 1, PartyIntelligence::PI_MTNAME));
}


void LuaBindsAI::BindPartyIntelligence(lua_State* L)
{
	PartyInt_CreateMetatable(L);
}


void LuaBindsAI::PartyInt_CreateMetatable(lua_State* L)
{
	luaL_newmetatable(L, PartyIntelligence::PI_MTNAME);
	lua_pushvalue(L, -1); // copy mt cos setfield pops
	lua_setfield(L, -1, "__index"); // mt.__index = mt
	luaL_setfuncs(L, PartyInt_BindLib, 0); // copy funcs
	lua_pop(L, 1); // pop mt
}


int LuaBindsAI::PartyInt_CanPullTarget(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	if (DungeonData* data = intelligence->GetDungeonData())
		if (auto encounter = data->GetEncounter(target->GetName()))
		{
			lua_pushboolean(L, false);
			return 1;
		}
	lua_pushboolean(L, true);
	return 1;
}


int LuaBindsAI::PartyInt_CmdBuff(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Integer spellid = luaL_checkinteger(L, 4);
	AgentCmdBuff* cmd = new AgentCmdBuff(guid->guid, spellid, luaL_checkstring(L, 5));
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdCC(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	AgentCmdCC* cmd = new AgentCmdCC(guid->guid);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdDispel(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	AgentCmdDispel* cmd = new AgentCmdDispel(guid->guid, luaL_checkstring(L, 4));
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdEngage(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	//LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Number angle = luaL_checknumber(L, 3);
	AgentCmdEngage* cmd = new AgentCmdEngage(angle);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdFollow(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Number dist = luaL_checknumber(L, 4);
	lua_Number angle = luaL_checknumber(L, 5);
	AgentCmdFollow* cmd = new AgentCmdFollow(guid->guid, dist, angle);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdHeal(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Integer numHeals = luaL_checkinteger(L, 4);
	AgentCmdHeal* cmd = new AgentCmdHeal(guid->guid, numHeals);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdLoot(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Integer itemId = luaL_checkinteger(L, 4);
	AgentCmdLoot* cmd = new AgentCmdLoot(guid->guid, itemId);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdTank(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Number desiredThreat = luaL_checknumber(L, 4);
	AgentCmdTank* cmd = new AgentCmdTank(guid->guid, desiredThreat);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdPull(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	AgentCmdPull* cmd = new AgentCmdPull(guid->guid);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdScript(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	AgentCmdScript* cmd = new AgentCmdScript();
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_CmdTrade(lua_State* L)
{
	LuaAgent* ai = AI_GetAIObject(L, 2);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 3);
	lua_Integer bag = luaL_checkinteger(L, 4);
	lua_Integer slot = luaL_checkinteger(L, 5);
	AgentCmdTrade* cmd = new AgentCmdTrade(guid->guid, bag, slot);
	lua_pushinteger(L, ai->CommandsAdd(cmd));
	return 1;
}


int LuaBindsAI::PartyInt_GetOwnerGuid(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Guid_CreateUD(L, intelligence->GetOwnerGuid());
	return 1;
}


int LuaBindsAI::PartyInt_GetData(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	intelligence->PushUV(L);
	return 1;
}


int LuaBindsAI::PartyInt_LoadInfoFromLuaTbl(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	intelligence->LoadInfoFromLuaTbl(L);
	return 0;
}


int LuaBindsAI::PartyInt_GetAgents(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_newtable(L);
	lua_Integer idx = 1;
	for (auto& it : intelligence->GetAgentMap())
	{
		Player* agent = it.second;
		if (LuaAgent* agentAI = agent->GetLuaAI())
		{
			if (agentAI->IsReady() && agentAI->IsInitialized() && agentAI->GetTopGoal()->GetActivated())
			{
				agentAI->PushUD(L);
				lua_seti(L, -2, idx);
				++idx;
			}
		}
	}
	return 1;
}


int LuaBindsAI::PartyInt_GetAttackers(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_newtable(L);
	lua_Integer idx = 1;
	std::set<ObjectGuid> _added;
	for (auto& it : intelligence->GetAgentMap())
		AddAttackersFrom(L, it.second->ToUnit(), idx, _added);

	if (Player* owner = sObjectAccessor.FindPlayer(intelligence->GetOwnerGuid()))
		AddAttackersFrom(L, owner->ToUnit(), idx, _added);

	return 1;
}


int LuaBindsAI::PartyInt_GetAngleForTank(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	LuaAgent* ai = AI_GetAIObject(L, 2);
	Unit* target = Unit_GetUnitObject(L, 3);
	bool allowFlip = luaL_checkboolean(L, 4);
	bool forceFlip = luaL_checkboolean(L, 5);
	bool flipped;
	lua_pushnumber(L, intelligence->GetAngleForTank(ai, target, flipped, allowFlip, forceFlip));
	lua_pushboolean(L, flipped);
	return 2;
}


int LuaBindsAI::PartyInt_GetCLinePInLosAtD(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* agent = Unit_GetUnitObject(L, 2);
	Unit* target = Unit_GetUnitObject(L, 3);
	Unit* losTarget = Unit_GetUnitObject(L, 4);
	lua_Number minD = luaL_checknumber(L, 5);
	lua_Number maxD = luaL_checknumber(L, 6);
	lua_Number step = luaL_checknumber(L, 7);
	bool reverse = luaL_checkboolean(L, 8);
	lua_Integer lineIdx = lua_gettop(L) > 8 ? luaL_checkinteger(L, 9) : -1;
	DungeonData* cline = intelligence->GetDungeonData();
	if (!cline)
		luaL_error(L, "PartyInt_GetCLinePInLosAtD: cline doesn't exist");
	if (cline->mapId != agent->GetMapId())
		luaL_error(L, "PartyInt_GetCLinePInLosAtD: cline map mismatch");
	G3D::Vector3 result;
	bool found = cline->GetPointInLosAtD(agent, target, losTarget, result, minD, maxD, step, reverse, lineIdx);
	if (!found || agent->GetDistanceSqr(result.x, result.y, result.z) < 4.0f)
	{
		lua_pushnil(L);
		return 1;
	}
	result.z = agent->GetMap()->GetHeight(result.x, result.y, result.z);
	lua_pushnumber(L, result.x);
	lua_pushnumber(L, result.y);
	lua_pushnumber(L, result.z);
	return 3;
}


int LuaBindsAI::PartyInt_HasCLineFor(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* unit = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, intelligence->HasCLineFor(unit));
	return 1;
}


int LuaBindsAI::PartyInt_GetNearestCLineP(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* unit = Unit_GetUnitObject(L, 2);
	float x, y, z;
	if (lua_gettop(L) < 3)
		x = unit->GetPositionX(), y = unit->GetPositionY(), z = unit->GetPositionZ();
	else
		x = luaL_checknumber(L, 3), y = luaL_checknumber(L, 4), z = luaL_checknumber(L, 5);
	DungeonData* cline = intelligence->GetDungeonData();
	if (!cline)
		luaL_error(L, "PartyInt_GetNearestCLineP: cline doesn't exist");
	if (cline->mapId != unit->GetMapId())
		luaL_error(L, "PartyInt_GetNearestCLineP: cline map mismatch");
	G3D::Vector3 P;
	float D, pct;
	int S;
	int line = cline->ClosestP(G3D::Vector3(x,y,z), P, D, S, pct);
	lua_pushnumber(L, P.x);
	lua_pushnumber(L, P.y);
	lua_pushnumber(L, P.z);
	lua_pushnumber(L, D);
	lua_pushinteger(L, S);
	lua_pushinteger(L, line);
	return 6;
}


int LuaBindsAI::PartyInt_GetNextCLineS(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* unit = Unit_GetUnitObject(L, 2);
	lua_Integer lineIdx = luaL_checkinteger(L, 3);
	lua_Integer S = luaL_checkinteger(L, 4);
	bool reverse = luaL_checkboolean(L, 5);
	DungeonData* cline = intelligence->GetDungeonData();
	if (!cline)
		luaL_error(L, "PartyInt_GetNextCLineS: cline doesn't exist");
	if (cline->mapId != unit->GetMapId())
		luaL_error(L, "PartyInt_GetNextCLineS: cline map mismatch");
	if (cline->lines.size() <= lineIdx || lineIdx < 0)
		luaL_error(L, "PartyInt_GetNextCLineS: cline only has %d lines, got %d", cline->lines.size(), lineIdx);
	if (cline->lines[lineIdx].pts.size() <= S || S < 0)
		luaL_error(L, "PartyInt_GetNextCLineS: idx check fail, cline %d only has %d pts, got %d", lineIdx, cline->lines[lineIdx].pts.size(), S);

	G3D::Vector3* P; // = &cline->lines[lineIdx].pts[0].pos;
	//if (S == 0 && lineIdx == 0);
	//else
	{
		CLine& line = cline->lines[lineIdx];
		//if (S == 0)
		//{
		//	--lineIdx;
		//	line = cline->lines[lineIdx];
		//	S = line.pts.size() - 1;
		//	P = &line.pts.back().pos;
		//}
		//else
		{
			int finalS = reverse ? 0 : (line.pts.size() - 1);
			int inc = reverse ? -1 : 1;
			if (S != finalS) S += inc;
			P = &line.pts[S].pos;
		}
	}
	lua_pushnumber(L, P->x);
	lua_pushnumber(L, P->y);
	lua_pushnumber(L, P->z);
	lua_pushinteger(L, S);
	lua_pushinteger(L, lineIdx);
	return 5;
}


int LuaBindsAI::PartyInt_GetCLineLen(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* unit = Unit_GetUnitObject(L, 2);
	lua_Integer lineIdx = luaL_checkinteger(L, 3);
	DungeonData* cline = intelligence->GetDungeonData();
	if (!cline)
		luaL_error(L, "PartyInt_GetCLineLen: cline doesn't exist");
	if (lineIdx < 0 || lineIdx >= cline->lines.size())
		luaL_error(L, "PartyInt_GetCLineLen: index out of bounds, got %I, max %d", lineIdx, cline->lines.size());
	lua_pushinteger(L, cline->lines[lineIdx].pts.size());
	return 1;
}


int LuaBindsAI::PartyInt_GetCLineCount(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* unit = Unit_GetUnitObject(L, 2);
	DungeonData* cline = intelligence->GetDungeonData();
	if (!cline)
		luaL_error(L, "PartyInt_GetCLineCount: cline doesn't exist");
	lua_pushinteger(L, cline->lines.size());
	return 1;
}


int LuaBindsAI::PartyInt_ShouldReverseCLine(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* unit = Unit_GetUnitObject(L, 2);
	Unit* target = Unit_GetUnitObject(L, 3);
	bool useTargetLine = lua_gettop(L) > 3 ? luaL_checkboolean(L, 4) : false;
	DungeonData* cline = intelligence->GetDungeonData();
	if (!cline)
		luaL_error(L, "PartyInt_ShouldReverseCLine: cline doesn't exist");
	if (cline->mapId != unit->GetMapId())
		luaL_error(L, "PartyInt_ShouldReverseCLine: cline map mismatch");

	if (Player* owner = sObjectAccessor.FindPlayer(intelligence->GetOwnerGuid()))
	{
		int resultS, lineIdx;
		float resultD, resultpct;
		G3D::Vector3 result;
		G3D::Vector3 from(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ());
		lineIdx = cline->ClosestP(from, result, resultD, resultS, resultpct);
		CLine& line = cline->lines[lineIdx];

		// If we're on the same line check if target's segment is further than my segment;
		{
			int targetSegment;
			G3D::Vector3 fromTarget(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
			int targetLineIdx = cline->ClosestP(fromTarget, result, resultD, targetSegment, resultpct);
			if (targetLineIdx == lineIdx && targetSegment != resultS)
			{
				lua_pushboolean(L, targetSegment < resultS);
				return 1;
			}
			if (useTargetLine)
			{
				line = cline->lines[targetLineIdx];
				resultS = targetSegment;
			}
		}

		G3D::Vector3& AB = line.pts[resultS + 1].pos - line.pts[resultS].pos;

		G3D::Vector3 ownerPos(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ());
		G3D::Vector3 targetPos(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
		G3D::Vector3& ownerToTarget = targetPos - ownerPos;
		// flip direction if owner is opposite side
		if (AB.dot(ownerToTarget) < 0.f)
		{
			lua_pushboolean(L, true);
			return 1;
		}
	}

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::PartyInt_AddCC(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	LuaObjectGuid* agentGuid = Guid_GetGuidObject(L, 2);
	LuaObjectGuid* targetGuid = Guid_GetGuidObject(L, 3);
	intelligence->AddCC(targetGuid->guid, agentGuid->guid);
	return 0;
}


int LuaBindsAI::PartyInt_IsCC(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, intelligence->IsCC(target->GetObjectGuid()));
	return 1;
}


int LuaBindsAI::PartyInt_RemoveCC(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	LuaObjectGuid* targetGuid = Guid_GetGuidObject(L, 2);
	intelligence->RemoveCC(targetGuid->guid);
	return 0;
}


int LuaBindsAI::PartyInt_GetCCTable(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_newtable(L);
	lua_Integer i = 1;
	for (auto& it : intelligence->GetCCMap())
	{
		PartyIntelligence::CCInfo& ccinfo = it.second;
		Player* agent = intelligence->GetAgent(ccinfo.agentGuid);
		Unit* target = agent->GetMap()->GetUnit(ccinfo.guid);
		if (!target || !agent)
			continue;
		lua_newtable(L);
		LuaAgent* ai = agent->GetLuaAI();
		ai->PushUD(L);
		lua_setfield(L, -2, "agent");
		Unit_CreateUD(target, L);
		lua_setfield(L, -2, "target");
		lua_seti(L, -2, i);
		++i;
	}
	return 1;
}


int LuaBindsAI::PartyInt_RemoveAgent(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 2);
	intelligence->RemoveAgent(guid->guid);
	return 0;
}


int LuaBindsAI::PartyInt_RemoveAll(lua_State* L)
{
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	intelligence->RemoveAll();
	return 0;
}

// ---------------------------------------------------------
//    TIMERS
// ---------------------------------------------------------


int LuaBindsAI::PartyInt_GetTimer(lua_State* L) {
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_Integer i = luaL_checkinteger(L, 2);
	if (i > -1 && i < PARTYINT_TIMER_COUNT_MAX)
		lua_pushnumber(L, intelligence->GetTimer(i));
	else
		luaL_error(L, "PartyIntelligence.GetTimer - number index out of bounds. Allowed - [0, %d)", PARTYINT_TIMER_COUNT_MAX);
	return 1;
}


int LuaBindsAI::PartyInt_SetTimer(lua_State* L) {
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_Integer i = luaL_checkinteger(L, 2);
	lua_Number n = luaL_checknumber(L, 3);
	if (i > -1 && i < PARTYINT_TIMER_COUNT_MAX)
		intelligence->SetTimer(i, n);
	else
		luaL_error(L, "PartyIntelligence.SetTimer - number index out of bounds. Allowed - [0, %d)", PARTYINT_TIMER_COUNT_MAX);
	return 0;
}


int LuaBindsAI::PartyInt_IsFinishTimer(lua_State* L) {
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_Integer i = luaL_checkinteger(L, 2);
	if (i > -1 && i < PARTYINT_TIMER_COUNT_MAX)
		lua_pushboolean(L, intelligence->HasTimerFinished(i));
	else
		luaL_error(L, "PartyIntelligence.IsFinishTimer - number index out of bounds. Allowed - [0, %d)", PARTYINT_TIMER_COUNT_MAX);
	return 1;
}


int LuaBindsAI::PartyInt_InitTriggersForMap(lua_State* L) {
	PartyIntelligence* intelligence = PartyInt_GetPIObject(L);
	lua_Integer mapId = luaL_checkinteger(L, 2);
	intelligence->InitTriggersForMap(L, mapId);
	return 0;
}
