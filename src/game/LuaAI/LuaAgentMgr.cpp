
#include "Hierarchy/LuaAgentPartyInt.h"
#include "LuaAgent.h"
#include "LuaAgentMgr.h"
#include "AccountMgr.h"
#include "Database/DatabaseImpl.h"
#include "Group.h"
#include "lua.hpp"
#include "Goal/GoalManager.h"
#include "Goal/LogicManager.h"
#include "LuaAgentBindsCommon.h"
#include "LuaAgentUtils.h"
#include <experimental/filesystem>
#include <fstream>
#include "Libs/CLine.h"
#include "Libs/ObserveRegion.h"


class LuaAgentLoginQueryHolder : public LoginQueryHolder
{
public:
	int logicID;
	std::string spec;
	ObjectGuid masterGuid;
	LuaAgentLoginQueryHolder(uint32 accountId, ObjectGuid guid, ObjectGuid masterGuid, int logicID, std::string spec)
		: LoginQueryHolder(accountId, guid), masterGuid(masterGuid), logicID(logicID), spec(spec) { }
};


class LuaAgentCharacterHandler
{
public:
	void HandleLoginCallback(std::unique_ptr<QueryResult> /*dummy*/, SqlQueryHolder* holder)
	{
		if (!holder)
			return;

		LuaAgentLoginQueryHolder* lqh = dynamic_cast<LuaAgentLoginQueryHolder*>(holder);
		if (!lqh) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgent - login callback failed to cast SqlQueryHolder");
			delete holder;
			return;
		}

		// copy holder values
		ObjectGuid masterGuid = lqh->masterGuid;
		ObjectGuid myGuid = lqh->GetGuid();
		int logicID = lqh->logicID;
		std::string spec = lqh->spec;

		// already logged in or master's gone
		if (!ObjectAccessor::FindPlayerNotInWorld(lqh->masterGuid) || ObjectAccessor::FindPlayerNotInWorld(lqh->GetGuid()))
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Failed to add bot: already logged in %s", myGuid.GetString());
			delete holder;
			return;
		}

		// Deleted in LuaAgentMgr::__RemoveAgents
		printf("HandleLoginCallback for agent %s\n", myGuid.GetString().c_str());
		WorldSession* session = new WorldSession(lqh->GetAccountId(), nullptr, sAccountMgr.GetSecurity(lqh->GetAccountId()), 0, LOCALE_enUS);
		session->SetToLoading();
		session->HandlePlayerLogin(lqh); // will delete lqh
		// added?
		if (Player* pPlayer = sObjectAccessor.FindPlayerNotInWorld(myGuid))
			sLuaAgentMgr.OnAgentLogin(session, myGuid, masterGuid, logicID, spec);
		else
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Failed to add bot %s", myGuid.GetString());
			delete session;
			sLuaAgentMgr.EraseLoginInfo(myGuid);
		}

	}
} luaChrHandler;


LuaAgentMgr::LuaAgentMgr() :
	L(nullptr),
	m_updateInterval(50),
	m_bLuaCeaseUpdates(false),
	m_bLuaReload(true),
	m_bGroupAllInProgress(false),
	m_bDisableAgentSaving(true),
	m_dungeons(),
	m_loadedFiles(),
	m_bLuaReloadNoGear(false)
{
	m_updateTimer.Reset(0);
	m_triggerRecordStorage = std::make_unique<LuaAI::TriggerRecordStorage>();
}


LuaAgentMgr::~LuaAgentMgr() noexcept
{
	if (L) lua_close(L);
}


// ********************************************************
// **                  Lua basics                        **
// ********************************************************


bool LuaAgentMgr::LuaDofile(const std::string& filename) {
	// already compiled
	if (m_loadedFiles.find(filename) != m_loadedFiles.end())
		return true;
	m_loadedFiles.insert(filename);

	if ((luaL_loadfile(L, filename.c_str()) || lua_dopcall(L, 0, LUA_MULTRET)) != LUA_OK) {
		m_bLuaCeaseUpdates = true;
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr: Lua error executing file %s: %s\n", filename.c_str(), lua_tostring(L, -1));
		lua_pop(L, 1); // pop the error object
		return false;
	}
	return true;
}


void LuaAgentMgr::LuaLoadFiles(const std::string& fpath) {
	if (!L) return;

	m_loadedFiles.clear();

	// logic and goal list must be loaded before all other files.
	if (!LuaDofile((fpath + "/logic_list.lua")))
		return;
	if (!LuaDofile((fpath + "/goal_list.lua")))
		return;
	if (!LuaDofile((fpath + "/ai_define.lua")))
		return;

	// do all files recursively
	for (const auto& entry : std::experimental::filesystem::recursive_directory_iterator(fpath))
		if (entry.path().extension().generic_string() == ".lua")
			if (!LuaDofile(entry.path().generic_string()))
				return;

	//for (const auto& entry : std::experimental::filesystem::recursive_directory_iterator(fpath + "/data"))
	//	if (entry.path().extension().generic_string() == ".txt")
	//		CLineLoad(entry.path().generic_string());

	m_dungeons.clear();
	if (!LoadDungeonData())
		return;

	//for (auto& it : m_dungeons)
	//{
	//	it.second->WriteAsTable(std::to_string(it.first) + ".lua");
	//}

}


void LuaAgentMgr::LuaLoadAll() {
	std::string fpath = "ai";
	m_triggerRecordStorage->Clear(nullptr);
	if (L) lua_close(L); // kill old state
	L = nullptr;

	GoalManager::ClearRegistered();
	LogicManager::ClearRegistered();

	L = luaL_newstate();
	luaL_openlibs(L); // replace with individual libraries later

	LuaBindsAI::BindAll(L);

	LuaLoadFiles(fpath);

	// allow enabling saving in lua
	if (LUA_TBOOLEAN == lua_getglobal(L, "__Mgr_Disable_Agent_Save"))
		SetDisableSavingAgents(lua_toboolean(L, -1));
	lua_pop(L, 1);
}


// ********************************************************
// **                  Bot Management                    **
// ********************************************************


void LuaAgentMgr::Update(uint32 diff)
{

	// reload requested
	if (m_bLuaReload)
	{
		sWorld.SendServerMessage(SERVER_MSG_CUSTOM, "Lua reload started");

		LuaLoadAll();

		for (auto& it : m_agents)
		{
			// reset all
			if (LuaAgent* agent = it.second->GetLuaAI())
			{
				agent->Reset(true);
				agent->SetCeaseUpdates(false);
				if (m_bLuaReloadNoGear) agent->EquipSetShouldGenerate(false);
				else agent->EquipSetHasGeneratedGear(false);
			}
		}

		for (auto& party : m_parties)
		{
			party->Reset(L, true);
			party->Init(L);
		}

		m_bLuaReload = false;
		m_bLuaCeaseUpdates = false;
		m_bLuaReloadNoGear = false;

		sWorld.SendServerMessage(SERVER_MSG_CUSTOM, "Lua reload finished");
	}

	// lua error on initialization
	if (m_bLuaCeaseUpdates)
		return;

	m_updateTimer.Update(diff);
	if (m_updateTimer.Passed())
	{
		m_updateTimer.Reset(m_updateInterval);
		for (auto& party : m_parties)
			party->Update(diff, L);

		for (auto& it : m_agents)
		{
			LuaAgent* agent = it.second->GetLuaAI();
			if (!agent)
			{
				LogoutAgent(it.first);
				continue;
			}

			agent->Update(diff);
			agent->UpdateSession(diff);
		}
	}
	
	__RemoveAgents(); // process logout queue
	__AddAgents(); // process login queue

}


Player* LuaAgentMgr::GetAgent(ObjectGuid guid)
{
	LuaAgentMap::const_iterator it = m_agents.find(guid);
	return (it == m_agents.end()) ? nullptr : it->second;
}


const LuaAgentInfoHolder* LuaAgentMgr::GetLoginInfo(ObjectGuid guid)
{
	const std::lock_guard<std::mutex> lock(loginMutex);
	auto it = m_toAdd.find(guid);
	return (it == m_toAdd.end()) ? nullptr : &(it->second);
}


void LuaAgentMgr::EraseLoginInfo(ObjectGuid guid)
{
	const std::lock_guard<std::mutex> lock(loginMutex);
	auto it = m_toAdd.find(guid);
	if (it != m_toAdd.end())
		it->second.status = LuaAgentInfoHolder::TODELETE;
}


void LuaAgentMgr::SetLoggedIn(ObjectGuid guid)
{
	const std::lock_guard<std::mutex> lock(loginMutex);
	auto it = m_toAdd.find(guid);
	if (it != m_toAdd.end())
	{
		it->second.status = LuaAgentInfoHolder::LOGGEDIN;
		it->second.timeout = WorldTimer::getMSTime() + 60000;
	}
}


void LuaAgentMgr::AddParty(std::string name, const ObjectGuid& owner)
{
	for (auto& party : m_parties)
		if (party->GetName() == name && party->GetOwnerGuid() == owner)
			return;

	std::unique_ptr<PartyIntelligence> party = std::make_unique<PartyIntelligence>(name, owner);
	party->Init(L);
	m_parties.push_back(std::move(party));
}


void LuaAgentMgr::RemoveParty(std::string name, const ObjectGuid& owner)
{
	for (auto& it = m_parties.begin(); it != m_parties.end(); ++it)
	{
		auto party = it->get();
		if (party->GetName() == name && party->GetOwnerGuid() == owner)
		{
			party->RemoveAll();
			it = m_parties.erase(it);
			return;
		}
	}
}


LuaAgentMgr::CheckResult LuaAgentMgr::CheckAgentValid(std::string charName, ObjectGuid masterGuid)
{
	ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(charName);
	// character with name doesn't exist
	if (guid.IsEmpty())
		return CHAR_DOESNT_EXIST;
	// bot's account doesn't exist
	if (sObjectMgr.GetPlayerAccountIdByGUID(guid) == 0)
		return CHAR_ACCOUNT_DOESNT_EXIST;
	// already a bot
	if (GetAgent(guid))
		return CHAR_ALREADY_EXISTS;
	// already queued to add as bot
	if (GetLoginInfo(guid))
		return CHAR_ALREADY_IN_QUEUE;
	// already logged in
	if (sObjectAccessor.FindPlayerNotInWorld(guid))
		return CHAR_ALREADY_LOGGED_IN;
	// not all bots need master
	if (!masterGuid.IsEmpty())
	{
		// master player doesn't exist
		if (!ObjectAccessor::FindPlayerNotInWorld(masterGuid))
			return CHAR_MASTER_PLAYER_NOT_FOUND;
	}
	return CHAR_OK;
}


LuaAgentMgr::CheckResult LuaAgentMgr::AddAgent(std::string charName, ObjectGuid masterGuid, int logicID, std::string spec)
{
	CheckResult check = CheckAgentValid(charName, masterGuid);
	if (check == CHAR_OK)
	{
		const std::lock_guard<std::mutex> lock(loginMutex);
		m_toAdd.emplace(sObjectMgr.GetPlayerGuidByName(charName), LuaAgentInfoHolder(charName, masterGuid, logicID, spec, WorldTimer::getMSTime()));
	}
	return check;
}


void LuaAgentMgr::__AddAgents()
{
	const std::lock_guard<std::mutex> lock(loginMutex);
	for (std::map<ObjectGuid, LuaAgentInfoHolder>::iterator it = m_toAdd.begin(); it != m_toAdd.end();)
	{
		LuaAgentInfoHolder& info = it->second;
		if (info.status == LuaAgentInfoHolder::LOGGEDIN)
		{
			if (Player* player = ObjectAccessor::FindPlayerNotInWorld(it->first))
				m_agents[player->GetObjectGuid()] = player;
			else
				if (WorldTimer::getMSTime() < info.timeout)
					continue;
				else
					sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::__AddAgents: logged in agent not found and timed out %s", info.name.c_str());
			info.status = LuaAgentInfoHolder::TODELETE;
		}

		if (info.status == LuaAgentInfoHolder::TODELETE)
		{
			it = m_toAdd.erase(it);
			continue;
		}

		if (info.status != LuaAgentInfoHolder::OFFLINE)
		{
			++it;
			continue;
		}

		// not all bots need to be owned
		if (info.masterGuid.IsEmpty() || ObjectAccessor::FindPlayerNotInWorld(info.masterGuid))
		{
			ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(info.name);
			uint32 accountId = sObjectMgr.GetPlayerAccountIdByGUID(guid);
			LuaAgentLoginQueryHolder* holder = new LuaAgentLoginQueryHolder(accountId, guid, info.masterGuid, info.logicID, info.spec);
			if (!holder->Initialize())
			{
				delete holder;                                      // delete all unprocessed queries
				it = m_toAdd.erase(it);
				continue;
			}

			CharacterDatabase.DelayQueryHolderUnsafe(&luaChrHandler, &LuaAgentCharacterHandler::HandleLoginCallback, holder);
			info.status = LuaAgentInfoHolder::LOADING;
		}
		else
		{
			it = m_toAdd.erase(it);
			continue;
		}
		++it;
	}
}


void LuaAgentMgr::OnAgentLogin(WorldSession* session, ObjectGuid guid, ObjectGuid masterGuid, int logicID, std::string spec)
{
	Player* player = session->GetPlayer();
	if (!player)
	{
		LogoutAgent(guid);
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgent - player ptr is null after log in. Removing agent %s.", guid.GetString());
		return;
	}

	player->SetSavingDisabled(m_bDisableAgentSaving);

	player->CreateLuaAI(player, masterGuid, logicID);
	LuaAgent* ai = player->GetLuaAI();
	if (!ai)
	{
		LogoutAgent(guid);
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgent - ai failed to create after log in. Removing agent %s.", guid.GetString());
		return;
	}
	ai->SetSpec(spec);
	SetLoggedIn(player->GetObjectGuid());
	ai->OnLoggedIn();
}


void LuaAgentMgr::__RemoveAgents()
{
	for (auto& guid : m_toRemove)
		if (Player* player = GetAgent(guid))
		{
			if (WorldSession* session = player->GetSession())
			{
				// remove myself from group as LogoutPlayer won't do it without socket
				if (Group* group = player->GetGroup())
					session->HandleGroupDisbandOpcode(WorldPacket(CMSG_GROUP_DISBAND));
				session->LogoutPlayer(false);
				delete session;
			}
			m_agents.erase(guid);
		}
	m_toRemove.clear();

	for (auto itr = m_toRemoveParties.rbegin(); itr != m_toRemoveParties.rend(); ++itr)
		m_parties.erase(m_parties.begin() + *itr);
	m_toRemoveParties.clear();
}


void LuaAgentMgr::LogoutAgent(ObjectGuid guid)
{
	m_toRemove.insert(guid);
}


void LuaAgentMgr::LogoutBrokenAgent(ObjectGuid guid)
{
	if (GetAgent(guid))
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::LogoutBrokenAgent: agent was valid, logging out normally");
		LogoutAgent(guid);
		return;
	}

	Player* agent = sObjectAccessor.FindPlayerNotInWorld(guid);
	if (!agent)
		return;

	LuaAgent* ai = agent->GetLuaAI();
	if (!ai)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::LogoutBrokenAgent: player is not an agent");
		return;
	}

	if (WorldSession* session = agent->GetSession())
	{
		if (Group* group = agent->GetGroup())
			session->HandleGroupDisbandOpcode(WorldPacket(CMSG_GROUP_DISBAND));
		session->LogoutPlayer(false);
		delete session;
	}
}


void LuaAgentMgr::LogoutAllAgents()
{
	for (auto& itr : m_agents)
		m_toRemove.insert(itr.first);
	for (size_t i = 0; i < m_parties.size(); ++i)
		m_toRemoveParties.insert(i);
}


void LuaAgentMgr::LogoutAllImmediately()
{
	LogoutAllAgents();
	__RemoveAgents();
}


void LuaAgentMgr::ResetAgents()
{
	for (auto& itr : m_agents)
		if (LuaAgent* agent = itr.second->GetLuaAI())
			agent->Reset(false);
}

void LuaAgentMgr::ReviveAll(Player* owner, float hp, bool sickness)
{
	for (auto& itr : m_agents)
	{
		Player* player = itr.second;
		if (!player || player->IsAlive() || !player->IsInWorld() || player->IsBeingTeleported()) continue;

		LuaAgent* agent = player->GetLuaAI();
		Player* master = nullptr;
		if (agent)
			master = ObjectAccessor::FindPlayer(agent->GetMasterGuid());

		// owned bots only
		if (master && owner->GetObjectGuid() == agent->GetMasterGuid())
			player->ResurrectPlayer(hp, sickness);
	}
}


void LuaAgentMgr::GroupAll(Player* owner)
{
	SetGroupAllInProgress(true);
	for (auto& itr : m_agents)
	{
		Player* player = itr.second;
		if (!player || !player->IsInWorld() || player->IsBeingTeleported() || player->GetGroupInvite()) continue;

		if (LuaAgent* agent = player->GetLuaAI())
		{
			Player* master = ObjectAccessor::FindPlayer(agent->GetMasterGuid());

			if (master && owner->GetObjectGuid() == agent->GetMasterGuid())
			{
				// already grouped
				if (player->IsInSameRaidWith(master))
					continue;

				// leave my group if any
				player->GetSession()->HandleGroupDisbandOpcode(WorldPacket(CMSG_GROUP_DISBAND));

				if (Group* group = master->GetGroup())
				{
					std::unique_ptr<WorldPacket> packet = std::make_unique<WorldPacket>(CMSG_GROUP_INVITE);
					*packet << std::string(player->GetName());
					master->GetSession()->QueuePacket(std::move(packet));
				}
				else
				{
					// Adds agent and master to group in the same logic frame
					// Necessary as group isn't truly created until at least 2 players join
					// failing to invite more members in the same frame
					WorldPacket packet(CMSG_GROUP_INVITE);
					packet << std::string(player->GetName());
					master->GetSession()->HandleGroupInviteOpcode(packet);
				}
			}
		}
	}
	SetGroupAllInProgress(false);
}

// ********************************************************
// **                       CLINE                        **
// ********************************************************

namespace
{
	DungeonData currentLines;
	uint32 helperId = VISUAL_WAYPOINT;
}


void LuaAgentMgr::CLineSaveSeg(G3D::Vector3& point, Player* gm)
{
	int mapId = gm->GetMap()->GetId();
	if (currentLines.mapId == -1)
		currentLines.mapId = mapId;

	if (currentLines.lines.empty())
		currentLines.lines.emplace_back();
	CLine& currentCLine = currentLines.lines.back();

	if (mapId != currentLines.mapId)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::CLineSaveSeg: attempt to start a new cline before current is finished");
		return;
	}

	currentCLine.Point(point, ObjectGuid());
	currentCLine.pts.back().SummonHelper(gm, helperId);
}


void LuaAgentMgr::CLineMoveSeg(G3D::Vector3& newpos, Player* gm, const ObjectGuid& helper)
{
	int mapId = gm->GetMap()->GetId();
	if (mapId != currentLines.mapId)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::CLineMoveSeg: attempt to edit cline from a different map");
		return;
	}

	if (CLinePoint* point = currentLines.FindPoint(helper))
		point->Move(newpos, gm, helperId);
	else
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::CLineMoveSeg: point not found");
}


void LuaAgentMgr::CLineDelLastSeg(Player* gm)
{
	if (currentLines.lines.size())
	{
		CLine& line = currentLines.lines.back();
		if (line.pts.size())
		{
			CLinePoint& cpt = line.pts.back();
			if (!cpt.helper.IsEmpty())
				if (Creature* c = gm->GetMap()->GetCreature(cpt.helper))
					c->Kill(c, nullptr, false);
			line.pts.pop_back();
		}
	}
}


void LuaAgentMgr::CLineWrite()
{
	currentLines.WriteAsTable("out_cline.lua");
}


void LuaAgentMgr::CLineFinish(Player* gm)
{
	while (currentLines.lines.size())
	{
		CLine& line = currentLines.lines.back();
		while (line.pts.size())
			CLineDelLastSeg(gm);
		currentLines.lines.pop_back();
	}
	currentLines = DungeonData();
}


void LuaAgentMgr::CLineNewLine()
{
	if (currentLines.lines.empty())
		currentLines.lines.emplace_back();
	else if (currentLines.lines.back().pts.empty())
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::CLineNewLine: latest line has no points");
	else
		currentLines.lines.emplace_back();
}


void LuaAgentMgr::CLineLoadForEdit(Player* gm, uint32 mapId)
{
	CLineFinish(gm);
	if (DungeonData* data = GetDungeonData(mapId))
	{
		currentLines = *data;
		currentLines.SummonHelpers(gm, helperId);
	}
	else
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr::CLineLoadForEdit: can't find data for map %d", mapId);
		return;
	}
}


void LuaAgentMgr::CLineLoad(const std::string& fname)
{
	std::unique_ptr<DungeonData> net = std::make_unique<DungeonData>();
	net->Load(fname, nullptr, 0u);
	if (net->mapId)
		m_dungeons.emplace(net->mapId, std::move(net));
}


DungeonData* LuaAgentMgr::GetDungeonData(uint32 key)
{
	auto it = m_dungeons.find(key);
	return (it == m_dungeons.end()) ? nullptr : it->second.get();
}


void DungeonData::LoadFromTable(lua_State* L, uint32 mapId, Player* gm, uint32 helperId)
{
	if (lines.size())
	{
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::LoadFromTable lines vector is not empty");
		return;
	}

	this->mapId = mapId;
	lua_Integer nLines = luaL_len(L, -1);
	for (lua_Integer i = 1; i <= nLines; ++i)
	{
		if (lua_geti(L, -1, i) == LUA_TTABLE)
		{
			lines.emplace_back();
			CLine& line = lines.back();
			lua_Integer nPoints = luaL_len(L, -1);
			for (lua_Integer j = 1; j <= nPoints; ++j)
			{
				if (lua_geti(L, -1, j) == LUA_TTABLE)
				{
					line.pts.emplace_back();
					CLinePoint& P = line.pts.back();
					lua_geti(L, -1, 1);
					lua_geti(L, -2, 2);
					lua_geti(L, -3, 3);
					lua_geti(L, -4, 4);
					lua_geti(L, -5, 5);
					P.R     = luaL_checknumber(L, -1);
					P.minz  = luaL_checknumber(L, -2);
					P.pos.z = luaL_checknumber(L, -3);
					P.pos.y = luaL_checknumber(L, -4);
					P.pos.x = luaL_checknumber(L, -5);
					lua_pop(L, 5);
					if (gm)
						P.SummonHelper(gm, helperId);
				}
				else
					luaL_error(L, "DungeonData::LoadFromTable point at idx %d of line %d is not a table for map %d - type %s", j, i, mapId, luaL_typename(L, -1));
				lua_pop(L, 1);
			}
		}
		else
			luaL_error(L, "DungeonData::LoadFromTable line at idx %d is not a table for map %d - type %s", i, mapId, luaL_typename(L, -1));
		lua_pop(L, 1);
	}

	if (lua_getfield(L, -1, "encounters") == LUA_TTABLE)
	{
		lua_Integer n = luaL_len(L, -1);
		for (int i = 1; i <= n; ++i)
		{
			if (lua_geti(L, -1, i) == LUA_TTABLE)
			{
				lua_getfield(L, -1, "name");
				std::string name = luaL_checkstring(L, -1);
				G3D::Vector3 pos;
				bool forcePos = false;
				if (lua_getfield(L, -2, "tpos") == LUA_TTABLE)
				{
					forcePos = true;
					lua_geti(L, -1, 1);
					lua_geti(L, -2, 2);
					lua_geti(L, -3, 3);
					pos.x = luaL_checknumber(L, -3);
					pos.y = luaL_checknumber(L, -2);
					pos.z = luaL_checknumber(L, -1);
					lua_pop(L, 3);
				}
				EncounterData encounterData(ObjectGuid(), name, pos, forcePos);
				encounters.emplace(name, std::move(encounterData));
				lua_pop(L, 2);
			}
			else
				luaL_error(L, "DungeonData::LoadFromTable encounter at idx %d is not a table for map %d - type %s", i, mapId, luaL_typename(L, -1));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}


bool LuaAgentMgr::LoadDungeonData()
{
	if (m_dungeons.empty())
	{
		lua_pushcfunction(L, __LoadDungeonData);
		lua_pushlightuserdata(L, &m_dungeons);
		lua_pushlightuserdata(L, m_triggerRecordStorage.get());
		if (lua_dopcall(L, 2, 0) != LUA_OK)
		{
			m_bLuaCeaseUpdates = true;
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "LuaAgentMgr: Lua error loading dungeon data: %s", lua_tostring(L, -1));
			lua_pop(L, 1); // pop the error object
			return false;
		}
	}
	return true;
}


int LuaAgentMgr::__LoadDungeonData(lua_State* L)
{
	luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
	luaL_checktype(L, -2, LUA_TLIGHTUSERDATA);
	auto m_dungeons = static_cast<std::unordered_map<uint32, std::unique_ptr<DungeonData>>*>(lua_touserdata(L, -2));
	auto m_triggerRecordStorage = static_cast<LuaAI::TriggerRecordStorage*>(lua_touserdata(L, -1));
	lua_getglobal(L, "t_dungeons");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_Integer mapId = luaL_checknumber(L, -2);
		if (!sMapMgr.IsValidMAP(mapId))
			luaL_error(L, "LuaAgentMgr::LoadDungeonData: map %d does not exist", mapId);
		std::unique_ptr<DungeonData> data = std::make_unique<DungeonData>();
		data->LoadFromTable(L, mapId);
		m_dungeons->emplace(data->mapId, std::move(data));

		// load triggers
		if (lua_getfield(L, -1, "triggers") == LUA_TTABLE)
		{
			lua_Integer nLines = luaL_len(L, -1);
			for (lua_Integer i = 1; i <= nLines; ++i)
			{
				if (lua_geti(L, -1, i) == LUA_TTABLE)
					m_triggerRecordStorage->RefTriggerRecord(mapId, L);
				else
					luaL_error(L, "LuaAgentMgr::__LoadDungeonData trigger idx %d is not a table for map %d - type %s", i, mapId, luaL_typename(L, -1));
				lua_pop(L, 1);
			}
		}

		lua_pop(L, 2);
	}
	lua_pop(L, 1);
	return 0;
}
