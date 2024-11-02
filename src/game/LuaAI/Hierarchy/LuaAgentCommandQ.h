
#ifndef MANGOS_LUAAGENTCOMMANDQUEUE_H
#define MANGOS_LUAAGENTCOMMANDQUEUE_H

#include "lua.hpp"

enum class AgentCmdType : uint8
{
	None,
	Move,
	Follow,
	Engage,
	Tank,
	Heal,
	Pull,
	CC,
	Buff,
	Dispel,
	Script,
	Trade,
	Loot,
	Max,
};


class AgentCmd
{
public:
	enum class State : uint8
	{
		Waiting,
		InProgress,
		Complete,
		Failed,
		Max,
	};

protected:
	AgentCmdType tp;
	State state;

public:
	AgentCmd(AgentCmdType tp) : tp(tp), state(State::Waiting) {}
	virtual int Push(lua_State* L) { return 0; }
	int PushType(lua_State* L);
	AgentCmdType GetType() { return tp; }
	State GetState() { return state; }
	void SetState(State state) { this->state = state; }
	virtual bool MinReqMet() { return true; }
	virtual void MinReqProgress(lua_State* L, int idx) {}
	virtual int AddProgress(lua_State* L, int idx) { return 0; }
	virtual int GetProgress(lua_State* L) { return 0; }
};


class AgentCmdMove : public AgentCmd
{
	G3D::Vector3 destination;

public:
	AgentCmdMove(float x, float y, float z) : destination(x,y,z), AgentCmd(AgentCmdType::Move) {}
	int Push(lua_State* L) override;
};


class AgentCmdFollow : public AgentCmd
{
	ObjectGuid targetGuid;
	float angle;
	float dist;

public:
	AgentCmdFollow(const ObjectGuid& targetGuid, float dist, float angle) : targetGuid(targetGuid), AgentCmd(AgentCmdType::Follow), angle(angle), dist(dist) {}
	int Push(lua_State* L) override;
};


class AgentCmdEngage : public AgentCmd
{
	float angle;

public:
	AgentCmdEngage(float angle) : AgentCmd(AgentCmdType::Engage), angle(angle) {}
	int Push(lua_State* L) override;
};


class AgentCmdTank : public AgentCmd
{
	ObjectGuid targetGuid;
	lua_Number desiredThreat;
	lua_Number doneThreat;

public:
	AgentCmdTank(const ObjectGuid& targetGuid, lua_Number desiredThreat) :
		AgentCmd(AgentCmdType::Tank), targetGuid(targetGuid), desiredThreat(desiredThreat), doneThreat(0.0) {}
	int Push(lua_State* L) override;
	bool MinReqMet() override { return doneThreat >= desiredThreat; }
	void MinReqProgress(lua_State* L, int idx) override { doneThreat = luaL_checknumber(L, idx); }
	int GetProgress(lua_State* L) override { lua_pushnumber(L, doneThreat); return 1; }
};


class AgentCmdHeal : public AgentCmd
{
	ObjectGuid targetGuid;
	lua_Integer numHeals;
	lua_Integer curHeals;

public:
	AgentCmdHeal(const ObjectGuid& targetGuid, lua_Integer numHeals)
		: AgentCmd(AgentCmdType::Heal), targetGuid(targetGuid), numHeals(numHeals), curHeals(0ll) {}
	int Push(lua_State* L) override;
	int AddProgress(lua_State* L, int idx) override { curHeals++; return 0; }
	bool MinReqMet() override { return curHeals >= numHeals; }
	void MinReqProgress(lua_State* L, int idx) override { curHeals = luaL_checkinteger(L, idx); }
	int GetProgress(lua_State* L) override { lua_pushinteger(L, numHeals - curHeals); return 1; }
};


class AgentCmdPull : public AgentCmd
{
	ObjectGuid targetGuid;

public:
	AgentCmdPull(const ObjectGuid& targetGuid) : AgentCmd(AgentCmdType::Pull), targetGuid(targetGuid) {}
	int Push(lua_State* L) override;
};


class AgentCmdCC : public AgentCmd
{
	ObjectGuid targetGuid;

public:
	AgentCmdCC(const ObjectGuid& targetGuid) : AgentCmd(AgentCmdType::CC), targetGuid(targetGuid) {}
	int Push(lua_State* L) override;
};


class AgentCmdBuff : public AgentCmd
{
	ObjectGuid targetGuid;
	lua_Integer spellid;
	std::string key;

public:
	AgentCmdBuff(const ObjectGuid& targetGuid, lua_Integer spellid, std::string key)
		: AgentCmd(AgentCmdType::Buff), targetGuid(targetGuid), spellid(spellid), key(key) {}
	int Push(lua_State* L) override;
};


class AgentCmdDispel : public AgentCmd
{
	ObjectGuid targetGuid;
	std::string key;

public:
	AgentCmdDispel(const ObjectGuid& targetGuid, std::string key)
		: AgentCmd(AgentCmdType::Dispel), targetGuid(targetGuid), key(key) {}
	int Push(lua_State* L) override;
};


class AgentCmdScript : public AgentCmd
{
public:
	AgentCmdScript() : AgentCmd(AgentCmdType::Script) {}
	int Push(lua_State* L) override { return 0; }
};


class AgentCmdTrade : public AgentCmd
{
	ObjectGuid targetGuid;
	lua_Integer bag, slot;

public:
	AgentCmdTrade(const ObjectGuid& targetGuid, lua_Integer bag, lua_Integer slot)
		: AgentCmd(AgentCmdType::Trade), targetGuid(targetGuid), bag(bag), slot(slot) {}
	int Push(lua_State* L) override;
};


class AgentCmdLoot : public AgentCmd
{
	ObjectGuid targetGuid;
	lua_Integer itemId;

public:
	AgentCmdLoot(const ObjectGuid& targetGuid, lua_Integer itemId)
		: AgentCmd(AgentCmdType::Loot), targetGuid(targetGuid), itemId(itemId) {}
	int Push(lua_State* L) override;
};


class CmdQueue
{
	std::vector<AgentCmd> commands;

public:
	CmdQueue() {}

	auto begin() { return commands.begin(); }
	auto end() { return commands.end(); }
	size_t size() { return commands.size(); }
	AgentCmd& operator[](int i) { return commands[i]; }
};

#endif
