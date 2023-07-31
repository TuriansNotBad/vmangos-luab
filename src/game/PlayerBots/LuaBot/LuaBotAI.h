
#ifndef MANGOS_LuaBot_H
#define MANGOS_LuaBot_H

#include "lua.hpp"
#include "PartyBotAI.h"
#include "GoalManager.h"
#include "LogicManager.h"

enum LuaBotRoles {
    LBROLE_INVALID = 0,
    LBROLE_MDPS = 1,
    LBROLE_RDPS = 2,
    LBROLE_TANK = 3,
    LBROLE_HEALER = 4,
    LBROLE_MAXROLE = 4,
};

class LuaBotAI : public PartyBotAI {

    bool _queueGoname;
    std::string _queueGonameName;

public:

    static const char* AI_MTNAME;
    int userDataRef;
    int userDataRefPlayer;
    int userTblRef;
    bool ceaseUpdates;
    lua_State* L;
    LuaBotRoles m_roleID;
    int logicId;
    std::string m_spec;
    Goal topGoal;
    GoalManager goalManager;
    LogicManager logicManager;

    int eventRequest;

    LuaBotAI(Player* pLeader, Player* pClone, uint8 race, uint8 class_, uint8 gender, uint8 level, uint32 mapId, uint32 instanceId, float x, float y, float z, float o, int logicId, std::string spec = "");

    ~LuaBotAI() noexcept {
        // clean up ud refs ? more important when using shared state
        Unref(L);
        UnrefPlayerUD(L);
        UnrefUserTbl(L);
        //lua_close(L); we don't own it anymore
    }

    void Initialize();
    void UpdateAI(uint32 const diff);
    void UpdateAILua(uint32 const diff);
    void Reset(bool droprefs);

    const std::string& GetSpec() { return m_spec; }
    void SetSpec(const std::string& spec) { m_spec = spec; }
    LuaBotRoles GetRole() { return m_roleID; }
    void SetRole(LuaBotRoles role) { m_roleID = role; }

    bool IsInitialized() { return m_initialized; }
    bool IsReady();

    void AttackAutoshot(Unit* pVictim, float chaseDist);
    void AttackStopAutoshot();
    bool CanCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry, bool bAura = true, bool bGCD = true) const;
    bool DrinkAndEat(float healthPer, float manaPer);
    void SummonPetIfNeeded(uint32 petId);
    void GonameCommand(std::string name);
    void GonameCommandQueue(std::string name);
    void Mount(bool toMount, uint32 mountSpell);
    Player* GetPartyLeader() const;
    int HasEventRequest() { return eventRequest != -1; }
    int GetEventRequest() {
        int eventNo = eventRequest;
        eventRequest = -1;
        return eventNo;
    }
    // If AI script cares about events it can later retrieve this with ai:GetEventRequest and do whatever
    // Event is reset when its retrieved
    void SetEventRequest(int eventNo) { eventRequest = eventNo; }

    // spells

    uint32 GetSpellChainFirst(uint32 spellID);
    uint32 GetSpellChainPrev(uint32 spellID);
    uint32 GetSpellRank(uint32 spellID);
    uint32 GetSpellMaxRankForMe(uint32 lastSpell);
    uint32 GetSpellMaxRankForLevel(uint32 lastSpell, uint32 level);
    uint32 GetSpellOfRank(uint32 lastSpell, uint32 rank);
    std::string GetSpellName(uint32 spellID);
    uint32 GetSpellLevel(uint32 spellID);
    
    // equip

    void AddItemToInventory(uint32 itemId, uint32 count = 1, int32 randomPropertyId = -1);
    uint32 EquipFindItemByName(const std::string& name);
    void EquipItem(uint32 itemID, int32 randomPropertyId = -1);
    void EquipDestroyAll();
    void EquipEnchant(uint32 enchantID, EnchantmentSlot slot, EquipmentSlots itemSlot, int duration, int charges);
    uint32 EquipGetEnchantId(EnchantmentSlot slot, EquipmentSlots itemSlot);
    int32 EquipGetRandomProp(EquipmentSlots itemSlot);
    bool EquipCopyFromMaster();


    Goal* AddTopGoal(int goalId, double life, std::vector<GoalParamP>& goalParams, lua_State* L);
    Goal* GetTopGoal() { return &topGoal; };

    void CeaseUpdates(bool cease = true) { ceaseUpdates = cease; }
    void SetRef(int v) { userDataRef = v; };
    int GetRef() { return userDataRef; };
    int GetUserTblRef() { return userTblRef; }
    void CreateUserTbl() {
        if (userTblRef == LUA_NOREF) {
            lua_newtable(L);
            userTblRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }
    }

    // lua bits
    bool IsLuaBot() override { return true; }
    void CreateUD( lua_State* L );
    void PushUD( lua_State* L );
    void CreatePlayerUD(lua_State* L);
    void PushPlayerUD(lua_State* L);
    void Unref( lua_State* L );
    void UnrefPlayerUD(lua_State* L);
    void UnrefUserTbl(lua_State* L);
    void Print();

};

#endif
