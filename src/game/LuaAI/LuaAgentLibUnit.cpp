
#include "LuaAgent.h"
#include "LuaAgentUtils.h"
#include "LuaAgentLibUnit.h"
#include "LuaAgentLibWorldObj.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "TargetedMovementGenerator.h"
#include "Totem.h"
#include "DynamicObject.h"


void LuaBindsAI::BindUnit(lua_State* L) {
	Unit_CreateMetatable(L);
}


Unit* LuaBindsAI::Unit_GetUnitObject(lua_State* L, int idx) {
	return *((Unit**) luaL_checkudwithfield(L, idx, "isUnit"));//(Unit**) luaL_checkudata(L, idx, LuaBindsAI::UnitMtName);
}


void LuaBindsAI::Unit_CreateUD(Unit* unit, lua_State* L) {
	// create userdata on top of the stack pointing to a pointer of an AI object
	Unit** unitud = static_cast<Unit**>(lua_newuserdatauv(L, sizeof(Unit*), 0));
	*unitud = unit; // swap the AI object being pointed to to the current instance
	luaL_setmetatable(L, LuaBindsAI::UnitMtName);
}


int LuaBindsAI_Unit_ToString(lua_State* L)
{
	WorldObject* obj1 = *LuaBindsAI::WObj_GetWObjObject(L);
	lua_pushfstring(L, obj1->GetObjectGuid().GetString().c_str());
	return 1;
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
	lua_pushcfunction(L, LuaBindsAI_Unit_ToString);
	lua_setfield(L, -2, "__tostring");
	lua_pop(L, 1); // pop mt
}


int LuaBindsAI::Unit_GetAI(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Player* agent = unit->ToPlayer())
		if (LuaAgent* ai = agent->GetLuaAI())
		{
			ai->PushUD(L);
			return 1;
		}
	lua_pushnil(L);
	return 1;
}


// ---------------------------------------------------------
// --                Attacking
// ---------------------------------------------------------


int LuaBindsAI::Unit_Attack(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, unit->Attack(target, true));
	return 1;
}


int LuaBindsAI::Unit_AttackStop(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->AttackStop());
	return 1;
}


int LuaBindsAI::Unit_CalculateDamage(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer attType = luaL_checkinteger(L, 2);
	bool normalized = luaL_checkboolean(L, 3);
	if (attType < 0 || attType > WeaponAttackType::RANGED_ATTACK)
		luaL_error(L, "Unit_CalculateDamage: invalid attack type. Allowed values [0, %d], got %I", WeaponAttackType::RANGED_ATTACK, attType);
	lua_pushnumber(L, unit->CalculateDamage(WeaponAttackType(attType), normalized));
	return 1;
}


int LuaBindsAI::Unit_CanAttack(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, unit->CanAttack(target, true));
	return 1;
}


int LuaBindsAI::Unit_CastSpell(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_Integer spellId = luaL_checkinteger(L, 3);
	bool triggered = luaL_checkboolean(L, 4);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
	{
		if (!triggered && (unit->HasGCD(spell) || !unit->IsSpellReady(*spell)))
		{
			lua_pushinteger(L, SPELL_FAILED_NOT_READY);
			return 1;
		}
		if ((spell->InterruptFlags & SpellInterruptFlags::SPELL_INTERRUPT_FLAG_MOVEMENT) && !unit->IsStopped())
			unit->StopMoving();
		SpellCastResult result = unit->CastSpell(target, spell, triggered);
		if (result == SpellCastResult::SPELL_FAILED_NO_AMMO)
		{
			if (Player* player = unit->ToPlayer())
				if (LuaAgent* ai = player->GetLuaAI())
					if (uint32 ammo = ai->GetAmmo())
						if (!player->HasItemCount(ammo))
						{
							player->StoreNewItemInBestSlots(ammo, 200u);
							player->SetAmmo(ammo);
							result = unit->CastSpell(target, spell, triggered);
						}
		}
		else if (result == SpellCastResult::SPELL_FAILED_ITEM_NOT_READY)
		{
			if (Player* player = unit->ToPlayer())
				if (LuaAgent* ai = player->GetLuaAI())
					if (spell->Reagent[0])
					{
						player->StoreNewItemInBestSlots(spell->Reagent[0], spell->ReagentCount[0]);
						result = unit->CastSpell(target, spell, triggered);
					}
		}
		else if (result == SpellCastResult::SPELL_FAILED_UNIT_NOT_INFRONT)
		{
			if (Player* player = unit->ToPlayer())
				player->SetFacingTo(player->GetAngle(target));
		}
		lua_pushinteger(L, result);
	}
	else
		luaL_error(L, "Unit_CastSpell: spell %d doesn't exist", spellId);
	return 1;
}


int LuaBindsAI::Unit_GetCurrentSpellId(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	CurrentSpellTypes tp = CurrentSpellTypes::CURRENT_GENERIC_SPELL;
	if (lua_gettop(L) > 1)
	{
		lua_Integer inTp = luaL_checkinteger(L, 2);
		if (inTp < 0 || inTp >= CURRENT_MAX_SPELL)
			luaL_error(L, "Unit_GetCurrentSpellId: spell type doesn't exist. Allowed - [%d, %d], got %I", 0, CURRENT_MAX_SPELL - 1, inTp);
		tp = CurrentSpellTypes(inTp);
	}
	if (Spell* spell = unit->GetCurrentSpell(tp))
		lua_pushinteger(L, spell->m_spellInfo->Id);
	else
		lua_pushinteger(L, 0ll);
	return 1;
}


int LuaBindsAI::Unit_GetPowerCost(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
	{
		Spell s(unit, spell, false);
		lua_pushnumber(L, Spell::CalculatePowerCost(spell, unit, &s));
	}
	else
		luaL_error(L, "Unit_GetPowerCost: spell %d doesn't exist", spellId);
	return 1;
}


int LuaBindsAI::Unit_GetSheath(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetSheath());
	return 1;
}


int LuaBindsAI::Unit_GetSpellDamageAndThreat(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* unitTarget = Unit_GetUnitObject(L, 2);
	lua_Integer spellId = luaL_checkinteger(L, 3);
	bool isHealSpell = luaL_checkboolean(L, 4);
	bool forceCalc = luaL_checkboolean(L, 5);
	int effIdx = SpellEffectIndex::EFFECT_INDEX_0;
	if (lua_gettop(L) == 6)
	{
		effIdx = luaL_checkinteger(L, 6);
		if (effIdx < 0 || effIdx > SpellEffectIndex::EFFECT_INDEX_2)
			luaL_error(L, "Unit_GetSpellDamageAndThreat: invalid effect index. Allowed [0, %d], got %d", SpellEffectIndex::EFFECT_INDEX_2, effIdx);
	}
	if (const SpellEntry* spellProto = sSpellMgr.GetSpellEntry(spellId))
	{
		Spell s(unit, spellProto, true);
		float damage = s.CalculateDamage(SpellEffectIndex(effIdx), unitTarget);
		float threat = 0.f;
		if (forceCalc || isHealSpell || unitTarget->CanHaveThreatList())
		{
			if (isHealSpell)
			{
				damage = unit->SpellHealingBonusDone(unitTarget, spellProto, SpellEffectIndex(effIdx), damage, DamageEffectType::HEAL, 1, &s);
				damage = unitTarget->SpellHealingBonusTaken(unit, spellProto, SpellEffectIndex(effIdx), damage, DamageEffectType::HEAL, 1, &s);
				threat = damage * (unit->GetClass() == Classes::CLASS_PALADIN ? 0.25f : 0.5f);
				uint32 size = unitTarget->GetHostileRefManager().getSize();
				threat /= size ? size : 1;
			}
			else if (spellProto->IsDirectDamageSpell())
				threat = damage;

			SpellThreatEntry const* entry = sSpellMgr.GetSpellThreatEntry(spellProto->Id);
			if (entry)
			{
				threat *= entry->multiplier;
				threat += entry->threat;
			}

			threat = unit->ApplyTotalThreatModifier(threat, s.m_spellSchoolMask);
		}
		lua_pushnumber(L, damage);
		lua_pushnumber(L, threat);
	}
	else
		luaL_error(L, "Unit_GetSpellDamageAndThreat: spell %d doesn't exist", spellId);
	return 2;
}


int LuaBindsAI::Unit_GetSpellCastLeft(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Spell* spell = unit->GetCurrentSpell(CurrentSpellTypes::CURRENT_GENERIC_SPELL))
		lua_pushinteger(L, spell->GetCastedTime());
	else
		lua_pushinteger(L, 0ll);
	return 1;
}


int LuaBindsAI::Unit_HasEnoughPowerFor(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	bool allowDiffPower = luaL_checkboolean(L, 3);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
	{
		if (allowDiffPower || spell->powerType == unit->GetPowerType())
		{
			Spell s(unit, spell, false);
			uint32 cost = Spell::CalculatePowerCost(spell, unit, &s);
			lua_pushboolean(L, cost <= unit->GetPower(Powers(spell->powerType)));
			return 1;
		}
	}
	else
		luaL_error(L, "Unit_HasEnoughPowerFor: spell %d doesn't exist", spellId);
	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_InterruptSpell(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer tp = luaL_checkinteger(L, 2);
	if (tp < 0 || tp > CurrentSpellTypes::CURRENT_CHANNELED_SPELL)
		luaL_error(L, "Unit_InterruptSpell: invalid spell type. Allowed values [0, %d], got %d", CurrentSpellTypes::CURRENT_CHANNELED_SPELL, tp);
	unit->InterruptSpell(CurrentSpellTypes(tp), false);
	return 0;
}


int LuaBindsAI::Unit_IsCastingHeal(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Spell* spell = unit->GetCurrentSpell(CurrentSpellTypes::CURRENT_GENERIC_SPELL))
		lua_pushboolean(L, spell->m_spellInfo->IsHealSpell());
	else
		lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_IsCastingInterruptableSpell(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);

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


int LuaBindsAI::Unit_IsImmuneToSpell(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellid = luaL_checkinteger(L, 2);
	auto entry = sSpellMgr.GetSpellEntry(spellid);
	if (!entry)
		luaL_error(L, "Unit_IsImmuneToSpell: spell doesn't exist %I", spellid);
	lua_pushboolean(L, unit->IsImmuneToSpell(entry, false));
	return 1;
}


int LuaBindsAI::Unit_IsInPositionToCast(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_Integer spellId = luaL_checkinteger(L, 3);
	lua_Number bufferDist = luaL_checknumber(L, 4);
	if (const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId))
	{
		// Spell::CheckTarget and Spell:CheckRange bits
		if (!(spell->AttributesEx2 & SpellAttributesEx2::SPELL_ATTR_EX2_IGNORE_LINE_OF_SIGHT) && !unit->IsWithinLOSInMap(target))
			lua_pushinteger(L, SpellCastResult::SPELL_FAILED_LINE_OF_SIGHT);
		else
		{
			SpellCastResult result = SpellCastResult::SPELL_CAST_OK;
			SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spell->rangeIndex);
			float max_range = srange ? srange->maxRange : 0.f;
			float min_range = srange ? srange->minRange : 0.f;

			if (Player* modOwner = unit->ToPlayer())
				modOwner->ApplySpellMod(spell->Id, SpellModOp::SPELLMOD_RANGE, max_range, nullptr);

			if (target && target != unit)
			{
				float const dist = unit->GetCombatDistance(target);

				if (dist > max_range - bufferDist)
					result = SpellCastResult::SPELL_FAILED_OUT_OF_RANGE;
				else if (min_range && dist < min_range)
					result = SpellCastResult::SPELL_FAILED_TOO_CLOSE;
			}
			lua_pushinteger(L, result);
		}
	}
	else
		luaL_error(L, "Unit_IsInPositionToCast: spell %d doesn't exist", spellId);
	return 1;
}


int LuaBindsAI::Unit_IsInLOS(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, unit->IsWithinLOSInMap(target));
	return 1;
}


int LuaBindsAI::Unit_IsSpellReady(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellid = luaL_checkinteger(L, 2);
	if (const SpellEntry* entry = sSpellMgr.GetSpellEntry(spellid))
		lua_pushboolean(L, unit->IsSpellReady(*entry));
	else
		luaL_error(L, "Unit_IsSpellReady: spell %I doesn't exist", spellid);
	return 1;
}


int LuaBindsAI::Unit_IsTargetableByHeal(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, !(unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE_2) || unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER)));
	return 1;
}


int LuaBindsAI::Unit_GetTotemEntry(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer slot = luaL_checkinteger(L, 2);
	if (slot < 0 || slot > TotemSlot::TOTEM_SLOT_AIR)
		luaL_error(L, "Unit_GetTotemEntry: invalid totem slot. Allowed values [0, %d], got %I", TotemSlot::TOTEM_SLOT_AIR, slot);
	if (Totem* totem = unit->GetTotem(TotemSlot(slot)))
		lua_pushinteger(L, totem->GetEntry());
	else
		lua_pushnil(L);
	return 1;
}


int LuaBindsAI::Unit_GetTotems(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer idx = 1;
	lua_newtable(L);
	for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
		if (Totem* totem = unit->GetTotem(TotemSlot(i)))
		{
			lua_pushunitornil(L, totem);
			lua_seti(L, -2, idx);
			++idx;
		}
	return 1;
}


int LuaBindsAI::Unit_GetGuardians(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (!unit->GetMap()) return 0;
	lua_newtable(L);
	lua_Integer idx = 1;
	for (const auto& guid : *(unit->GetGuardianPets()))
		if (Pet* pet = unit->GetMap()->GetPet(guid))
		{
			lua_pushunitornil(L, pet);
			lua_seti(L, -2, idx);
			++idx;
		}
	return 1;
}


int LuaBindsAI::Unit_RemoveSpellCooldown(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (spellId < 0 || !sSpellMgr.GetSpellEntry(spellId))
		luaL_error(L, "Unit_RemoveSpellCooldown: spell %d doesn't exist", spellId);
	unit->RemoveSpellCooldown(spellId, true);
	return 0;
}


int LuaBindsAI::Unit_UnsummonAllTotems(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	unit->UnsummonAllTotems();
	return 0;
}


// ---------------------------------------------------------
// --                     Info
// ---------------------------------------------------------


int LuaBindsAI::Unit_IsAgent(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Player* agent = unit->ToPlayer())
		lua_pushboolean(L, agent->IsLuaAgent());
	else
		lua_pushboolean(L, unit->IsAlive());
	return 1;
}


int LuaBindsAI::Unit_IsAlive(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsAlive());
	return 1;
}


int LuaBindsAI::Unit_IsDead(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsDead());
	return 1;
}


int LuaBindsAI::Unit_IsInCombat(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsInCombat());
	return 1;
}


int LuaBindsAI::Unit_IsNonMeleeSpellCasted(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsNonMeleeSpellCasted(false, false, true));
	return 1;
}


int LuaBindsAI::Unit_IsNextSwingSpellCasted(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsNextSwingSpellCasted());
	return 1;
}


int LuaBindsAI::Unit_IsTanking(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Player* player = unit->ToPlayer())
		if (LuaAgent* agentAI = player->GetLuaAI())
		{
			lua_pushboolean(L, agentAI->CommandsGetFirstType() == AgentCmdType::Tank);
			return 1;
		}
	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_GetAngle(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* unit2 = Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, unit->GetAngle(unit2));
	return 1;
}


int LuaBindsAI::Unit_GetBoundingRadius(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetObjectBoundingRadius());
	return 1;
}


int LuaBindsAI::Unit_GetCombatReach(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (lua_gettop(L) == 2)
		lua_pushnumber(L, unit->GetCombatReachToTarget(Unit_GetUnitObject(L, 2), false, 0.f, true));
	else
		lua_pushnumber(L, unit->GetCombatReach(true));
	return 1;
}


int LuaBindsAI::Unit_GetCreatureChaseInfo(lua_State* L)
{
	//Unit* unit = Unit_GetUnitObject(L);
	//if (unit->GetMotionMaster()->GetCurrentMovementGeneratorType() == MovementGeneratorType::CHASE_MOTION_TYPE)
	//	if (auto constGen = dynamic_cast<const ChaseMovementGenerator<Creature>*>(unit->GetMotionMaster()->GetCurrent()))
	//	{
	//		auto gen = const_cast<ChaseMovementGenerator<Creature>*>(constGen);
	//		lua_pushnumber(L, gen->GetOffset());
	//		lua_pushnumber(L, gen->GetAngle());
	//		return 2;
	//	}
	return 0;
}


int LuaBindsAI::Unit_GetDistance(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (lua_gettop(L) == 2)
	{
		Unit* target = Unit_GetUnitObject(L, 2);
		lua_pushnumber(L, unit->GetDistance(target));
	}
	else
	{
		lua_Number x = luaL_checknumber(L, 2);
		lua_Number y = luaL_checknumber(L, 3);
		lua_Number z = luaL_checknumber(L, 4);
		lua_pushnumber(L, unit->GetDistance(x, y, z));
	}
	return 1;
}


int LuaBindsAI::Unit_GetDistanceEx(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (lua_gettop(L) == 3)
	{
		Unit* target = Unit_GetUnitObject(L, 2);
		lua_Integer sf = luaL_checkinteger(L, 3);
		if (sf < 0 || sf > (lua_Integer) SizeFactor::CombatReachWithMelee)
			luaL_error(L, "Unit_GetDistanceEx: invalid size factr. Got %I, allowed [0, 3]", sf);
		lua_pushnumber(L, unit->GetDistance(target, SizeFactor(sf)));
	}
	else
	{
		lua_Number x = luaL_checknumber(L, 2);
		lua_Number y = luaL_checknumber(L, 3);
		lua_Number z = luaL_checknumber(L, 4);
		lua_Integer sf = luaL_checkinteger(L, 5);
		if (sf < 0 || sf > (lua_Integer) SizeFactor::CombatReachWithMelee)
			luaL_error(L, "Unit_GetDistanceEx: invalid size factr. Got %I, allowed [0, 3]", sf);
		lua_pushnumber(L, unit->GetDistance(x, y, z, SizeFactor(sf)));
	}
	return 1;
}


int LuaBindsAI::Unit_GetForwardVector(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	float ori = unit->GetOrientation();
	lua_newtable(L);
	lua_pushnumber(L, std::cos(ori));
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, std::sin(ori));
	lua_setfield(L, -2, "y");
	return 1;
}


int LuaBindsAI::Unit_GetOrientation(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetOrientation());
	return 1;
}


int LuaBindsAI::Unit_GetPosition(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetPositionX());
	lua_pushnumber(L, unit->GetPositionY());
	lua_pushnumber(L, unit->GetPositionZ());
	return 3;
}


int LuaBindsAI::Unit_GetPositionOfObj(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 2);
	if (!unit->GetMap())
		return 0;

	if (GameObject* obj = unit->GetMap()->GetGameObject(guid->guid))
	{
		lua_pushnumber(L, obj->GetPositionX());
		lua_pushnumber(L, obj->GetPositionY());
		lua_pushnumber(L, obj->GetPositionZ());
		return 3;
	}
	return 0;
}


int LuaBindsAI::Unit_GetMapId(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetMapId());
	return 1;
}


int LuaBindsAI::Unit_GetZoneId(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetZoneId());
	return 1;
}


int LuaBindsAI::Unit_IsInDungeon(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->GetMap()->IsDungeon());
	return 1;
}


int LuaBindsAI::Unit_GetHealth(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetHealth());
	return 1;
}


int LuaBindsAI::Unit_GetHealthPct(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetHealthPercent());
	return 1;
}


int LuaBindsAI::Unit_GetMaxHealth(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetMaxHealth());
	return 1;
}


int LuaBindsAI::Unit_GetPower(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer powerType = luaL_checkinteger(L, 2);
	if (powerType < 0 || powerType > Powers::POWER_HAPPINESS)
		luaL_error(L, "Unit_GetPower: allowed power types [0, %d], got %d", Powers::POWER_HAPPINESS, powerType);
	lua_pushinteger(L, unit->GetPower(Powers(powerType)));
	return 1;
}


int LuaBindsAI::Unit_GetPowerPct(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer powerType = luaL_checkinteger(L, 2);
	if (powerType < 0 || powerType > Powers::POWER_HAPPINESS)
		luaL_error(L, "Unit_GetPowerPct: allowed power types [0, %d], got %d", Powers::POWER_HAPPINESS, powerType);
	lua_pushnumber(L, unit->GetPowerPercent(Powers(powerType)));
	return 1;
}


int LuaBindsAI::Unit_GetMaxPower(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer powerType = luaL_checkinteger(L, 2);
	if (powerType < 0 || powerType > Powers::POWER_HAPPINESS)
		luaL_error(L, "Unit_GetMaxPower: allowed power types [0, %d], got %d", Powers::POWER_HAPPINESS, powerType);
	lua_pushinteger(L, unit->GetMaxPower(Powers(powerType)));
	return 1;
}


int LuaBindsAI::Unit_GetPowerType(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetPowerType());
	return 1;
}


int LuaBindsAI::Unit_Kill(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Creature* targetCreature = unit->ToCreature()) targetCreature->SetLootRecipient(nullptr);
	unit->DealDamage(unit, unit->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
	return 0;
}


int LuaBindsAI::Unit_SetHealth(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number value = luaL_checknumber(L, 2);
	if (value < 0ll)
		value = 0ll;
	unit->SetHealth(value);
	return 0;
}


int LuaBindsAI::Unit_SetHealthPct(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number value = luaL_checknumber(L, 2);
	if (value < .0)
		luaL_error(L, "Unit_SetHealthPct: value must be >= 0");
	unit->SetHealthPercent(value);
	return 0;
}


int LuaBindsAI::Unit_SetMaxHealth(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number value = luaL_checknumber(L, 2);
	if (value < 1ll)
		luaL_error(L, "Unit_SetMaxHealth: value must be >= 1");
	unit->SetMaxHealth(value);
	return 0;
}


int LuaBindsAI::Unit_SetPower(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer powerType = luaL_checkinteger(L, 2);
	lua_Integer value = luaL_checkinteger(L, 3);
	if (powerType < 0ll || powerType > Powers::POWER_HAPPINESS)
		luaL_error(L, "Unit_SetPower: allowed power types [0, %d], got %d", Powers::POWER_HAPPINESS, powerType);
	if (value < 0ll)
		luaL_error(L, "Unit_SetPower: value must be >= 0");
	unit->SetPower(Powers(powerType), value);
	return 0;
}


int LuaBindsAI::Unit_SetPowerPct(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer powerType = luaL_checkinteger(L, 2);
	lua_Number value = luaL_checknumber(L, 3);
	if (powerType < 0ll || powerType > Powers::POWER_HAPPINESS)
		luaL_error(L, "Unit_SetPowerPct: allowed power types [0, %d], got %d", Powers::POWER_HAPPINESS, powerType);
	if (value < .0)
		luaL_error(L, "Unit_SetPowerPct: value must be >= 0");
	unit->SetPowerPercent(Powers(powerType), value);
	return 0;
}


int LuaBindsAI::Unit_SetMaxPower(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer powerType = luaL_checkinteger(L, 2);
	lua_Integer value = luaL_checkinteger(L, 3);
	if (powerType < 0ll || powerType > Powers::POWER_HAPPINESS)
		luaL_error(L, "Unit_SetMaxPower: allowed power types [0, %d], got %d", Powers::POWER_HAPPINESS, powerType);
	if (value < 1ll)
		luaL_error(L, "Unit_SetMaxPower: value must be >= 1");
	unit->SetMaxPower(Powers(powerType), value);
	return 0;
}


int LuaBindsAI::Unit_CanReachWithMeleeAutoAttack(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, unit->CanReachWithMeleeAutoAttack(target));
	return 1;
}


int LuaBindsAI::Unit_CanHaveThreatList(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->CanHaveThreatList());
	return 1;
}


int LuaBindsAI::Unit_GetAttackers(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_newtable(L);
	int tblIdx = 1;
	for (const auto pAttacker : unit->GetAttackers())
		if (unit->CanAttack(pAttacker, true)) {
			Unit_CreateUD(pAttacker, L);
			lua_seti(L, -2, tblIdx);
			tblIdx++;
		}
	return 1;
}


int LuaBindsAI::Unit_GetAttackersNum(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetAttackers().size());
	return 1;
}


int LuaBindsAI::Unit_GetThreat(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	lua_pushnumber(L, unit->GetThreatManager().getThreat(target));
	return 1;
}


int LuaBindsAI::Unit_GetThreatTbl(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
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


int LuaBindsAI::Unit_GetHighestThreat(lua_State* L)
{
	// todo: delete this function?
	Unit* unit = Unit_GetUnitObject(L);
	Unit* except = nullptr;

	if (lua_gettop(L) > 1)
		except = Unit_GetUnitObject(L, 2);

	float maxTank = 0.f;
	float maxNonTank = 0.f;
	for (auto& entry : unit->GetThreatManager().getThreatList())
	{
		Player* player = entry->getTarget()->ToPlayer();
		if (!player || (except && player->GetObjectGuid() == except->GetObjectGuid()))
			continue;

		LuaAgent* agentAI = player->GetLuaAI();
		bool isNotTank = !agentAI || agentAI->GetRole() != LuaAgentRoles::Tank;
		if (isNotTank)
		{
			if (maxNonTank == 0.f)
				maxNonTank = entry->getThreat();
		}
		else if (maxTank == 0.f)
			maxTank = entry->getThreat();

		if (maxTank != 0.f && maxNonTank != 0.f)
			break;
	}
	lua_pushnumber(L, maxNonTank);
	lua_pushnumber(L, maxTank);
	return 2;
}


int LuaBindsAI::Unit_GetVictim(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushunitornil(L, unit->GetVictim());
	return 1;
}


int LuaBindsAI::Unit_GetShapeshiftForm(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetShapeshiftForm());
	return 1;
}


int LuaBindsAI::Unit_IsRanged(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->HasDistanceCasterMovement());
	return 1;
}


int LuaBindsAI::Unit_HasAura(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (!sSpellMgr.GetSpellEntry(spellId))
		luaL_error(L, "Unit_HasAura: spell %I doesn't exist", spellId);
	lua_pushboolean(L, unit->HasAura(spellId));
	return 1;
}


int LuaBindsAI::Unit_HasAuraType(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer auraId = luaL_checkinteger(L, 2);
	if (auraId < SPELL_AURA_NONE || auraId >= TOTAL_AURAS)
		luaL_error(L, "Unit_HasAuraType: invalid aura type id, expected value in range [0, %d], got %I", SPELL_AURA_NONE, TOTAL_AURAS - 1, auraId);
	lua_pushboolean(L, unit->HasAuraType((AuraType) auraId));
	return 1;
}


int LuaBindsAI::Unit_HasAuraWithMechanics(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer mask = luaL_checkinteger(L, 2);
	for (auto& itr : unit->GetSpellAuraHolderMap())
		if (itr.second->HasMechanicMask(mask))
		{
			lua_pushboolean(L, true);
			return 1;
		}
	lua_pushboolean(L, false);
	return 1;
}

int LuaBindsAI::Unit_GetAurasByTypeTbl(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer tp = luaL_checkinteger(L, 2);
	lua_newtable(L);
	lua_Integer idx = 1;
	for (auto& itr : unit->GetAurasByType(AuraType(tp)))
	{
		lua_newtable(L);
		lua_pushboolean(L, itr->IsPositive());
		lua_setfield(L, -2, "positive");
		lua_pushinteger(L, itr->GetSpellProto()->Id);
		lua_setfield(L, -2, "spellid");
		lua_pushinteger(L, itr->GetSpellProto()->Dispel);
		lua_setfield(L, -2, "dispel");
		lua_seti(L, -2, idx);
		++idx;
	}
	return 1;
}

int LuaBindsAI::Unit_GetAuraStacks(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (spellId < 0 || !sSpellMgr.GetSpellEntry(spellId))
		luaL_error(L, "Unit_GetAuraStacks: spell %I doesn't exist", spellId);
	if (SpellAuraHolder* sah = unit->GetSpellAuraHolder(spellId))
		lua_pushinteger(L, sah->GetStackAmount());
	else
		lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_GetAuraTimeLeft(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (spellId < 0 || !sSpellMgr.GetSpellEntry(spellId))
		luaL_error(L, "Unit_GetAuraTimeLeft: spell %I doesn't exist", spellId);
	if (SpellAuraHolder* sah = unit->GetSpellAuraHolder(spellId))
		lua_pushinteger(L, sah->GetAuraDuration());
	else
		lua_pushinteger(L, -1);
	return 1;
}


int LuaBindsAI::Unit_GetAuraTypeTimeLeft(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer auraTp = luaL_checkinteger(L, 2);
	if (auraTp < 0 || auraTp >= AuraType::TOTAL_AURAS)
		luaL_error(L, "Unit_GetAuraTypeTimeLeft: aura type %I doesn't exist", auraTp);
	int timeleft = -1;
	for (auto& it : unit->GetAurasByType(AuraType(auraTp)))
	{
		if (it->GetAuraDuration() > timeleft)
			timeleft = it->GetAuraDuration();
	}
	lua_pushinteger(L, timeleft);
	return 1;
}


int LuaBindsAI::Unit_GetDispelTbl(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	bool positive = luaL_checkboolean(L, 2);
	lua_newtable(L);
	for (auto& it : unit->GetSpellAuraHolderMap())
	{
		SpellAuraHolder* sah = it.second;
		if (sah->IsPositive() != positive)
			continue;
		auto proto = sah->GetSpellProto();
		if (proto->Dispel == DISPEL_MAGIC || proto->Dispel == DISPEL_ALL)
		{
			lua_pushboolean(L, true);
			lua_setfield(L, -2, "Magic");
		}
		if (proto->Dispel == DISPEL_DISEASE || proto->Dispel == DISPEL_ALL)
		{
			lua_pushboolean(L, true);
			lua_setfield(L, -2, "Disease");
		}
		if (proto->Dispel == DISPEL_CURSE || proto->Dispel == DISPEL_ALL)
		{
			lua_pushboolean(L, true);
			lua_setfield(L, -2, "Curse");
		}
		if (proto->Dispel == DISPEL_POISON || proto->Dispel == DISPEL_ALL)
		{
			lua_pushboolean(L, true);
			lua_setfield(L, -2, "Poison");
		}
	}
	return 1;
}


int LuaBindsAI::Unit_GetPersistentAreaAuraInfo(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (DynamicObject* obj = unit->GetDynObject(spellId))
	{
		lua_newtable(L);
		lua_pushnumber(L, obj->GetPositionX());
		lua_setfield(L, -2, "x");
		lua_pushnumber(L, obj->GetPositionY());
		lua_setfield(L, -2, "y");
		lua_pushnumber(L, obj->GetPositionZ());
		lua_setfield(L, -2, "z");
		lua_pushnumber(L, obj->GetRadius());
		lua_setfield(L, -2, "r");
		lua_pushinteger(L, obj->GetDuration());
		lua_setfield(L, -2, "d");
	}
	else
		lua_pushnil(L);
	return 1;
}


int LuaBindsAI::Unit_RemoveAuraByCancel(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer spellId = luaL_checkinteger(L, 2);
	if (spellId < 0 || !sSpellMgr.GetSpellEntry(spellId))
		luaL_error(L, "Unit_RemoveAuraByCancel: spell %I doesn't exist", spellId);
	unit->RemoveAurasDueToSpellByCancel(spellId);
	return 0;
}


int LuaBindsAI::Unit_RemoveSpellsCausingAura(lua_State* L) {
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer auraType = luaL_checkinteger(L, 2);
	if (auraType < 0 || auraType >= AuraType::TOTAL_AURAS)
		luaL_error(L, "Unit_RemoveSpellsCausingAura: aura type %I doesn't exist", auraType);
	unit->RemoveSpellsCausingAura(AuraType(auraType));
	return 0;
}


int LuaBindsAI::Unit_HasUnitState(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer state = luaL_checkinteger(L, 2);
	lua_pushboolean(L, unit->HasUnitState(state));
	return 1;
}


// ---------------------------------------------------------
// --                General Info
// ---------------------------------------------------------


int LuaBindsAI::Unit_GetClass(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetClass());
	return 1;
}


int LuaBindsAI::Unit_GetEntry(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetEntry());
	return 1;
}


int LuaBindsAI::Unit_GetId(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetObjectGuid().GetCounter());
	return 1;
}


int LuaBindsAI::Unit_GetGuid(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Guid_CreateUD(L, unit->GetObjectGuid());
	return 1;
}


int LuaBindsAI::Unit_GetLevel(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetLevel());
	return 1;
}


int LuaBindsAI::Unit_GetName(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushstring(L, unit->GetName());
	return 1;
}


int LuaBindsAI::Unit_GetRace(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetRace());
	return 1;
}


int LuaBindsAI::Unit_GetRole(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (Player* player = unit->ToPlayer())
		if (LuaAgent* agentAI = player->GetLuaAI())
		{
			lua_pushinteger(L, lua_Integer(agentAI->GetRole()));
			return 1;
		}
	lua_pushinteger(L, 0ll);
	return 1;
}


int LuaBindsAI::Unit_GetTeam(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetTeam());
	return 1;
}


int LuaBindsAI::Unit_IsPlayer(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsPlayer());
	return 1;
}


// ---------------------------------------------------------
// --                Movement Control
// ---------------------------------------------------------


int LuaBindsAI::Unit_ClearMotion(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (unit->GetMotionMaster()->LuaAIMoveClear())
		unit->StopMoving();
	return 0;
}


int LuaBindsAI::Unit_GetMotionType(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushnumber(L, unit->GetMotionMaster()->GetCurrentMovementGeneratorType());
	return 1;
}


int LuaBindsAI::Unit_GetSpeed(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer mode = luaL_checkinteger(L, 2);
	lua_pushnumber(L, unit->GetSpeed(UnitMoveType(mode)));
	return 1;
}


int LuaBindsAI::Unit_HasLostControl(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	UnitState states = UnitState(
		UnitState::UNIT_STAT_STUNNED |
		UnitState::UNIT_STAT_POSSESSED |
		UnitState::UNIT_STAT_TAXI_FLIGHT |
		UnitState::UNIT_STAT_DISTRACTED |
		UnitState::UNIT_STAT_CONFUSED |
		UnitState::UNIT_STAT_FLEEING |
		UnitState::UNIT_STAT_PENDING_STUNNED
	);
	lua_pushboolean(L, unit->HasUnitState(states) || unit->IsCharmed() || !unit->GetPossessorGuid().IsEmpty());
	return 1;
}


int LuaBindsAI::Unit_IsMoving(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, unit->IsMoving());
	return 1;
}


int LuaBindsAI::Unit_IsWalking(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushboolean(L, !unit->HasUnitState(UnitState::UNIT_STAT_RUNNING) || unit->IsWalking());
	return 1;
}


int LuaBindsAI::Unit_MoveFacing(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number offset = luaL_checknumber(L, 2);
	unit->SetFacingTo(offset);
	return 0;
}


int LuaBindsAI::Unit_MoveFollow(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	double offset = luaL_checknumber(L, 3);
	double angle = luaL_checknumber(L, 4);
	unit->GetMotionMaster()->LuaAIMoveFollow(target, offset, angle);
	return 0;
}


int LuaBindsAI::Unit_MoveChase(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	double offset = luaL_checknumber(L, 3);
	double offsetMin = luaL_checknumber(L, 4);
	double offsetMax = luaL_checknumber(L, 5);
	double angle = luaL_checknumber(L, 6);
	double angleT = luaL_checknumber(L, 7);
	bool noMinOffsetIfMutual = luaL_checkboolean(L, 8);
	bool useAngle = luaL_checkboolean(L, 9);
	bool isRanged = luaL_checkboolean(L, 10);
	unit->GetMotionMaster()->LuaAIMoveChase(target, offset, offsetMin, offsetMax, angle, angleT, noMinOffsetIfMutual, useAngle, isRanged);
	return 0;
}


int LuaBindsAI::Unit_MovePoint(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number x = luaL_checknumber(L, 2);
	lua_Number y = luaL_checknumber(L, 3);
	lua_Number z = luaL_checknumber(L, 4);
	bool force = luaL_checkboolean(L, 5);
	uint32 options = MoveOptions::MOVE_PATHFINDING | MoveOptions::MOVE_RUN_MODE;
	if (force)
		options |= MoveOptions::MOVE_FORCE_DESTINATION;
	unit->GetMotionMaster()->MovePoint(2048, x, y, z, options);
	return 0;
}


int LuaBindsAI::Unit_StopMoving(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	unit->StopMoving();
	return 0;
}


int LuaBindsAI::Unit_GetStandState(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_pushinteger(L, unit->GetStandState());
	return 1;
}


int LuaBindsAI::Unit_SetStandState(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Integer state = luaL_checkinteger(L, 2);
	if (state < 0 || state > UnitStandStateType::UNIT_STAND_STATE_KNEEL)
		luaL_error(L, "Unit_SetStandState: invalid state. Allowed values [0, %d], got %d", UnitStandStateType::UNIT_STAND_STATE_KNEEL, state);
	unit->SetStandState(state);
	return 0;
}


int LuaBindsAI::Unit_GetMapHeight(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number x = luaL_checknumber(L, 2);
	lua_Number y = luaL_checknumber(L, 3);
	lua_Number z = luaL_checknumber(L, 4);
	lua_pushnumber(L, unit->GetMap()->GetHeight(x, y, z));
	return 1;
}


int LuaBindsAI::Unit_GetAllowedZ(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	lua_Number x = luaL_checknumber(L, 2);
	lua_Number y = luaL_checknumber(L, 3);
	lua_Number z = luaL_checknumber(L, 4);
	float result = z;
	unit->UpdateAllowedPositionZ(x,y,result);
	lua_pushnumber(L, result);
	return 1;
}


int LuaBindsAI::Unit_HasInArc(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* target = Unit_GetUnitObject(L, 2);
	float arc = luaL_checknumber(L, 3);
	float offset = 0.0F;
	if (lua_gettop(L) == 4)
		offset = luaL_checknumber(L, 4);
	lua_pushboolean(L, unit->HasInArc(target, arc, offset));
	return 1;
}


int LuaBindsAI::Unit_GetReactionTo(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* to = Unit_GetUnitObject(L, 2);
	lua_pushinteger(L, unit->GetReactionTo(to));
	return 1;
}


int LuaBindsAI::Unit_IsFriendlyTo(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* to = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, unit->IsFriendlyTo(to));
	return 1;
}


int LuaBindsAI::Unit_IsHostileTo(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	Unit* to = Unit_GetUnitObject(L, 2);
	lua_pushboolean(L, unit->IsHostileTo(to));
	return 1;
}

// ---------------------------------------------------------
// --                 Interactions
// ---------------------------------------------------------

int LuaBindsAI::Unit_CanUseObj(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 2);
	if (!unit->GetMap())
	{
		lua_pushboolean(L, false);
		return 1;
	}

	if (GameObject* obj = unit->GetMap()->GetGameObject(guid->guid))
	{
		lua_pushboolean(L, unit->IsWithinDistInMap(obj, INTERACTION_DISTANCE));
		return 1;
	}
	else
		luaL_error(L, "Unit_CanUseObj: object not found %s", guid->guid.GetString().c_str());

	lua_pushboolean(L, false);
	return 1;
}


int LuaBindsAI::Unit_UseObj(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	LuaObjectGuid* guid = Guid_GetGuidObject(L, 2);
	if (!unit->GetMap())
		return 0;

	if (GameObject* obj = unit->GetMap()->GetGameObject(guid->guid))
		obj->Use(unit);
	else
		luaL_error(L, "Unit_UseObj: object not found %s", guid->guid.GetString().c_str());

	return 0;
}


int LuaBindsAI::Unit_GetLootList(lua_State* L)
{
	Unit* other = Unit_GetUnitObject(L);
	lua_newtable(L);
	lua_Integer idx = 1;
	if (other->IsCreature() && other->IsDead())
	{
		Creature* c = static_cast<Creature*>(other);
		Loot* loot = &c->loot;
		for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
		{
			LootItem& lootItem = loot->items[itemSlot];
			ItemPrototype const* itemProto = sObjectMgr.GetItemPrototype(lootItem.itemid);
			if (!itemProto)
			{
				sLog.Out(LOG_BASIC, LOG_LVL_DEBUG, "Unit_GetLootList: missing item prototype for item with id: %d", lootItem.itemid);
				continue;
			}
			if (lootItem.is_looted)
				continue;
			lua_pushinteger(L, lootItem.itemid);
			lua_seti(L, -2, idx);
			++idx;
		}
	}
	return 1;
}


int LuaBindsAI::Unit_GetLootRecipient(lua_State* L)
{
	Unit* unit = Unit_GetUnitObject(L);
	if (unit->IsCreature())
	{
		Creature* c = static_cast<Creature*>(unit);
		Guid_CreateUD(L, c->GetLootRecipientGuid());
	}
	else
		Guid_CreateUD(L);
	return 1;
}
