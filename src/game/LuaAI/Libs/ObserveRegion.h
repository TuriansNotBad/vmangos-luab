
#ifndef MANGOS_LUAAGENTTRIGGERS_H
#define MANGOS_LUAAGENTTRIGGERS_H

#include <vector>
#include <unordered_map>
#include <string>
#include "LuaAI/LuaAgentUtils.h"

class Unit;
class lua_RefHolder;
class PartyIntelligence;
struct lua_State;

namespace LuaAI
{

	class TriggerRegionCircle
	{
		std::string m_name;
		float m_x, m_y, m_z, m_rsqr, m_maxZ, m_minZ;
		std::set<ObjectGuid> m_unitsInArea;
		lua_RefHolder* m_ref;

	public:

		TriggerRegionCircle(std::string name, float x, float y, float z, float r, float zr, lua_RefHolder* ref);

		bool UnitInArea(Unit* u);
		void UpdateForUnit(lua_State* L, Unit* u, PartyIntelligence* p);
		void CallLuaFn(lua_State* L, Unit* u, PartyIntelligence* p, std::string k);
		void OnEnter(lua_State* L, Unit* u, PartyIntelligence* p) { CallLuaFn(L, u, p, "OnEnter"); }
		void OnLeave(lua_State* L, Unit* u, PartyIntelligence* p) { CallLuaFn(L, u, p, "OnLeave"); }

	};

	class TriggerMgr
	{
		std::vector<TriggerRegionCircle> m_triggers;

	public:
		TriggerMgr();
		void Clear() { m_triggers.clear(); }
		void InitForMap(lua_State* L, int mapId);
		void UpdateForUnit(lua_State* L, Unit* u, PartyIntelligence* p);

	};

	struct TriggerRecord
	{
		lua_RefHolder m_table;
		TriggerRecord();
		bool Init(lua_State* L);
		void Unref(lua_State* L);
	};

	class TriggerRecordStorage
	{
		std::unordered_map<int, std::vector<TriggerRecord>> m_recordMap;

	public:
		TriggerRecordStorage();

		void RefTriggerRecord(int mapId, lua_State* L);
		bool HasTriggerRecordsForMap(int mapId);
		std::vector<TriggerRecord>& GetTriggerRecordsForMap(int mapId);
		void Clear(lua_State* L);
	};

}
#endif
