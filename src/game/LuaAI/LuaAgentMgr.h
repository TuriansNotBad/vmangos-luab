
#ifndef MANGOS_LUAAGENTMANAGER_H
#define MANGOS_LUAAGENTMANAGER_H

namespace LuaAI
{
	class TriggerRecordStorage;
}

struct lua_State;
struct DungeonData;
class PartyIntelligence;

typedef std::unordered_map<ObjectGuid, Player*> LuaAgentMap;


struct LuaAgentInfoHolder
{
	enum LBIHStatus
	{
		OFFLINE,
		LOADING,
		LOGGEDIN,
		TODELETE
	};
public:
	uint32 timeout;
	std::string name;
	const ObjectGuid masterGuid;
	int logicID;
	std::string spec;
	LBIHStatus status;
	LuaAgentInfoHolder(std::string name, const ObjectGuid& masterGuid, int logicID, std::string spec, uint32 timeout)
		: name(name), masterGuid(masterGuid), logicID(logicID), spec(spec), status(OFFLINE), timeout(timeout) {}
};


class LuaAgentMgr
{
	friend class LuaAgentCharacterHandler;

	std::mutex loginMutex;

	lua_State* L;

	bool m_bGroupAllInProgress;

	bool m_bDisableAgentSaving;
	bool m_bLuaCeaseUpdates;
	bool m_bLuaReload;
	int m_updateInterval;
	ShortTimeTracker m_updateTimer;

	std::set<std::string> m_loadedFiles;

	std::set<uint32> m_toRemoveParties;
	std::vector<std::unique_ptr<PartyIntelligence>> m_parties;
	LuaAgentMap m_agents;
	std::set<ObjectGuid> m_toRemove;
	std::map<ObjectGuid, LuaAgentInfoHolder> m_toAdd;

	std::unordered_map<uint32, std::unique_ptr<DungeonData>> m_dungeons;
	std::unique_ptr<LuaAI::TriggerRecordStorage> m_triggerRecordStorage;

	LuaAgentMgr();

	void __AddAgents();
	void __RemoveAgents();

	void SetGroupAllInProgress(bool value) { m_bGroupAllInProgress = value; }

	void LuaLoadAll();
	void LuaLoadFiles(const std::string& fpath);
	bool LoadDungeonData();
	static int __LoadDungeonData(lua_State* L);

	void SetLoggedIn(ObjectGuid guid);

protected:
	void EraseLoginInfo(ObjectGuid guid);

public:
	enum CheckResult
	{
		CHAR_DOESNT_EXIST,
		CHAR_ALREADY_LOGGED_IN,
		CHAR_ACCOUNT_DOESNT_EXIST,
		CHAR_ALREADY_EXISTS,
		CHAR_ALREADY_IN_QUEUE,
		CHAR_MASTER_SESSION_NOT_FOUND,
		CHAR_MASTER_PLAYER_NOT_FOUND,
		CHAR_OK,
	};

	~LuaAgentMgr() noexcept;

	lua_State* Lua() { return L; }

	void LuaReload() { m_bLuaReload = true; }
	bool IsReloading() const { return m_bLuaReload; }
	bool LuaDofile(const std::string& filename);
	bool LuaIsFiledLoaded(const std::string& fname) { return m_loadedFiles.find(fname) != m_loadedFiles.end(); }
	void LuaCeaseUpdates() { m_bLuaCeaseUpdates = true; }

	void ResetAgents();
	Player* GetAgent(ObjectGuid guid);
	const LuaAgentInfoHolder* GetLoginInfo(ObjectGuid guid);

	void AddParty(std::string name, const ObjectGuid& owner);
	void RemoveParty(std::string name, const ObjectGuid& owner);
	CheckResult AddAgent(std::string charName, ObjectGuid masterGuid, int logicID, std::string spec);
	CheckResult CheckAgentValid(std::string charName, ObjectGuid masterGuid);

	void OnAgentLogin(WorldSession* session, ObjectGuid guid, ObjectGuid masterGuid, int logicID, std::string spec);
	void LogoutAgent(ObjectGuid guid);
	void LogoutBrokenAgent(ObjectGuid guid);
	void LogoutAllAgents();
	void LogoutAllImmediately();
	void SetDisableSavingAgents(bool v) { m_bDisableAgentSaving = v; }

	void Update(uint32 diff);

	void GroupAll(Player* owner);
	void ReviveAll(Player* owner, float hp = 1.0, bool sickness = false);
	bool IsGroupAllInProgress() { return m_bGroupAllInProgress; }

	void CLineNewLine();
	void CLineSaveSeg(G3D::Vector3& point, Player* gm);
	void CLineMoveSeg(G3D::Vector3& newpos, Player* gm, const ObjectGuid& helper);
	void CLineDelLastSeg(Player* gm);
	void CLineWrite();
	void CLineLoadForEdit(Player* gm, uint32 mapId);
	void CLineLoad(const std::string& fname);
	void CLineFinish(Player* gm);
	DungeonData* GetDungeonData(uint32 key);
	LuaAI::TriggerRecordStorage* GetTriggerRecordStorage() { return m_triggerRecordStorage.get(); }

	static LuaAgentMgr& getInstance()
	{
		static LuaAgentMgr instance;
		return instance;
	}

	LuaAgentMgr(LuaAgentMgr const&) = delete;
	void operator=(LuaAgentMgr const&) = delete;

};

#define sLuaAgentMgr LuaAgentMgr::getInstance()

#endif
