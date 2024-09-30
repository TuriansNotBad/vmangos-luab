
#include "lua.hpp"
#include "ObserveRegion.h"
#include "LuaAI/LuaAgentUtils.h"
#include "LuaAI/Hierarchy/LuaAgentPartyInt.h"
#include "LuaAI/LuaAgentMgr.h"

LuaAI::TriggerRegionCircle::TriggerRegionCircle(std::string name, float x, float y, float  z, float r, float zr, lua_RefHolder* ref)
	: m_name(name), m_x(x), m_y(y), m_z(z), m_rsqr(r*r), m_minZ(z - zr), m_maxZ(z + zr), m_ref(ref), m_unitsInArea() {}


bool LuaAI::TriggerRegionCircle::UnitInArea(Unit* u)
{
	float ux = u->GetPositionX(), uy = u->GetPositionY(), uz = u->GetPositionZ();
	return (ux - m_x) * (ux - m_x) + (uy - m_y) * (uy - m_y) + (uz - m_z) * (uz - m_z) < m_rsqr && uz > m_minZ && uz < m_maxZ;
}


void LuaAI::TriggerRegionCircle::UpdateForUnit(lua_State* L, Unit* u, PartyIntelligence* p)
{
	if (m_unitsInArea.find(u->GetObjectGuid()) != m_unitsInArea.end())
	{
		if (!UnitInArea(u))
		{
			m_unitsInArea.erase(u->GetObjectGuid());
			OnLeave(L,u,p);
		}
	}
	else
	{
		if (UnitInArea(u))
		{
			m_unitsInArea.insert(u->GetObjectGuid());
			OnEnter(L,u,p);
		}
	}
}


void LuaAI::TriggerRegionCircle::CallLuaFn(lua_State* L, Unit* u, PartyIntelligence* p, std::string k)
{
	if (m_ref->Push(L) == LUA_TTABLE)
	{
		if (lua_getfield(L, -1, k.c_str()) == LUA_TFUNCTION)
		{
			lua_pushunitornil(L, u);
			p->PushUD(L);
			p->PushUV(L);
			if (lua_dopcall(L, 3, 0) != LUA_OK)
			{
				sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "TriggerRegionCircle::CallLuaFn: Lua error calling trigger handler: %s", k.c_str());
				sLuaAgentMgr.LuaCeaseUpdates();
				lua_pop(L, 1); // pop the error object
				return;
			}
		}
		else
			lua_pop(L, 1);
	}
	lua_pop(L, 1);
}


LuaAI::TriggerMgr::TriggerMgr() : m_triggers{} {}
void LuaAI::TriggerMgr::InitForMap(lua_State* L, int mapId)
{
	auto triggersStorage = sLuaAgentMgr.GetTriggerRecordStorage();
	if (triggersStorage->HasTriggerRecordsForMap(mapId))
	{
		for (auto& trigger : triggersStorage->GetTriggerRecordsForMap(mapId))
		{
			if (trigger.m_table.Push(L) == LUA_TTABLE)
			{
				float x, y, z, r, zr;
				std::string name;
				if (luaL_setnfromfield(L, -1, "x", x)
					&& luaL_setnfromfield(L, -1, "y", y)
					&& luaL_setnfromfield(L, -1, "z", z)
					&& luaL_setnfromfield(L, -1, "r", r)
					&& luaL_setnfromfield(L, -1, "zr", zr)
					&& luaL_setsfromfield(L, -1, "name", name))
				{
					if (lua_getfield(L, -1, "OnEnter") == LUA_TFUNCTION)
						m_triggers.emplace_back(TriggerRegionCircle(name, x, y, z, r, zr, &trigger.m_table));
					else
						sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "TriggerMgr::InitForMap: OnEnter handler not specified for trigger %s", mapId, name.c_str());
					lua_pop(L, 1);
				}
				else
					sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "TriggerMgr::InitForMap: failed to load triggers for map %d", mapId);
			}
			lua_pop(L, 1);
		}
	}
}


void LuaAI::TriggerMgr::UpdateForUnit(lua_State* L, Unit* u, PartyIntelligence* p)
{
	for (auto& trigger : m_triggers)
		trigger.UpdateForUnit(L,u,p);
}


LuaAI::TriggerRecord::TriggerRecord() : m_table(LUA_TTABLE) {}
bool LuaAI::TriggerRecord::Init(lua_State* L)
{
	return m_table.Ref(L);
}


void LuaAI::TriggerRecord::Unref(lua_State* L)
{
	m_table.Unref(L);
}


LuaAI::TriggerRecordStorage::TriggerRecordStorage() : m_recordMap() {}
bool LuaAI::TriggerRecordStorage::HasTriggerRecordsForMap(int mapId)
{
	return m_recordMap.find(mapId) != m_recordMap.end();
}


std::vector<LuaAI::TriggerRecord>& LuaAI::TriggerRecordStorage::GetTriggerRecordsForMap(int mapId)
{
	return m_recordMap[mapId];
}


void LuaAI::TriggerRecordStorage::RefTriggerRecord(int mapId, lua_State* L)
{
	if (!HasTriggerRecordsForMap(mapId))
		m_recordMap.emplace(mapId, std::vector<LuaAI::TriggerRecord>());

	auto& triggers = GetTriggerRecordsForMap(mapId);
	triggers.emplace_back();
	if (!triggers.back().Init(L))
	{
		triggers.pop_back();
		if (triggers.size() == 0)
			m_recordMap.erase(mapId);
		luaL_error(L, "TriggerRecordStorage::RefTriggerRecord: expected table on top of lua stack");
	}
}


void LuaAI::TriggerRecordStorage::Clear(lua_State* L)
{
	if (L)
		for (auto& triggers : m_recordMap)
			for (auto& trigger : triggers.second)
				trigger.Unref(L);
	m_recordMap.clear();
}
