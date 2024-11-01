
#ifndef MANGOS_LUAAGENT_H
#define MANGOS_LUAAGENT_H

#include "Goal/Goal.h"
#include "Goal/GoalManager.h"
#include "Goal/LogicManager.h"
#include "Hierarchy/LuaAgentCommandQ.h"

class Player;
class PartyIntelligence;
enum EnchantmentSlot;
enum EquipmentSlots;
enum ShapeshiftForm;

enum class LuaAgentRoles {
	Invalid = 0,
	Mdps = 1,
	Rdps = 2,
	Tank = 3,
	Healer = 4,
	Script = 5,
	Max = 5
};


class LuaAgent
{
	struct FallInfo
	{
		float startX{0.f};
		float startY{0.f};
		float startZ{0.f};
		float lastX{0.f};
		float lastY{0.f};
		float lastZ{0.f};
		float vcos{0.f};
		float vsin{0.f};
		float speedxy{0.f};
		float speedz{0.f};
		float endT{0.f};
		uint32 lastT{0u};
		uint32 beginT{0u};
		bool falling{false};
	} m_fall;

	bool m_queueGoname;
	std::string m_queueGonameName;

	uint32 m_ammo;
	float m_stdThreat;
	ObjectGuid m_healTarget;
	ObjectGuid m_ccTarget;
	ShapeshiftForm m_form;
	int m_desiredLevel;

	ShortTimeTracker m_updateTimer;
	uint32 m_updateInterval;
	ShortTimeTracker m_updateSpeedTimer;
	uint32 m_updateSpeedInterval;

	int m_logic;
	std::string m_spec;
	LuaAgentRoles m_roleId;

	PartyIntelligence* m_party;
	Player* me;
	ObjectGuid m_masterGuid;

	bool m_bCmdQueueMode;
	bool m_bCeaseUpdates;
	bool m_bInitialized;
	bool m_bHasReset;
	bool m_bSimpleChaseMode;

	int m_userDataRef;
	int m_userDataRefPlayer;
	int m_userTblRef;

	Goal m_topGoal;
	GoalManager m_goalManager;
	LogicManager m_logicManager;
	std::vector<std::unique_ptr<AgentCmd>> commands;

	int m_chatMaxQueueSz;
	std::queue<std::string> m_chatMsgQ;

	void Fall();
	void FallEnd(float x, float y, float z);

	void OnChatMessage(WorldPacket& pck);

public:

	static const char* AI_MTNAME;

	LuaAgent(Player* me, ObjectGuid masterGuid, int logicID);
	~LuaAgent() noexcept;

	void Init();
	void Update(uint32 diff);
	void UpdateSession(uint32 diff);
	void Reset(bool droprefs);

	void OnLoggedIn();
	void OnPacketReceived(const WorldPacket& pck);

	std::string GetSpec() { return m_spec; }
	void SetSpec(std::string spec) { m_spec = spec; }
	LuaAgentRoles GetRole() { return m_roleId; }
	void SetRole(LuaAgentRoles role) { m_roleId = role; }

	bool IsInitialized() { return m_bInitialized; }
	bool IsReady();

	Goal* AddTopGoal(int goalId, double life, std::vector<GoalParamP>& goalParams, lua_State* L);
	Goal* GetTopGoal() { return &m_topGoal; };

	void SetRef(int v) { m_userDataRef = v; };
	int GetRef() { return m_userDataRef; };
	int GetUserTblRef() { return m_userTblRef; }
	void CreateUserTbl();
	bool GetCeaseUpdates() { return m_bCeaseUpdates; }
	void SetCeaseUpdates(bool value = true) { m_bCeaseUpdates = value; }
	bool HasReset() { if (m_bHasReset) { m_bHasReset = false; return true; } return false; }
	bool IsSimpleChaseMode() { return m_bSimpleChaseMode; }
	void SetSimpleChaseMode(bool v) { m_bSimpleChaseMode = v; }

	float GetStdThreat() { return m_stdThreat; }
	void SetStdThreat(float value) { m_stdThreat = value; }
	ShapeshiftForm GetForm() { return m_form; }
	void SetForm(ShapeshiftForm value) { m_form = value; }
	int GetDesiredLevel() { return m_desiredLevel; }
	void SetDesiredLevel(int value) { m_desiredLevel = value; }

	ObjectGuid& GetCCTarget() { return m_ccTarget; }
	void SetCCTarget(const ObjectGuid& guid) { m_ccTarget = guid; }
	ObjectGuid& GetHealTarget() { return m_healTarget; }
	void SetHealTarget(const ObjectGuid& guid) { m_healTarget = guid; }

	void GonameCommand(std::string name);
	void GonameCommandQueue(std::string name);

	// equipment

	bool EquipCopyFromMaster();
	void EquipDestroyAll();
	uint32 EquipGetEnchantId(EnchantmentSlot slot, EquipmentSlots itemSlot);
	int32 EquipGetRandomProp(EquipmentSlots itemSlot);
	bool EquipHasItemInSlot(uint32 bag, uint32 slot, bool tradeCheck);
	bool EquipItem(uint32 itemId, uint32 enchantId = 0u, int32 randomPropertyId = 0);
	void EquipPrint();
	void UpdateVisibilityForMaster();
	void SetAmmo(uint32 ammo) { m_ammo = ammo; }
	uint32 GetAmmo() { return m_ammo; }

	// commands

	void CommandsSetQMode(bool set) { m_bCmdQueueMode = set; }
	bool CommandsGetQMode() { return m_bCmdQueueMode; }
	size_t CommandsAdd(AgentCmd* cmd)
	{
		std::unique_ptr<AgentCmd> ptr(cmd);
		if (!m_bCmdQueueMode)
			commands.clear();
		commands.push_back(std::move(ptr));
		return commands.size() - 1;
	}
	void CommandsClear() { commands.clear(); }
	void CommandsPopFront() { if (commands.size()) commands.erase(commands.begin()); }
	AgentCmd* CommandsGetFirst() { return commands.size() > 0 ? commands.front().get() : nullptr; };
	AgentCmdType CommandsGetFirstType() { return commands.size() > 0 ? commands.front()->GetType() : AgentCmdType::None; }
	void CommandsPrint();

	// lua bits
	void CreateUD(lua_State* L);
	void PushUD(lua_State* L);
	void CreatePlayerUD(lua_State* L);
	void PushPlayerUD(lua_State* L);
	void Unref(lua_State* L);
	void UnrefPlayerUD(lua_State* L);
	void UnrefUserTbl(lua_State* L);

	// spells

	uint32 GetSpellChainFirst(uint32 spellID);
	uint32 GetSpellChainPrev(uint32 spellID);
	uint32 GetSpellRank(uint32 spellID);
	uint32 GetSpellMaxRankForMe(uint32 lastSpell);
	uint32 GetSpellMaxRankForLevel(uint32 lastSpell, uint32 level);
	uint32 GetSpellOfRank(uint32 lastSpell, uint32 rank);
	uint32 GetSpellLevel(uint32 spellID);

	void FallBegin(float vcos, float vsin, float speedxy, float speedz, uint32 beginT);
	bool IsFalling() const { return m_fall.falling; }

	const ObjectGuid& GetMasterGuid() { return m_masterGuid; }
	Player* GetPlayer() { return me; }
	PartyIntelligence* GetPartyIntelligence() { return m_party; }
	void SetPartyIntelligence(PartyIntelligence* party) { m_party = party; }

	// chat

	void ChatSendWhisper(Player* recipient, const std::string& text);
	std::queue<std::string>& ChatGetMsgQ() { return m_chatMsgQ; }
	void ChatSendInvToMaster(bool ignoreEquipped);

	// trade

	void TradeInitiate(ObjectGuid guid);
	void TradeBegin();
	void TradeAccept();
	void TradeCancel();
	void TradeAddItem(uint32 tradeSlot, uint32 bag, uint32 slot);

};

#endif
