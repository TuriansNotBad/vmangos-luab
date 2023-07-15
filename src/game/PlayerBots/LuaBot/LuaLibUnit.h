#ifndef MANGOS_LuaBotUnit_H
#define MANGOS_LuaBotUnit_H

#include "lua.hpp"

namespace LuaBindsAI {

	static const char* UnitMtName = "LuaAI.Unit";

	// Creates metatable for the AI userdata with name specified by AI::Unit_MTNAME
	void BindUnit(lua_State* L);
	// Creates metatable for the AI userdata with name specified by AI::Unit_MTNAME.
	// Registers all the functions listed in LuaBindsBot::Unit_BindLib with that metatable.
	void Unit_CreateMetatable(lua_State* L);
	Unit** Unit_GetUnitObject(lua_State* L, int idx = 1);
	void Unit_CreateUD(Unit* unit, lua_State* L);

	int Unit_Print(lua_State* L);

	int Unit_GetAI(lua_State* L);

	// combat related
	int Unit_AddAura(lua_State* L);
	int Unit_AddThreat(lua_State* L);
	int Unit_CalculateSpellEffect(lua_State* L);
	int Unit_CastSpell(lua_State* L);
	int Unit_ClearTarget(lua_State* L);
	int Unit_GetAttackersTbl(lua_State* L);
	int Unit_GetAttackRange(lua_State* L);
	int Unit_GetAttackTimer(lua_State* L);
	int Unit_GetAuraStacks(lua_State* L);
	int Unit_GetAuraDuration(lua_State* L);
	int Unit_GetCombatDistance(lua_State* L);
	int Unit_GetCombatReach(lua_State* L);
	int Unit_GetCurrentSpellId(lua_State* L);
	int Unit_GetEnemyCountInRadiusAround(lua_State* L);
	int Unit_GetHealth(lua_State* L);
	int Unit_GetHealthPercent(lua_State* L);
	int Unit_GetMaxHealth(lua_State* L);
	int Unit_GetMaxPower(lua_State* L);
	int Unit_GetMeleeReach(lua_State* L);
	int Unit_GetMeleeRange(lua_State* L);
	int Unit_GetPet(lua_State* L);
	int Unit_GetPower(lua_State* L);
	int Unit_GetPowerPercent(lua_State* L);
	int Unit_GetSpellCost(lua_State* L);
	int Unit_GetSpellCD(lua_State* L);
	int Unit_GetShapeshiftForm(lua_State* L);
	int Unit_GetThreat(lua_State* L);
	int Unit_GetThreatTbl(lua_State* L);
	int Unit_HasTotem(lua_State* L);
	int Unit_GetVictim(lua_State* L);
	int Unit_GetVictimsInRange(lua_State* L);
	int Unit_GetVictimNearest(lua_State* L);
	int Unit_HasAura(lua_State* L);
	int Unit_HasAuraBy(lua_State* L);
	int Unit_HasAuraType(lua_State* L);
	int Unit_HasAuraWithMechanic(lua_State* L);
	int Unit_HasObjInArc(lua_State* L);
	int Unit_HasPosInArc(lua_State* L);
	int Unit_InterruptSpell(lua_State* L);
	int Unit_IsInCombat(lua_State* L);
	int Unit_IsNonMeleeSpellCasted(lua_State* L);
	int Unit_IsCastingInterruptableSpell(lua_State* L);
	int Unit_IsSpellReady(lua_State* L);
	int Unit_IsTargetInRangeOfSpell(lua_State* L);
	int Unit_IsValidHostileTarget(lua_State* L);
	int Unit_PetAttack(lua_State* L);
	int Unit_PetCast(lua_State* L);
	int Unit_PetAutocast(lua_State* L);
	int Unit_PetCasterChaseDist(lua_State* L);
	int Unit_RemoveAura(lua_State* L);
	int Unit_RemoveAuraByCancel(lua_State* L);
	int Unit_RemoveAurasByCasterSpell(lua_State* L);
	int Unit_RemoveSpellCooldown(lua_State* L);
	int Unit_RemoveSpellsCausingAura(lua_State* L);
	int Unit_SetHealth(lua_State* L);
	int Unit_SetHealthPercent(lua_State* L);
	int Unit_SetMaxHealth(lua_State* L);
	int Unit_SetMaxPower(lua_State* L);
	int Unit_SetPower(lua_State* L);
	int Unit_SetPowerPercent(lua_State* L);

	//caster chase
	int Unit_HasDistanceCasterMovement(lua_State* L);
	int Unit_SetCasterChaseDistance(lua_State* L);

	//gen info
	int Unit_ClearUnitState(lua_State* L);
	int Unit_GetClass(lua_State* L);
	int Unit_GetDisplayId(lua_State* L);
	int Unit_GetEntry(lua_State* L);
	int Unit_GetLevel(lua_State* L);
	int Unit_GetNativeDisplayId(lua_State* L);
	int Unit_GetName(lua_State* L);
	int Unit_GetObjectGuid(lua_State* L);
	int Unit_GetObjectBoundingRadius(lua_State* L);
	int Unit_GetPowerType(lua_State* L);
	int Unit_GetRace(lua_State* L);
	int Unit_GetRank(lua_State* L);
	int Unit_GetTargetGuid(lua_State* L);
	int Unit_HasUnitState(lua_State* L);
	int Unit_IsInDisallowedMountForm(lua_State* L);
	int Unit_IsInDungeon(lua_State* L);
	int Unit_IsPlayer(lua_State* L);

	// movement related
	int Unit_DoesPathExist(lua_State* L);
	int Unit_DoesPathExistPos(lua_State* L);
	int Unit_GetCurrentMovementGeneratorType(lua_State* L);
	int Unit_GetSpeedRate(lua_State* L);
	int Unit_GetStandState(lua_State* L);
	int Unit_IsMounted(lua_State* L);
	int Unit_IsMoving(lua_State* L);
	int Unit_IsStopped(lua_State* L);
	int Unit_MonsterMove(lua_State* L);
	int Unit_MotionMasterClear(lua_State* L);
	int Unit_MoveFollow(lua_State* L);
	int Unit_MoveChase(lua_State* L);
	int Unit_MoveIdle(lua_State* L);
	int Unit_MovePoint(lua_State* L);
	int Unit_SetStandState(lua_State* L);
	int Unit_StopMoving(lua_State* L);
	int Unit_UpdateSpeed(lua_State* L);

	// position related
	int Unit_GetAbsoluteAngle(lua_State* L);
	int Unit_GetAngle(lua_State* L);
	int Unit_GetDistance(lua_State* L);
	int Unit_GetDistanceToPos(lua_State* L);
	int Unit_GetExactDist(lua_State* L);
	int Unit_GetForwardVector(lua_State* L);
	int Unit_GetGroundHeight(lua_State* L);
	int Unit_GetMapId(lua_State* L);
	int Unit_GetNearPoint(lua_State* L);
	int Unit_GetNearPoint2D(lua_State* L);
	int Unit_GetNearPointAroundPosition(lua_State* L);
	int Unit_GetOrientation(lua_State* L);
	int Unit_SetOrientation(lua_State* L);
	int Unit_GetPosition(lua_State* L);
	int Unit_GetRelativeAngle(lua_State* L);
	int Unit_GetZoneId(lua_State* L);
	int Unit_IsInWorld(lua_State* L);
	int Unit_IsWithinLOS(lua_State* L);
	int Unit_IsWithinLOSInMap(lua_State* L);
	int Unit_SetFacingTo(lua_State* L);
	int Unit_SetFacingToObject(lua_State* L);
	int Unit_ToAbsoluteAngle(lua_State* L);

	// death related
	int Unit_GetDeathState(lua_State* L);
	int Unit_IsAlive(lua_State* L);
	int Unit_IsDead(lua_State* L);

	static const struct luaL_Reg Unit_BindLib[]{
		{"Print", Unit_Print},

		{"GetAI", Unit_GetAI},
		// combat related
		{"AddAura", Unit_AddAura},
		{"AddThreat", Unit_AddThreat},
		{"CalculateSpellEffect", Unit_CalculateSpellEffect},
		{"CastSpell", Unit_CastSpell},
		{"ClearTarget", Unit_ClearTarget},

		{"GetAttackersTbl", Unit_GetAttackersTbl},
		{"GetAttackRange", Unit_GetAttackRange},
		{"GetAttackTimer", Unit_GetAttackTimer},
		{"GetAuraStacks", Unit_GetAuraStacks},
		{"GetAuraDuration", Unit_GetAuraDuration},
		{"GetCombatDistance", Unit_GetCombatDistance},
		{"GetCombatReach", Unit_GetCombatReach},
		{"GetCurrentSpellId", Unit_GetCurrentSpellId},
		{"GetEnemyCountInRadiusAround", Unit_GetEnemyCountInRadiusAround},

		{"GetHealth", Unit_GetHealth},
		{"GetHealthPercent", Unit_GetHealthPercent},
		{"GetMaxHealth", Unit_GetMaxHealth},
		{"GetMaxPower", Unit_GetMaxPower},

		{"GetMeleeReach", Unit_GetMeleeReach},
		{"GetMeleeRange", Unit_GetMeleeRange},
		{"GetPet", Unit_GetPet},
		{"GetPower", Unit_GetPower},
		{"GetPowerPercent", Unit_GetPowerPercent},
		//{"GetSpellCD", Unit_GetSpellCD},
		{"GetShapeshiftForm", Unit_GetShapeshiftForm},
		{"GetSpellCost", Unit_GetSpellCost},
		{"GetThreat", Unit_GetThreat},
		{"GetThreatTbl", Unit_GetThreatTbl},
		{"HasTotem", Unit_HasTotem},
		{"GetVictim", Unit_GetVictim},
		{"GetVictimsInRange", Unit_GetVictimsInRange},
		{"GetVictimNearest", Unit_GetVictimNearest},

		{"HasAura", Unit_HasAura},
		{"HasAuraBy", Unit_HasAuraBy},
		{"HasAuraType", Unit_HasAuraType},
		{"HasObjInArc", Unit_HasObjInArc},
		{"HasPosInArc", Unit_HasPosInArc},

		{"InterruptSpell", Unit_InterruptSpell},

		{"IsInCombat", Unit_IsInCombat},
		{"IsNonMeleeSpellCasted", Unit_IsNonMeleeSpellCasted},
		{"IsCastingInterruptableSpell", Unit_IsCastingInterruptableSpell},
		{"IsSpellReady", Unit_IsSpellReady},
		{"IsTargetInRangeOfSpell", Unit_IsTargetInRangeOfSpell},
		{"IsValidHostileTarget", Unit_IsValidHostileTarget},

		{"PetAttack", Unit_PetAttack},
		{"PetCast", Unit_PetCast},
		{"PetAutocast", Unit_PetAutocast},
		{"PetCasterChaseDist", Unit_PetCasterChaseDist},

		{"RemoveAura", Unit_RemoveAura},
		{"RemoveAurasByCasterSpell", Unit_RemoveAurasByCasterSpell},
		{"RemoveAuraByCancel", Unit_RemoveAuraByCancel},
		{"RemoveSpellCooldown", Unit_RemoveSpellCooldown},
		{"RemoveSpellsCausingAura", Unit_RemoveSpellsCausingAura},

		{"SetHealth", Unit_SetHealth},
		{"SetHealthPercent", Unit_SetHealthPercent},
		{"SetMaxHealth", Unit_SetMaxHealth},
		{"SetMaxPower", Unit_SetMaxPower},
		{"SetPower", Unit_SetPower},
		{"SetPowerPercent", Unit_SetPowerPercent},

		//caster chase
		{"HasDistanceCasterMovement", Unit_HasDistanceCasterMovement},
		{"SetCasterChaseDistance", Unit_SetCasterChaseDistance},

		//gen info
		{"ClearUnitState", Unit_ClearUnitState},
		{"GetClass", Unit_GetClass},
		{"GetDisplayId", Unit_GetDisplayId},
		{"GetEntry", Unit_GetEntry},
		{"GetLevel", Unit_GetLevel},
		{"GetNativeDisplayId", Unit_GetNativeDisplayId},
		{"GetName", Unit_GetName},
		{"GetObjectGuid", Unit_GetObjectGuid},
		{"GetObjectBoundingRadius", Unit_GetObjectBoundingRadius},
		{"GetPowerType", Unit_GetPowerType},
		{"GetRace", Unit_GetRace},
		{"GetRank", Unit_GetRank},
		{"GetTargetGuid", Unit_GetTargetGuid},
		{"HasUnitState", Unit_HasUnitState},
		{"IsInDisallowedMountForm", Unit_IsInDisallowedMountForm},
		{"IsInDungeon", Unit_IsInDungeon},
		{"IsPlayer", Unit_IsPlayer},

		// movement related
		{"DoesPathExist", Unit_DoesPathExist},
		{"DoesPathExistPos", Unit_DoesPathExistPos},
		{"GetCurrentMovementGeneratorType", Unit_GetCurrentMovementGeneratorType},
		{"GetStandState", Unit_GetStandState},
		{"GetSpeedRate", Unit_GetSpeedRate},
		{"IsMounted", Unit_IsMounted},
		{"IsMoving", Unit_IsMoving},
		{"IsStopped", Unit_IsStopped},
		{"MonsterMove", Unit_MonsterMove},
		{"MotionMasterClear", Unit_MotionMasterClear},
		{"MoveChase", Unit_MoveChase},
		{"MoveFollow", Unit_MoveFollow},
		{"MoveIdle", Unit_MoveIdle},
		{"MovePoint", Unit_MovePoint},
		{"SetStandState", Unit_SetStandState},
		{"StopMoving", Unit_StopMoving},
		{"UpdateSpeed", Unit_UpdateSpeed},

		// position related
		{"GetAbsoluteAngle", Unit_GetAbsoluteAngle},
		{"GetAngle", Unit_GetAngle},
		{"GetDistance", Unit_GetDistance},
		{"GetDistanceToPos", Unit_GetDistanceToPos},
		{"GetExactDist", Unit_GetExactDist},
		{"GetForwardVector", Unit_GetForwardVector},
		{"GetGroundHeight", Unit_GetGroundHeight},
		{"GetMapId", Unit_GetMapId},
		{"GetNearPoint", Unit_GetNearPoint},
		{"GetNearPoint2D", Unit_GetNearPoint2D},
		{"GetNearPointAroundPosition", Unit_GetNearPointAroundPosition},
		{"GetOrientation", Unit_GetOrientation},
		{"GetPosition", Unit_GetPosition},
		{"GetRelativeAngle", Unit_GetRelativeAngle},
		{"IsWithinLOS", Unit_IsWithinLOS},
		{"IsWithinLOSInMap", Unit_IsWithinLOSInMap},
		{"IsInWorld", Unit_IsInWorld},
		{"SetFacingTo", Unit_SetFacingTo},
		{"SetFacingToObject", Unit_SetFacingToObject},
		{"SetOrientation", Unit_SetOrientation},
		{"ToAbsoluteAngle", Unit_ToAbsoluteAngle},
		{"GetZoneId", Unit_GetZoneId},

		// death related
		{"GetDeathState", Unit_GetDeathState},
		{"IsAlive", Unit_IsAlive},
		{"IsDead", Unit_IsDead},

		{NULL, NULL}
	};

}


#endif