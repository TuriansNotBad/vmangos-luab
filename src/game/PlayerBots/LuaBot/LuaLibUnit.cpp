
//#include "LuaBot.h"
#include "LuaLibAux.h"
#include "LuaLibUnit.h"
#include "LuaLibWorldObj.h"
#include "PlayerAI.h"
#include "LuaBotAI.h"
#include "LuaUtils.h"
#include "Spell.h"
#include "GridSearchers.h"
#include "BotMovementGenerator.h"
#include <string>

namespace MaNGOS {
	class AnyHostileUnit {
		WorldObject const* i_obj;
		WorldObject const* hostileTo;
		float i_range;
		bool b_3dDist;
	public:
		AnyHostileUnit(WorldObject const* obj, WorldObject const* hostileTo, float range, bool distance_3d = true) : hostileTo(hostileTo), i_obj(obj), i_range(range), b_3dDist(distance_3d) {}
		WorldObject const& GetFocusObject() const { return *i_obj; }
		bool operator()(Unit* u) {
			return u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range, b_3dDist) && u->IsHostileTo(hostileTo);
		}
	};
}

enum PartyBotSpells
{
	PB_SPELL_FOOD = 1131,
	PB_SPELL_DRINK = 1137,
	PB_SPELL_AUTO_SHOT = 75,
	PB_SPELL_SHOOT_WAND = 5019,
	PB_SPELL_HONORLESS_TARGET = 2479,
};


#define PB_UPDATE_INTERVAL 1000
#define PB_MIN_FOLLOW_DIST 3.0f
#define PB_MAX_FOLLOW_DIST 6.0f
#define PB_MIN_FOLLOW_ANGLE 0.0f
#define PB_MAX_FOLLOW_ANGLE 6.0f


void LuaBindsAI::BindUnit(lua_State* L) {
	Unit_CreateMetatable(L);
}


Unit** LuaBindsAI::Unit_GetUnitObject(lua_State* L, int idx) {
	return (Unit**) luaL_checkudwithfield(L, idx, "isUnit");//(Unit**) luaL_checkudata(L, idx, LuaBindsAI::UnitMtName);
}


void LuaBindsAI::Unit_CreateUD(Unit* unit, lua_State* L) {
	// create userdata on top of the stack pointing to a pointer of an AI object
	Unit** unitud = static_cast<Unit**>(lua_newuserdatauv(L, sizeof(Unit*), 0));
	*unitud = unit; // swap the AI object being pointed to to the current instance
	luaL_setmetatable(L, LuaBindsAI::UnitMtName);
}


int LuaBindsAI_Unit_CompareEquality(lua_State* L) {
	WorldObject* obj1 = *LuaBindsAI::WObj_GetWObjObject(L);
	WorldObject* obj2 = *LuaBindsAI::WObj_GetWObjObject(L, 2);
	lua_pushboolean(L, obj1 == obj2);
	return 1;
}
void LuaBindsAI::Unit_CreateMetatable(lua_State* L) {
	luaL_newmetatable(L, LuaBindsAI::UnitMtName);
	lua_pushvalue(L, -1); // copy mt cos setfield pops
	lua_setfield(L, -1, "__index"); // mt.__index = mt
	luaL_setfuncs(L, Unit_BindLib, 0); // copy funcs
	lua_pushcfunction(L, LuaBindsAI_Unit_CompareEquality);
	lua_setfield(L, -2, "__eq");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "isWorldObject");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "isUnit");
	lua_pop(L, 1); // pop mt
}


int LuaBindsAI::Unit_GetAI(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	if (Player* p = unit->ToPlayer())
		if (p->AI() && p->AI()->IsLuaBot())
			if (LuaBotAI* botAI = dynamic_cast<LuaBotAI*>(p->AI())) {
				botAI->PushUD(L);
				return 1;
			}
	return 0;
}


// -----------------------------------------------------------
//                      Combat RELATED
// -----------------------------------------------------------

int LuaBindsAI::Unit_AddAura(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int spellId = luaL_checkinteger(L, 2);
	unit->AddAura(spellId);
	return 0;
}


int LuaBindsAI::Unit_AddThreat(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	float threatN = luaL_checknumber(L, 3);
	unit->AddThreat(target, threatN);
	return 0;
}


int LuaBindsAI::Unit_CalculateSpellEffect(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	int spellID = luaL_checkinteger(L, 3);
	int spellEffectIndex = luaL_checkinteger(L, 4);

	const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellID);
	if (!spell)
		luaL_error(L, "Unit.CalculateSpellEffect: spell %d doesn't exist", spellID);
	if (spellEffectIndex < 0 || spellEffectIndex >= MAX_EFFECT_INDEX)
		luaL_error(L, "Unit.CalculateSpellEffect: spell effect index out of bounds. [0, %d)", MAX_EFFECT_INDEX);

	lua_pushnumber(L, unit->CalculateSpellEffectValue(target, spell, SpellEffectIndex(spellEffectIndex)));
	return 1;
}


int LuaBindsAI::Unit_CastSpell(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	int spellId = luaL_checkinteger(L, 3);
	bool triggered = luaL_checkboolean(L, 4);
	lua_pushinteger(L, unit->CastSpell(target, spellId, triggered));
	return 1;
}


int LuaBindsAI::Unit_ClearTarget(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	unit->ClearTarget();
	return 0;
}


int LuaBindsAI::Unit_GetAttackersTbl(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_newtable(L);
	int tblIdx = 1;
	for (const auto pAttacker : unit->GetAttackers())
		if (IsValidHostileTarget(unit, pAttacker)) {
			Unit_CreateUD(pAttacker, L); // pushes pAttacker userdata on top of stack
			lua_seti(L, -2, tblIdx); // stack[-2][tblIdx] = stack[-1], pops pAttacker
			tblIdx++;
		}
	return 1;
}


int LuaBindsAI::Unit_GetAttackRange(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* toAttack = *Unit_GetUnitObject(L, 2);
	if (unit->IsCreature())
		lua_pushnumber(L, unit->ToCreature()->GetAttackDistance(toAttack));
	else
		lua_pushnumber(L, 0);
	return 1;
}


int LuaBindsAI::Unit_GetAttackTimer(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int type = luaL_checkinteger(L, 2);
	if (type < WeaponAttackType::BASE_ATTACK || type > WeaponAttackType::RANGED_ATTACK)
		luaL_error(L, "Unit.GetAttackTimer invalid attack type %d, allowed values [%d, %d]", type, BASE_ATTACK, RANGED_ATTACK);
	lua_pushinteger(L, unit->GetAttackTimer((WeaponAttackType) type));
	return 1;
}


int LuaBindsAI::Unit_GetAuraStacks(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	if (SpellAuraHolder* sah = unit->GetSpellAuraHolder(spellId))
		lua_pushinteger(L, sah->GetStackAmount());
	else
		lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_GetAuraDuration(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	if (SpellAuraHolder* sah = unit->GetSpellAuraHolder(spellId))
		lua_pushinteger(L, sah->GetAuraDuration());
	else
		lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_GetCombatDistance(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* to = *Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, unit->GetCombatDistance(to));
	return 1;
}


int LuaBindsAI::Unit_GetCombatReach(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetCombatReach());
	return 1;
}


int LuaBindsAI::Unit_GetEnemyCountInRadiusAround(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* to = *Unit_GetUnitObject(L, 2);
	float r = luaL_checknumber(L, 3);
	lua_pushinteger(L, unit->GetEnemyCountInRadiusAround(to, r));
	return 1;
}


int LuaBindsAI::Unit_GetHealth(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetHealth());
	return 1;
}


int LuaBindsAI::Unit_GetMaxHealth(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetMaxHealth());
	return 1;
}


int LuaBindsAI::Unit_GetHealthPercent(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetHealthPercent());
	return 1;
}


int LuaBindsAI::Unit_GetMeleeRange(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	float range = unit->GetCombatReach() + target->GetCombatReach() + 4.0f / 3.0f;
	lua_pushnumber(L, std::max(range, NOMINAL_MELEE_RANGE));
	return 1;
}


int LuaBindsAI::Unit_GetMeleeReach(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetMeleeReach());
	return 1;
}


int LuaBindsAI::Unit_GetPet(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushunitornil(L, unit->GetPet());
	return 1;
}


int LuaBindsAI::Unit_GetPower(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int powerId = luaL_checkinteger(L, 2);
	if (powerId < POWER_MANA || powerId > POWER_HAPPINESS)
		luaL_error(L, "Unit.GetPower. Invalid power id, expected value in range [0, 4], got %d", powerId);
	lua_pushinteger(L, unit->GetPower((Powers) powerId));
	return 1;
}


int LuaBindsAI::Unit_GetMaxPower(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int powerId = luaL_checkinteger(L, 2);
	if (powerId < POWER_MANA || powerId > POWER_HAPPINESS)
		luaL_error(L, "Unit.GetMaxPower. Invalid power id, expected value in range [0, 4], got %d", powerId);
	lua_pushinteger(L, unit->GetMaxPower((Powers) powerId));
	return 1;
}


int LuaBindsAI::Unit_GetPowerPercent(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int powerId = luaL_checkinteger(L, 2);
	if (powerId < POWER_MANA || powerId > POWER_HAPPINESS)
		luaL_error(L, "Unit.GetPowerPercent. Invalid power id, expected value in range [0, 4], got %d", powerId);
	lua_pushnumber(L, unit->GetPowerPercent((Powers) powerId));
	return 1;
}


int LuaBindsAI::Unit_GetSpellCD(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	TimePoint t;
	bool perm;

	return 0;
}


int LuaBindsAI::Unit_GetSpellCost(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
	if (spell) {
		uint32 const powerCost = Spell::CalculatePowerCost(spell, unit);
		lua_pushinteger(L, powerCost);
	}
	else {
		luaL_error(L, "Unit.GetSpellCost: spell %d doesn't exist", spellId);
		lua_pushnil(L); // technically never reached
	}
	return 1;
}


int LuaBindsAI::Unit_GetShapeshiftForm(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetShapeshiftForm());
	return 1;
}


int LuaBindsAI::Unit_GetThreat(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* victim = *Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, unit->GetThreatManager().getThreat(victim));
	return 1;
}


int LuaBindsAI::Unit_GetThreatTbl(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_newtable(L);
	int tblIdx = 1;
	for (auto v : unit->GetThreatManager().getThreatList()) {
		if (Unit* hostile = v->getTarget()) {
			Unit_CreateUD(hostile, L);
			lua_seti(L, -2, tblIdx); // stack[1][tblIdx] = stack[-1], pops unit
			tblIdx++;
		}
	}
	return 1;
}


int LuaBindsAI::Unit_HasTotem(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int slot = luaL_checkinteger(L, 2);
	if (slot < TotemSlot::TOTEM_SLOT_FIRE || slot > TotemSlot::TOTEM_SLOT_AIR)
		luaL_error(L, "Unit.HasTotem: totem slot out of bounds. [0, %d]", TOTEM_SLOT_AIR);
	lua_pushboolean(L, unit->GetTotem(TotemSlot(slot)) != nullptr);
	return 1;
}


int LuaBindsAI::Unit_GetVictim(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushunitornil(L, unit->GetVictim());
	return 1;
}


int LuaBindsAI::Unit_GetVictimsInRange(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* hostileTo = *Unit_GetUnitObject(L, 2);
	float range = luaL_checknumber(L, 3);
	lua_newtable(L);
	std::list<Unit*> lList;

	CellPair p(MaNGOS::ComputeCellPair(unit->GetPositionX(), unit->GetPositionY()));
	Cell cell(p);
	cell.SetNoCreate();

	MaNGOS::AnyHostileUnit check(unit, hostileTo, range);
	MaNGOS::UnitListSearcher<MaNGOS::AnyHostileUnit> searcher(lList, check);

	TypeContainerVisitor<MaNGOS::UnitListSearcher<MaNGOS::AnyHostileUnit>, WorldTypeMapContainer > world_unit_searcher(searcher);
	TypeContainerVisitor<MaNGOS::UnitListSearcher<MaNGOS::AnyHostileUnit>, GridTypeMapContainer >  grid_unit_searcher(searcher);

	cell.Visit(p, world_unit_searcher, *(unit->GetMap()), *unit, range);
	cell.Visit(p, grid_unit_searcher, *(unit->GetMap()), *unit, range);

	int i = 1;
	for (auto unit : lList) {
		Unit_CreateUD(unit, L);
		lua_seti(L, -2, i);
		i++;
	}
	return 1;
}


int LuaBindsAI::Unit_GetVictimNearest(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float dist = luaL_checknumber(L, 2);
	lua_pushunitornil(L, unit->SelectNearestTarget(dist));
	return 1;
}


int LuaBindsAI::Unit_GetCurrentSpellId(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int spellType = luaL_checkinteger(L, 2);
	if (spellType < CURRENT_MELEE_SPELL || spellType > CURRENT_CHANNELED_SPELL)
		luaL_error(L, "Unit.GetCurrentSpellId. Invalid spell type id, expected value in range [0, 3], got %d", spellType);
	if (Spell* curSpell = unit->GetCurrentSpell((CurrentSpellTypes) spellType))
		lua_pushinteger(L, curSpell->m_spellInfo->Id);
	else
		lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_HasAura(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int spellId = luaL_checkinteger(L, 2);
	lua_pushboolean(L, unit->HasAura(spellId));
	return 1;
}


int LuaBindsAI::Unit_HasAuraType(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int auraId = luaL_checkinteger(L, 2);
	if (auraId < SPELL_AURA_NONE || auraId >= TOTAL_AURAS)
		luaL_error(L, "Unit.RemoveSpellsCausingAura. Invalid aura type id, expected value in range [%d, %d], got %d", SPELL_AURA_NONE, TOTAL_AURAS - 1, auraId);
	lua_pushboolean(L, unit->HasAuraType((AuraType) auraId));
	return 1;
}


int LuaBindsAI::Unit_HasAuraBy(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* by = *Unit_GetUnitObject(L, 2);
	int auraId = luaL_checkinteger(L, 3);
	if (auraId < SPELL_AURA_NONE || auraId >= TOTAL_AURAS)
		luaL_error(L, "Unit.HasAuraBy. Invalid aura type id, expected value in range [%d, %d], got %d", SPELL_AURA_NONE, TOTAL_AURAS - 1, auraId);
	uint32 spellId = luaL_checkinteger(L, 4);
	lua_pushboolean(L, unit->HasAuraByCaster((AuraType) auraId, spellId, by->GetObjectGuid()));
	return 1;
}


int LuaBindsAI::Unit_HasObjInArc(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	WorldObject* obj = *WObj_GetWObjObject(L, 2);
	float arc = luaL_checknumber(L, 3);
	float offset = 0.0F;
	if (lua_gettop(L) == 4)
		luaL_checknumber(L, 4);
	lua_pushboolean(L, unit->HasInArc(obj, arc, offset));
	return 1;
}


int LuaBindsAI::Unit_HasPosInArc(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float arc = luaL_checknumber(L, 2);
	float x = luaL_checknumber(L, 3);
	float y = luaL_checknumber(L, 4);
	lua_pushboolean(L, unit->HasInArc(arc, x, y));
	return 1;
}


int LuaBindsAI::Unit_InterruptSpell(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int spellType = luaL_checkinteger(L, 2);
	bool withDelayed = luaL_checkboolean(L, 3);
	if (spellType < CURRENT_MELEE_SPELL || spellType > CURRENT_CHANNELED_SPELL)
		luaL_error(L, "Unit.InterruptSpell. Invalid spell type id, expected value in range [0, 3], got %d", spellType);
	unit->InterruptSpell((CurrentSpellTypes) spellType, withDelayed);
	if (spellType == CURRENT_CHANNELED_SPELL)
		unit->FinishSpell(CURRENT_CHANNELED_SPELL);
	return 0;
}


int LuaBindsAI::Unit_IsNonMeleeSpellCasted(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	bool withDelayed = luaL_checkboolean(L, 2);
	bool skipChanneled = luaL_checkboolean(L, 3);
	bool skipAutorepeat = luaL_checkboolean(L, 4);
	lua_pushboolean(L, unit->IsNonMeleeSpellCasted(withDelayed, skipChanneled, skipAutorepeat));
	return 1;
}


int LuaBindsAI::Unit_IsCastingInterruptableSpell(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	if (!unit->IsAlive()) {
		lua_pushboolean(L, false);
		return 1;
	}

	// TODO: not all spells that used this effect apply cooldown at school spells
	// also exist case: apply cooldown to interrupted cast only and to all spells
	for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
	{
		if (Spell* spell = unit->GetCurrentSpell(CurrentSpellTypes(i)))
		{
			// Nostalrius: fix CS of instant spells (with CC Delay)
			if (i != CURRENT_CHANNELED_SPELL && !spell->GetCastTime())
				continue;

			SpellEntry const* curSpellInfo = spell->m_spellInfo;
			// check if we can interrupt spell
			if ((spell->getState() == SPELL_STATE_CASTING
				|| (spell->getState() == SPELL_STATE_PREPARING && spell->GetCastTime() > 0.0f))
				&& curSpellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE
				&& ((i == CURRENT_GENERIC_SPELL && curSpellInfo->HasSpellInterruptFlag(SPELL_INTERRUPT_FLAG_DAMAGE_PUSHBACK))
					|| (i == CURRENT_CHANNELED_SPELL && curSpellInfo->HasChannelInterruptFlag(AURA_INTERRUPT_ACTION_CANCELS))))
			{
				lua_pushboolean(L, true);
				return 1;
			}
		}
	}
	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_IsInCombat(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsInCombat());
	return 1;
}


int LuaBindsAI::Unit_IsTargetInRangeOfSpell(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* pTarget = *Unit_GetUnitObject(L, 2);
	uint32 spellId = luaL_checkinteger(L, 3);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
		lua_pushboolean(L, spell->IsTargetInRange(unit, pTarget));
	else
		luaL_error(L, "Unit.IsTargetInRangeOfSpell spell doesn't exist. Id = %d", spellId);
	return 1;
}


int LuaBindsAI::Unit_IsValidHostileTarget(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, IsValidHostileTarget(unit, target));
	return 1;
}


/// <summary>
/// True if unit is a player.
/// </summary>
/// <param name="unit userdata">- Unit</param>
/// <returns>bool</returns>
int LuaBindsAI::Unit_IsPlayer(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsPlayer());
	return 1;
}


/// <summary>
/// If unit has pet sends attack command to pet ai
/// </summary>
/// <param name="unit userdata">- Unit</param>
/// <param name="unit userdata">- Target to attack</param>
int LuaBindsAI::Unit_PetAttack(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* pVictim = *Unit_GetUnitObject(L, 2);
	if (Pet* pPet = unit->GetPet())
		if (pPet->IsAlive() && (!pPet->GetVictim() || pPet->GetVictim()->GetObjectGuid() != pVictim->GetObjectGuid())) {
			pPet->GetCharmInfo()->SetIsCommandAttack(true);
			pPet->AI()->AttackStart(pVictim);
		}
	return 0;
}


/// <summary>
/// If unit has pet tries to make it cast an ability
/// </summary>
/// <param name="unit userdata">- Pet owner</param>
/// <param name="unit userdata">- Target of spell</param>
/// <param name="int">- Spell ID</param>
/// <returns>int - Spell cast result</returns>
int LuaBindsAI::Unit_PetCast(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* pVictim = *Unit_GetUnitObject(L, 2);
	uint32 spellId = luaL_checkinteger(L, 3);
	if (Pet* pPet = unit->GetPet())
		if (CreatureAI* pAI = pPet->AI()) {
			if (pPet->IsAlive()) {
				lua_pushinteger(L, pAI->DoCastSpellIfCan(pVictim, spellId));
				return 1;
			}
		}
	lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_PetAutocast(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	bool toggle = luaL_checkboolean(L, 3);

	if (!Spells::IsAutocastable(spellId))
		return 0;

	if (unit->IsPlayer() && unit->ToPlayer()) {

		Player* pPlayer = unit->ToPlayer();

		if (Pet* pPet = pPlayer->GetPet()) {

			uint8 state = toggle ? uint8(1) : uint8(0);

			CharmInfo* charmInfo = pPet->GetCharmInfo();
			if (!charmInfo)
				return 0;

			if (pPet->IsCharmed())
				// state can be used as boolean
				pPet->GetCharmInfo()->ToggleCreatureAutocast(spellId, state);
			else
				((Pet*) pPet)->ToggleAutocast(spellId, state);

			charmInfo->SetSpellAutocast(spellId, state);
		}

	}

	return 0;
}


/// <summary>
/// If unit has pet sets that pet's caster chase distance
/// </summary>
/// <param name="unit userdata">- Unit</param>
/// <param name="dist number">- Chase distance</param>
int LuaBindsAI::Unit_PetCasterChaseDist(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float dist = luaL_checknumber(L, 2);
	if (Pet* pPet = unit->GetPet())
		pPet->SetCasterChaseDistance(dist);
	return 0;
}


int LuaBindsAI::Unit_RemoveAura(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	unit->RemoveAurasDueToSpell(spellId);
	return 0;
}


int LuaBindsAI::Unit_RemoveAurasByCasterSpell(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* by = *Unit_GetUnitObject(L, 2);
	uint32 spellId = luaL_checkinteger(L, 2);
	unit->RemoveAurasByCasterSpell(spellId, by->GetObjectGuid());
	return 0;
}


int LuaBindsAI::Unit_RemoveAuraByCancel(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	unit->RemoveAurasDueToSpellByCancel(spellId);
	return 0;
}


int LuaBindsAI::Unit_RemoveSpellCooldown(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 spellId = luaL_checkinteger(L, 2);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
		unit->RemoveSpellCooldown(spellId);
	else
		luaL_error(L, "Unit.RemoveSpellCooldown spell doesn't exist. Id = %d", spellId);
	return 0;
}


int LuaBindsAI::Unit_RemoveSpellsCausingAura(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	int auraId = luaL_checkinteger(L, 2);
	if (auraId < SPELL_AURA_NONE || auraId >= TOTAL_AURAS)
		luaL_error(L, "Unit.RemoveSpellsCausingAura. Invalid aura type id, expected value in range [%d, %d], got %d", SPELL_AURA_NONE, TOTAL_AURAS-1, auraId);
	unit->RemoveSpellsCausingAura((AuraType) auraId);
	return 0;
}


int LuaBindsAI::Unit_SetHealth(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 health = luaL_checkinteger(L, 2);
	unit->SetHealth(health);
	return 0;
}


int LuaBindsAI::Unit_SetMaxHealth(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 health = luaL_checkinteger(L, 2);
	unit->SetMaxHealth(health);
	return 0;
}


int LuaBindsAI::Unit_SetHealthPercent(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float health = luaL_checknumber(L, 2);
	unit->SetHealthPercent(health);
	return 0;
}


int LuaBindsAI::Unit_SetMaxPower(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 power = luaL_checkinteger(L, 2);
	int powerId = luaL_checkinteger(L, 3);
	if (powerId < POWER_MANA || powerId > POWER_HAPPINESS)
		luaL_error(L, "Unit.SetMaxPower. Invalid power id, expected value in range [0, 4], got %d", powerId);
	unit->SetMaxPower((Powers) powerId, power);
	return 0;
}


int LuaBindsAI::Unit_SetPower(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 power = luaL_checkinteger(L, 2);
	int powerId = luaL_checkinteger(L, 3);
	if (powerId < POWER_MANA || powerId > POWER_HAPPINESS)
		luaL_error(L, "Unit.SetPower. Invalid power id, expected value in range [0, 4], got %d", powerId);
	unit->SetPower((Powers) powerId, power);
	return 0;
}


int LuaBindsAI::Unit_SetPowerPercent(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float power = luaL_checknumber(L, 2);
	int powerId = luaL_checkinteger(L, 3);
	if (powerId < POWER_MANA || powerId > POWER_HAPPINESS)
		luaL_error(L, "Unit.SetPowerPercent. Invalid power id, expected value in range [0, 4], got %d", powerId);
	unit->SetPowerPercent((Powers)powerId, power);
	return 0;
}

// -----------------------------------------------------------
//                      CASTER CHASE
// -----------------------------------------------------------


int LuaBindsAI::Unit_HasDistanceCasterMovement(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->HasDistanceCasterMovement());
	return 1;
}


int LuaBindsAI::Unit_SetCasterChaseDistance(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float dist = luaL_checknumber(L, 2);
	unit->SetCasterChaseDistance(dist);
	return 0;
}


// -----------------------------------------------------------
//                      GEN INFO
// -----------------------------------------------------------


int LuaBindsAI::Unit_ClearUnitState(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 state = luaL_checkinteger(L, 2);
	unit->ClearUnitState(state);
	return 0;
}


int LuaBindsAI::Unit_GetClass(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetClass());
	return 1;
}


int LuaBindsAI::Unit_GetDisplayId(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetDisplayId());
	return 1;
}


int LuaBindsAI::Unit_GetEntry(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetEntry());
	return 1;
}


int LuaBindsAI::Unit_GetLevel(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetLevel());
	return 1;
}


int LuaBindsAI::Unit_GetNativeDisplayId(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetNativeDisplayId());
	return 1;
}


int LuaBindsAI::Unit_GetName(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushstring(L, unit->GetName());
	return 1;
}


int LuaBindsAI::Unit_GetObjectGuid(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushstring(L, std::to_string(unit->GetObjectGuid().GetRawValue()).c_str());
	//printf("%s\n", std::to_string(unit->GetObjectGuid().GetRawValue()).c_str());
	return 1;
}


int LuaBindsAI::Unit_GetObjectBoundingRadius(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetObjectBoundingRadius());
	return 1;
}


int LuaBindsAI::Unit_GetPowerType(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetPowerType());
	return 1;
}


int LuaBindsAI::Unit_GetRace(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetRace());
	return 1;
}


int LuaBindsAI::Unit_GetRank(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	if (unit->IsCreature())
		lua_pushinteger(L, unit->ToCreature()->GetCreatureInfo()->rank);
	else
		lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_GetTargetGuid(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushstring(L, std::to_string(unit->GetTargetGuid().GetRawValue()).c_str());
	return 1;
}


int LuaBindsAI::Unit_HasUnitState(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 state = luaL_checkinteger(L, 2);
	lua_pushboolean(L, unit->HasUnitState(state));
	return 1;
}


int LuaBindsAI::Unit_IsInDisallowedMountForm(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsInDisallowedMountForm());
	return 1;
}


int LuaBindsAI::Unit_IsInDungeon(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->GetMap()->IsDungeon());
	return 1;
}


// -----------------------------------------------------------
//                      Movement RELATED
// -----------------------------------------------------------


int LuaBindsAI::Unit_DoesPathExist(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = luaL_checknumber(L, 4);

	GenericTransport* transport = unit->GetTransport();
	PathFinder path(unit);
	path.SetTransport(transport);
	bool result = path.calculate(x, y, z, false);
	if (result && (path.getPathType() & PATHFIND_NORMAL)) {
		lua_pushboolean(L, true);
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_DoesPathExistPos(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = luaL_checknumber(L, 4);
	float x1 = luaL_checknumber(L, 5);
	float y1 = luaL_checknumber(L, 6);
	float z1 = luaL_checknumber(L, 7);

	GenericTransport* transport = unit->GetTransport();
	PathFinder path(unit);
	path.SetTransport(transport);
	bool result = path.calculate(G3D::Vector3(x, y, z), G3D::Vector3(x1, y1, z1), false);
	if (result && (path.getPathType() & PATHFIND_NORMAL)) {
		lua_pushboolean(L, true);
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_GetCurrentMovementGeneratorType(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetMotionMaster()->GetCurrentMovementGeneratorType());
	return 1;
}


int LuaBindsAI::Unit_GetSpeedRate(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetSpeedRate(MOVE_RUN));
	return 1;
}


int LuaBindsAI::Unit_GetStandState(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetStandState());
	return 1;
}


int LuaBindsAI::Unit_IsMounted(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsMounted());
	return 1;
}


int LuaBindsAI::Unit_IsMoving(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsMoving());
	return 1;
}


int LuaBindsAI::Unit_IsStopped(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsStopped());
	return 1;
}


int LuaBindsAI::Unit_MonsterMove(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = luaL_checknumber(L, 4);
	float o = luaL_checknumber(L, 5);
	float speed = luaL_checknumber(L, 6);
	uint32 options = luaL_checkinteger(L, 7);
	unit->MonsterMoveWithSpeed(x, y, z, o, speed, options);
	return 0;
}


int LuaBindsAI::Unit_MotionMasterClear(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	bool reset = luaL_checkboolean(L, 2);
	bool all = luaL_checkboolean(L, 3);
	unit->GetMotionMaster()->Clear(reset, all);
	//unit->GetMotionMaster()->MoveIdle();
	return 0;
}


int LuaBindsAI::Unit_MoveChase(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	float dist = luaL_checknumber(L, 3);
	float angle = luaL_checknumber(L, 4);
	float angleT = luaL_checknumber(L, 5);
	unit->GetMotionMaster()->MoveBotChase(target, dist, angle, angleT);
	return 0;
}


int LuaBindsAI::Unit_MoveFollow(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* target = *Unit_GetUnitObject(L, 2);
	float dist = luaL_checknumber(L, 3);
	float angle = luaL_checknumber(L, 4);
	unit->GetMotionMaster()->MoveBotFollow(target, dist, angle);
	return 0;
}


int LuaBindsAI::Unit_MoveIdle(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	unit->GetMotionMaster()->MoveIdle();
	return 0;
}


int LuaBindsAI::Unit_MovePoint(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint32 data = luaL_checkinteger(L, 2);
	float x = luaL_checknumber(L, 3);
	float y = luaL_checknumber(L, 4);
	float z = luaL_checknumber(L, 5);

	uint32 options = MOVE_PATHFINDING | MOVE_FORCE_DESTINATION;
	if (lua_gettop(L) > 5)
		options = options | luaL_checkinteger(L, 6);

	float speed = 0.0F;
	if (lua_gettop(L) > 6)
		speed = luaL_checknumber(L, 7);

	float finalOrientation = -10.0F;
	if (lua_gettop(L) > 7)
		finalOrientation = luaL_checknumber(L, 8);

	if (unit->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
	{
		unit->GetMotionMaster()->MoveBotPoint(data, x, y, z, options, speed, finalOrientation);
		return 0;
	}
	else if (unit->IsPlayer())
	{
		if (auto cgenerator = dynamic_cast<const BotPointMovementGenerator<Player>*>(unit->GetMotionMaster()->GetCurrent()))
		{
			auto generator = const_cast<BotPointMovementGenerator<Player>*>(cgenerator);
			if (options == generator->GetOptions())
			{
				generator->ChangeDestination(*unit->ToPlayer(), x, y, z);
				return 0;
			}
		}
	}
	unit->GetMotionMaster()->MoveBotPoint(data, x, y, z, options, speed, finalOrientation);
	return 0;
}


int LuaBindsAI::Unit_SetStandState(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	uint8 state = luaL_checkinteger(L, 2);
	unit->SetStandState(state);
	return 0;
}


int LuaBindsAI::Unit_StopMoving(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	unit->StopMoving();
	return 0;
}


int LuaBindsAI::Unit_UpdateSpeed(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float speed = luaL_checknumber(L, 2);
	unit->UpdateSpeed(MOVE_RUN, false, speed);
	return 0;
}


// -----------------------------------------------------------
//                      POSITION RELATED
// -----------------------------------------------------------

int LuaBindsAI::Unit_GetAngle(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* to = *Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, unit->GetAngle(to));
	return 1;
}


int LuaBindsAI::Unit_GetAbsoluteAngle(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* to = *Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, GetAbsoluteAngle(to->GetPositionX(), to->GetPositionY(), unit->GetPositionX(), unit->GetPositionY()));
	return 1;
}


int LuaBindsAI::Unit_GetDistance(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	WorldObject* to = *WObj_GetWObjObject(L, 2);
	lua_pushnumber(L, unit->GetDistance(to));
	return 1;
}


int LuaBindsAI::Unit_GetDistanceToPos(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	//WorldObject* to = *WObj_GetWObjObject(L, 2);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = luaL_checknumber(L, 4);
	lua_pushnumber(L, unit->GetDistance(x, y, z));
	return 1;
}


int LuaBindsAI::Unit_GetExactDist(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	WorldObject* to = *WObj_GetWObjObject(L, 2);
	lua_pushnumber(L, unit->GetDistance2d(to, SizeFactor::None));
	return 1;
}


int LuaBindsAI::Unit_GetForwardVector(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float ori = unit->GetOrientation();
	lua_newtable(L);
	lua_pushnumber(L, std::cos(ori));
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, std::sin(ori));
	lua_setfield(L, -2, "y");
	return 1;
}


int LuaBindsAI::Unit_GetGroundHeight(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = unit->GetPositionZ();
	unit->UpdateGroundPositionZ(x, y, z);
	lua_pushnumber(L, z);
	return 1;
}


int LuaBindsAI::Unit_GetMapId(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetMapId());
	return 1;
}


int LuaBindsAI::Unit_GetNearPoint(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* searcher = *Unit_GetUnitObject(L, 2);
	float bounding_radius = luaL_checknumber(L, 3);
	float distance2d = luaL_checknumber(L, 4);
	float absAngle = luaL_checknumber(L, 5);

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	unit->GetNearPoint(searcher, x, y, z, bounding_radius, distance2d, absAngle);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_pushnumber(L, z);

	return 3;

}


/// <summary>
/// Caller's GetObjectBountingRadius() is added to distance in GetNearPoint2D.
/// </summary>
int LuaBindsAI::Unit_GetNearPoint2D(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* searcher = *Unit_GetUnitObject(L, 2);
	float bounding_radius = luaL_checknumber(L, 3);
	float distance2d = luaL_checknumber(L, 4);
	float absAngle = luaL_checknumber(L, 5);

	float x = 0.0f;
	float y = 0.0f;
	float z = unit->GetPositionZ();
	unit->GetNearPoint2D(x, y, bounding_radius + distance2d, absAngle);
	searcher->UpdateAllowedPositionZ(x, y, z);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_pushnumber(L, z);

	return 3;

}


int LuaBindsAI::Unit_GetNearPointAroundPosition(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = luaL_checknumber(L, 4);
	float bounding_radius = luaL_checknumber(L, 5);
	float distance2d = luaL_checknumber(L, 6);
	float absAngle = luaL_checknumber(L, 7);
	Unit* searcher = nullptr;
	if (lua_gettop(L) > 7)
		searcher = *Unit_GetUnitObject(L, 8);
	unit->GetNearPointAroundPosition(searcher, x, y, z, bounding_radius, distance2d, absAngle);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_pushnumber(L, z);
	return 3;
}


int LuaBindsAI::Unit_GetOrientation(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetOrientation());
	return 1;
}


int LuaBindsAI::Unit_GetPosition(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	//lua_newtable(L);
	lua_pushnumber(L, unit->GetPositionX());
	//lua_setfield(L, -2, "x");
	lua_pushnumber(L, unit->GetPositionY());
	//lua_setfield(L, -2, "y");
	lua_pushnumber(L, unit->GetPositionZ());
	//lua_setfield(L, -2, "z");
	return 3;
}


int LuaBindsAI::Unit_GetRelativeAngle(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	Unit* to = *Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, unit->GetAngle(to) - unit->GetOrientation());
	return 1;
}


int LuaBindsAI::Unit_IsInWorld(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsInWorld());
	return 1;
}


int LuaBindsAI::Unit_IsWithinLOS(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = luaL_checknumber(L, 4);
	lua_pushboolean(L, unit->IsWithinLOS(x, y, z));
	return 1;
}


int LuaBindsAI::Unit_IsWithinLOSInMap(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	WorldObject* to = *WObj_GetWObjObject(L, 2);
	bool checkDynLos = true;
	if (lua_gettop(L) == 3)
		checkDynLos = luaL_checkboolean(L, 3);
	lua_pushboolean(L, unit->IsWithinLOSInMap(to, checkDynLos));
	return 1;
}


int LuaBindsAI::Unit_SetFacingTo(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float ori = luaL_checknumber(L, 2);
	unit->SetFacingTo(ori);
	return 0;
}


int LuaBindsAI::Unit_SetFacingToObject(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	WorldObject* pObject = *WObj_GetWObjObject(L, 2);
	unit->SetFacingToObject(pObject);
	return 0;
}


int LuaBindsAI::Unit_SetOrientation(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float ori = luaL_checknumber(L, 2);
	unit->SetOrientation(ori);
	return 0;
}


int LuaBindsAI::Unit_ToAbsoluteAngle(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	float angle = luaL_checknumber(L, 2);
	lua_pushnumber(L, NormalizeOrientation(angle + unit->GetOrientation()));
	return 1;
}


int LuaBindsAI::Unit_GetZoneId(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetZoneId());
	return 1;
}


// -----------------------------------------------------------
//                    DEATH RELATED
// -----------------------------------------------------------


int LuaBindsAI::Unit_GetDeathState(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetDeathState());
	return 1;
}


int LuaBindsAI::Unit_IsAlive(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsAlive());
	return 1;
}


int LuaBindsAI::Unit_IsDead(lua_State* L) {
	Unit* unit = *Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsDead());
	return 1;
}


// -----------------------------------------------------------
//                    END DEATH RELATED
// -----------------------------------------------------------


int LuaBindsAI::Unit_Print(lua_State* L) {
	printf("Unit userdata test\n");
	return 0;
}


// -----------------------------------------------------------
//                    END DEATH RELATED
// -----------------------------------------------------------

bool Unit::HasAuraByCaster(AuraType auraType, uint32 spellId, ObjectGuid casterGuid) const {
	for (const auto& iter : m_modAuras[auraType])
		if (iter->GetCasterGuid() == casterGuid && iter->GetId() == spellId)
			return true;
	return false;

}




