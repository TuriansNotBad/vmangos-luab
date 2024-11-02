
#ifndef MANGOS_LUAAGENTPARTYINT_H
#define MANGOS_LUAAGENTPARTYINT_H

#include "lua.hpp"

namespace LuaAI
{
	class TriggerMgr;
}

const int PARTYINT_TIMER_COUNT_MAX = 20;
typedef std::unordered_map<ObjectGuid, Player*> LuaAgentMap;
class LuaAgent;
class Unit;
struct DungeonData;

class PartyIntelligence
{

public:
	static const char* PI_MTNAME;
	struct CCInfo
	{
		ObjectGuid guid;
		ObjectGuid agentGuid;
		CCInfo(const ObjectGuid& guid, const ObjectGuid& agentGuid) : guid(guid), agentGuid(agentGuid) {}
		bool Check(PartyIntelligence* intelligence);
	};
	struct AgentInfo
	{
		AgentInfo(std::string name, int logicId, std::string spec) : name(name), logicId(logicId), spec(spec) {}
		int logicId;
		std::string name;
		std::string spec;
	};

	PartyIntelligence(std::string name, ObjectGuid owner);
	~PartyIntelligence() noexcept;

	Player* GetAgent(const ObjectGuid& guid);
	LuaAgentMap& GetAgentMap() { return m_agents; }

	float GetAngleForTank(LuaAgent* tank, Unit* target, bool& flipped, bool allowFlip, bool forceFlip = false);
	DungeonData* GetDungeonData() { return m_dungeon; }
	bool HasCLineFor(Unit* agent);
	void InitTriggersForMap(lua_State* L, int mapId);

	void LoadInfoFromLuaTbl(lua_State* L);
	void LoadAgents();
	void RemoveAgent(const ObjectGuid& guid);
	void RemoveAll();
	void Reset(lua_State* L, bool dropRefs);
	void Init(lua_State* L);
	void Update(uint32 diff, lua_State* L);

	void CreateUD(lua_State* L);
	void PushUD(lua_State* L);
	void PushUV(lua_State* L);
	void Unref(lua_State* L);

	void SetCeaseUpdates(bool v) { m_bCeaseUpdates = v; }
	bool GetCeaseUpdates() { return m_bCeaseUpdates; }
	std::string GetName() { return m_name; }
	ObjectGuid GetOwnerGuid() { return m_owner; }

	void AddCC(const ObjectGuid& guid, const ObjectGuid& agentGuid) { m_cc.emplace(guid, CCInfo(guid, agentGuid)); }
	bool IsCC(const ObjectGuid& guid) { return m_cc.find(guid) != m_cc.end(); }
	std::unordered_map<ObjectGuid, CCInfo>& GetCCMap() { return m_cc; }
	void RemoveCC(const ObjectGuid& guid) { m_cc.erase(guid); }
	void UpdateCC();

	double GetTimer(int index);
	void SetTimer(int index, double time);
	bool HasTimerFinished(int index);

private:
	bool m_bCeaseUpdates;
	int m_userDataRef;
	ObjectGuid m_owner;

	int m_updateInterval;
	ShortTimeTracker m_updateTimer;

	DungeonData* m_dungeon;
	std::unique_ptr<LuaAI::TriggerMgr> m_triggerMgr;

	std::vector<AgentInfo> m_agentInfos;
	LuaAgentMap m_agents;

	std::string m_name;
	std::string m_init;
	std::string m_update;

	std::unordered_map<ObjectGuid, CCInfo> m_cc;

	std::array<clock_t, PARTYINT_TIMER_COUNT_MAX> m_timers;
};


namespace LuaBindsAI {
	void BindPartyIntelligence(lua_State* L);
	PartyIntelligence* PartyInt_GetPIObject(lua_State* L);
	void PartyInt_CreateMetatable(lua_State* L);

	int PartyInt_CanPullTarget(lua_State* L);

	int PartyInt_CmdBuff(lua_State* L);
	int PartyInt_CmdCC(lua_State* L);
	int PartyInt_CmdDispel(lua_State* L);
	int PartyInt_CmdEngage(lua_State* L);
	int PartyInt_CmdFollow(lua_State* L);
	int PartyInt_CmdHeal(lua_State* L);
	int PartyInt_CmdLoot(lua_State* L);
	int PartyInt_CmdTank(lua_State* L);
	int PartyInt_CmdPull(lua_State* L);
	int PartyInt_CmdScript(lua_State* L);
	int PartyInt_CmdTrade(lua_State* L);

	int PartyInt_GetData(lua_State* L);
	int PartyInt_GetOwnerGuid(lua_State* L);

	// cline

	int PartyInt_HasCLineFor(lua_State* L);
	int PartyInt_GetAngleForTank(lua_State* L);
	int PartyInt_GetCLinePInLosAtD(lua_State* L);
	int PartyInt_GetNearestCLineP(lua_State* L);
	int PartyInt_GetNextCLineS(lua_State* L);
	int PartyInt_GetCLineLen(lua_State* L);
	int PartyInt_GetCLineCount(lua_State* L);
	int PartyInt_ShouldReverseCLine(lua_State* L);

	// cc

	int PartyInt_AddCC(lua_State* L);
	int PartyInt_IsCC(lua_State* L);
	int PartyInt_RemoveCC(lua_State* L);

	int PartyInt_LoadInfoFromLuaTbl(lua_State* L);

	int PartyInt_GetAgents(lua_State* L);
	int PartyInt_GetAttackers(lua_State* L);
	int PartyInt_GetCCTable(lua_State* L);
	int PartyInt_RemoveAgent(lua_State* L);
	int PartyInt_RemoveAll(lua_State* L);

	// timer
	int PartyInt_SetTimer(lua_State* L);
	int PartyInt_GetTimer(lua_State* L);
	int PartyInt_IsFinishTimer(lua_State* L);

	int PartyInt_InitTriggersForMap(lua_State* L);

	static const struct luaL_Reg PartyInt_BindLib[]{
		{"CanPullTarget", PartyInt_CanPullTarget},

		{"CmdBuff", PartyInt_CmdBuff},
		{"CmdCC", PartyInt_CmdCC},
		{"CmdDispel", PartyInt_CmdDispel},
		{"CmdEngage", PartyInt_CmdEngage},
		{"CmdFollow", PartyInt_CmdFollow},
		{"CmdHeal", PartyInt_CmdHeal},
		{"CmdLoot", PartyInt_CmdLoot},
		{"CmdTank", PartyInt_CmdTank},
		{"CmdPull", PartyInt_CmdPull},
		{"CmdScript", PartyInt_CmdScript},
		{"CmdTrade", PartyInt_CmdTrade},

		{"GetData", PartyInt_GetData},
		{"GetOwnerGuid", PartyInt_GetOwnerGuid},

		// cline
		{"HasCLineFor", PartyInt_HasCLineFor},
		{"GetAngleForTank", PartyInt_GetAngleForTank},
		{"GetCLinePInLosAtD", PartyInt_GetCLinePInLosAtD},
		{"GetNearestCLineP", PartyInt_GetNearestCLineP},
		{"GetNextCLineS", PartyInt_GetNextCLineS},
		{"GetCLineLen", PartyInt_GetCLineLen},
		{"GetCLineCount", PartyInt_GetCLineCount},
		{"ShouldReverseCLine", PartyInt_ShouldReverseCLine},

		// cc
		{"AddCC", PartyInt_AddCC},
		{"IsCC", PartyInt_IsCC},
		{"RemoveCC", PartyInt_RemoveCC},

		{"LoadInfoFromLuaTbl", PartyInt_LoadInfoFromLuaTbl},

		{"GetAgents", PartyInt_GetAgents},
		{"GetAttackers", PartyInt_GetAttackers},
		{"GetCC", PartyInt_GetCCTable},
		{"RemoveAgent", PartyInt_RemoveAgent},
		{"RemoveAll", PartyInt_RemoveAll},

		// timers
		{"SetTimer", PartyInt_SetTimer},
		{"GetTimer", PartyInt_GetTimer},
		{"IsFinishTimer", PartyInt_IsFinishTimer},

		{"InitTriggersForMap", PartyInt_InitTriggersForMap},

		{NULL, NULL}
	};

}

#endif
