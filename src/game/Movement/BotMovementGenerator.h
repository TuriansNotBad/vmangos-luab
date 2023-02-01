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

#ifndef MANGOS_BOTMOVEMENTGENERATOR_H
#define MANGOS_BOTMOVEMENTGENERATOR_H

#include "MovementGenerator.h"
#include "FollowerReference.h"
#include "PathFinder.h"
#include "Unit.h"
#include "TargetedMovementGenerator.h"

template<class T, typename D>
class BotTargetedMovementGeneratorMedium
: public MovementGeneratorMedium< T, D >, public TargetedMovementGeneratorBase
{
    protected:
        BotTargetedMovementGeneratorMedium(Unit &target, float offset, float angle) :
            TargetedMovementGeneratorBase(target), m_checkDistanceTimer(0), m_fOffset(offset),
            m_fAngle(angle), m_bRecalculateTravel(false), m_bTargetReached(false),
            m_bReachable(true), m_fTargetLastX(0), m_fTargetLastY(0), m_fTargetLastZ(0), m_bTargetOnTransport(false)
        {
        }
        ~BotTargetedMovementGeneratorMedium() {}

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
class BotChaseMovementGenerator : public BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >
{
    public:
        explicit BotChaseMovementGenerator(Unit &target)
            : BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >(target) {}
        BotChaseMovementGenerator(Unit &target, float offset, float angle, float angleT = 15.0f)
            : BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >(target, offset, angle),
            m_angleT(angleT) {}
        ~BotChaseMovementGenerator() {}

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
        bool _lostTarget(T &u) const { return u.GetVictim() != this->GetTarget(); }
        void _reachTarget(T &);
    private:
        ShortTimeTracker m_spreadTimer{ 0 };
        ShortTimeTracker m_leashExtensionTimer{ 5000 };
        bool m_bIsSpreading = false;
        bool m_bCanSpread = true;
        uint8 m_uiSpreadAttempts = 0;
        float m_angleT;

        void DoBackMovement(T &, Unit* target);
        void DoSpreadIfNeeded(T &, Unit* target);
        bool TargetDeepInBounds(T &, Unit* target) const;
        bool TargetWithinBoundsPercentDistance(T &, Unit* target, float pct) const;

        // Needed to compile with gcc for some reason.
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::i_target;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_fAngle;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_fOffset;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_fTargetLastX;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_fTargetLastY;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_fTargetLastZ;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_checkDistanceTimer;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_bTargetOnTransport;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_bRecalculateTravel;
        using BotTargetedMovementGeneratorMedium<T, BotChaseMovementGenerator<T> >::m_bTargetReached;
};

template<class T>
class BotFollowMovementGenerator : public BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >
{
    public:
        explicit BotFollowMovementGenerator(Unit &target)
            : BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >(target){}
        BotFollowMovementGenerator(Unit &target, float offset, float angle)
            : BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >(target, offset, angle) {}
        ~BotFollowMovementGenerator() {}

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
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::i_target;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_fAngle;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_fOffset;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_fTargetLastX;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_fTargetLastY;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_fTargetLastZ;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_checkDistanceTimer;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_bTargetOnTransport;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_bRecalculateTravel;
        using BotTargetedMovementGeneratorMedium<T, BotFollowMovementGenerator<T> >::m_bTargetReached;
};

template<class T>
class BotPointMovementGenerator
    : public MovementGeneratorMedium< T, BotPointMovementGenerator<T> >
{
public:
    BotPointMovementGenerator(uint32 _id, float _x, float _y, float _z, uint32 options, float speed = 0.0f, float finalOrientation = -10.0f) :
        m_id(_id), m_x(_x), m_y(_y), m_z(_z), m_o(finalOrientation), m_options(options), m_speed(speed), m_recalculateSpeed(false) {}
    virtual ~BotPointMovementGenerator() {}

    virtual void Initialize(T&);
    virtual void Finalize(T&);
    void Interrupt(T&);
    void Reset(T& unit);
    bool Update(T&, uint32 const& diff);
    void UpdateAsync(T&, uint32 diff);

    void ChangeDestination(T&, float x, float y, float z);
    uint32 GetOptions() { return m_options; }

    virtual void MovementInform(T&);

    void UnitSpeedChanged() override { m_recalculateSpeed = true; }

    MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }

    bool GetDestination(float& x, float& y, float& z) const { x = m_x; y = m_y; z = m_z; return true; }
protected:
    uint32 m_id;
    float m_x, m_y, m_z, m_o;
    uint32 m_options;
    float m_speed;
    bool m_recalculateSpeed;
};



#endif
