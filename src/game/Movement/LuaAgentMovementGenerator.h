/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOS_LUAAGENTMOVEMENTGENERATOR_H
#define MANGOS_LUAAGENTMOVEMENTGENERATOR_H

#include "MovementGenerator.h"
#include "FollowerReference.h"
#include "PathFinder.h"
#include "Unit.h"
#include "TargetedMovementGenerator.h"

template<class T, typename D>
class LuaAITargetedMovementGeneratorMedium
: public MovementGeneratorMedium< T, D >, public TargetedMovementGeneratorBase
{
    protected:
        LuaAITargetedMovementGeneratorMedium(Unit &target, float offset, float angle) :
            TargetedMovementGeneratorBase(target), m_checkDistanceTimer(0), m_fOffset(offset),
            m_fAngle(angle), m_bRecalculateTravel(false), m_bTargetReached(false),
            m_bReachable(true), m_fTargetLastX(0), m_fTargetLastY(0), m_fTargetLastZ(0), m_bTargetOnTransport(false)
        {
        }
        ~LuaAITargetedMovementGeneratorMedium() {}

    public:
        
        void UpdateAsync(T&, uint32 diff);

        bool IsReachable() const
        {
            return m_bReachable;
        }

        Unit* GetTarget() const { return i_target.getTarget(); }

        void UnitSpeedChanged() { m_bRecalculateTravel=true; }
        void UpdateFinalDistance(float fDistance);

    protected:
        void _setTargetLocation(T &);

        ShortTimeTracker m_checkDistanceTimer;

        float m_offsetMin{0.f};
        float m_offsetMax{0.f};
        bool m_bIsRanged{false};
        bool m_noMinOffsetIfMutual{false};
        bool m_bUseAngle{true};
        bool m_bUseAbsAngle{false};
        float m_fAbsAngle{0.f};
        float m_fOffset;
        float m_fAngle;
        bool m_bRecalculateTravel : 1;
        bool m_bTargetReached : 1;
        bool m_bReachable;

        float m_fTargetLastX;
        float m_fTargetLastY;
        float m_fTargetLastZ;
        bool  m_bTargetOnTransport;
};

template<class T>
class LuaAIChaseMovementGenerator : public LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >
{
    public:
        LuaAIChaseMovementGenerator(Unit &target, float offset, float offsetMin, float offsetMax, float angle, float angleT, bool noMinOffsetIfMutual, bool useAngle, bool isRanged)
            : LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >(target, offset, angle),
            m_angleT(angleT)
        {
            m_offsetMax = offsetMax != .0f ? offsetMax : .2f;
            m_offsetMin = offsetMin != 0.f ? offsetMin : .2f;
            m_noMinOffsetIfMutual = noMinOffsetIfMutual;
            m_bUseAngle = useAngle;
            m_bIsRanged = isRanged;
        }
        ~LuaAIChaseMovementGenerator() noexcept {}

        float GetOffset() { return m_fOffset; }
        float GetOffsetMin() { return m_offsetMin; }
        float GetOffsetMax() { return m_offsetMax; }
        void SetOffset(float v) { m_fOffset = v; }
        void SetOffsetMin(float v) { m_offsetMin = v; }
        void SetOffsetMax(float v) { m_offsetMax = v; }
        float GetAngle() { return m_fAngle; }
        float GetAngleT() { return m_angleT; }
        void SetAngle(float v) { m_fAngle = v; }
        void SetAngleT(float v) { m_angleT = v; }

        bool IsUsingAbsAngle() { return m_bUseAbsAngle; }
        void UseAbsAngle(float A) { m_fAbsAngle = A; m_bUseAbsAngle = true; }
        void RemoveAbsAngle() { m_bUseAbsAngle = false; }
        bool IsUsingAngle() { return m_bUseAngle; }
        void SetUseAngle(bool v) { m_bUseAngle = v; }

        MovementGeneratorType GetMovementGeneratorType() const { return CHASE_MOTION_TYPE; }

        bool Update(T &, uint32 const&);
        void Initialize(T &);
        void Finalize(T &);
        void Interrupt(T &);
        void Reset(T &);
        void MovementInform(T &);

        static void _clearUnitStateMove(T &u) { u.ClearUnitState(UNIT_STAT_CHASE_MOVE); }
        static void _addUnitStateMove(T &u)  { u.AddUnitState(UNIT_STAT_CHASE_MOVE); }
        bool EnableWalking() const { return false;}
        bool _lostTarget(T& u) const { return !GetTarget()->IsAlive();/*u.GetVictim() != this->GetTarget();*/ }
        void _reachTarget(T &);
    private:
        float m_angleT;

        bool IsAngleBad(T& owner, bool mutualChase);
        bool IsDistBad(T& owner, bool mutualChase);

        // Needed to compile with gcc for some reason.
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::i_target;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_fAngle;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_fOffset;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_fTargetLastX;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_fTargetLastY;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_fTargetLastZ;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_checkDistanceTimer;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_bTargetOnTransport;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_bRecalculateTravel;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_bTargetReached;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_bUseAbsAngle;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_fAbsAngle;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_offsetMin;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_offsetMax;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_noMinOffsetIfMutual;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIChaseMovementGenerator<T> >::m_bUseAngle;
};

template<class T>
class LuaAIFollowMovementGenerator : public LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >
{
    public:
        explicit LuaAIFollowMovementGenerator(Unit &target)
            : LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >(target){}
        LuaAIFollowMovementGenerator(Unit &target, float offset, float angle)
            : LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >(target, offset, angle) {}
        ~LuaAIFollowMovementGenerator() {}

        MovementGeneratorType GetMovementGeneratorType() const { return FOLLOW_MOTION_TYPE; }

        bool Update(T &, uint32 const&);
        void Initialize(T &);
        void Finalize(T &);
        void Interrupt(T &);
        void Reset(T &);
        void MovementInform(T &);

        static void _clearUnitStateMove(T &u) { u.ClearUnitState(UNIT_STAT_FOLLOW_MOVE); }
        static void _addUnitStateMove(T &u)  { u.AddUnitState(UNIT_STAT_FOLLOW_MOVE); }
        bool EnableWalking() const;
        bool _lostTarget(T &) const { return false; }
        void _reachTarget(T &) {}
    private:
        void _updateSpeed(T &u);

        // Needed to compile with gcc for some reason.
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::i_target;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_fAngle;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_fOffset;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_fTargetLastX;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_fTargetLastY;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_fTargetLastZ;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_checkDistanceTimer;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_bTargetOnTransport;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_bRecalculateTravel;
        using LuaAITargetedMovementGeneratorMedium<T, LuaAIFollowMovementGenerator<T> >::m_bTargetReached;
};

#endif
