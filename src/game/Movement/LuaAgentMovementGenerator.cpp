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

#include "ByteBuffer.h"
#include "LuaAgentMovementGenerator.h"
#include "Errors.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Player.h"
#include "World.h"
#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "Anticheat.h"
#include "Transport.h"
#include "TemporarySummon.h"
#include "GameObjectAI.h"
#include "Geometry.h"
#include "LuaAI/LuaAgent.h"
#include "Spell.h"
#include "ObjectPosSelector.h"
#include "CellImpl.h"
#include "GridNotifiers.h"

namespace
{

    bool IsFalling(Player* owner)
    {
        return owner && owner->IsLuaAgent() && owner->GetLuaAI() && owner->GetLuaAI()->IsFalling();
    }

    class NearUsedPosDo
    {
    public:
        NearUsedPosDo(WorldObject const& obj, float objX, float objY, WorldObject const* searcher, float angle, ObjectPosSelector& selector)
            : i_object(obj), i_objectX(objX), i_objectY(objY), i_searcher(searcher), i_angle(angle), i_selector(selector) {}

        void operator()(Corpse*) const {}
        void operator()(DynamicObject*) const {}

        void operator()(Unit* c) const {}
        void operator()(Creature* c) const {}
        void operator()(Player* c) const {}

        template<class T>
        void operator()(T* u) const
        {
            // skip self or target
            if (u == i_searcher || u == &i_object)
                return;

            float x, y;

            x = u->GetPositionX();
            y = u->GetPositionY();

            add(u, x, y);
        }

        // we must add used pos that can fill places around center
        void add(WorldObject* u, float x, float y) const
        {
            // u is too nearest/far away to i_object
            if (!Geometry::IsInRange2D(i_objectX, i_objectY, x, y, i_selector.m_dist - i_selector.m_size + i_object.GetObjectBoundingRadius(), i_selector.m_dist + i_selector.m_size + i_object.GetObjectBoundingRadius()))
                return;

            float angle = Geometry::GetAngle(i_objectX, i_objectY, u->GetPositionX(), u->GetPositionY()) - i_angle;

            // move angle to range -pi ... +pi
            angle = MapManager::NormalizeOrientation(angle);

            // dist include size of u
            float dist2d = std::max(Geometry::GetDistance2D(i_objectX, i_objectY, x, y) - i_object.GetObjectBoundingRadius(), 0.0f);
            i_selector.AddUsedPos(u->GetObjectBoundingRadius(), angle, dist2d + i_object.GetObjectBoundingRadius());
        }
    private:
        WorldObject const& i_object;
        float i_objectX;
        float i_objectY;
        WorldObject const* i_searcher;
        float              i_angle;
        ObjectPosSelector& i_selector;
    };

    void GetNearPointAroundPositionA(WorldObject& me, WorldObject const* searcher, float& x, float& y, float& z, float searcher_bounding_radius, float distance2d, float absAngle)
    {
        float startX = x;
        float startY = y;
        float startZ = z;

        me.GetNearPoint2DAroundPosition(startX, startY, x, y, distance2d + searcher_bounding_radius, absAngle);

        // if detection disabled, return first point
        if (!sWorld.getConfig(CONFIG_BOOL_DETECT_POS_COLLISION))
        {
            if (searcher)
                searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
            else
                me.UpdateGroundPositionZ(x, y, z);
            return;
        }

        // or remember first point
        float first_x = x;
        float first_y = y;
        bool first_los_conflict = false;                        // first point LOS problems

        // prepare selector for work
        ObjectPosSelector selector(startX, startY, me.GetObjectBoundingRadius(), distance2d + searcher_bounding_radius);

        // adding used positions around object
        {
            NearUsedPosDo u_do(me, startX, startY, searcher, absAngle, selector);
            MaNGOS::WorldObjectWorker<NearUsedPosDo> worker(u_do);

            Cell::VisitAllObjects(&me, worker, distance2d);
        }

        // maybe can just place in primary position
        if (selector.CheckOriginal())
        {
            if (searcher)
                searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
            else
                me.UpdateGroundPositionZ(x, y, z);

            if (me.IsWithinLOSAtPosition(startX, startY, startZ, x, y, z))
                return;

            first_los_conflict = true;                          // first point have LOS problems
        }

        float angle;                                            // candidate of angle for free pos

        // special case when one from list empty and then empty side preferred
        if (selector.FirstAngle(angle))
        {
            me.GetNearPoint2DAroundPosition(startX, startY, x, y, distance2d, absAngle + angle);
            z = startZ;

            if (searcher)
                searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
            else
                me.UpdateGroundPositionZ(x, y, z);

            if (me.IsWithinLOSAtPosition(startX, startY, startZ, x, y, z))
                return;
        }

        // set first used pos in lists
        selector.InitializeAngle();

        // select in positions after current nodes (selection one by one)
        while (selector.NextAngle(angle))                       // angle for free pos
        {
            me.GetNearPoint2DAroundPosition(startX, startY, x, y, distance2d, absAngle + angle);
            z = startZ;

            if (searcher)
                searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
            else
                me.UpdateGroundPositionZ(x, y, z);

            if (me.IsWithinLOSAtPosition(startX, startY, startZ, x, y, z))
                return;
        }

        // BAD NEWS: not free pos (or used or have LOS problems)
        // Attempt find _used_ pos without LOS problem

        if (!first_los_conflict)
        {
            x = first_x;
            y = first_y;

            if (searcher)
                searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
            else
                me.UpdateGroundPositionZ(x, y, z);

            return;
        }

        // special case when one from list empty and then empty side preferred
        if (selector.IsNonBalanced())
        {
            if (!selector.FirstAngle(angle))                    // _used_ pos
            {
                me.GetNearPoint2DAroundPosition(startX, startY, x, y, distance2d, absAngle + angle);
                z = startZ;

                if (searcher)
                    searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
                else
                    me.UpdateGroundPositionZ(x, y, z);

                if (me.IsWithinLOSAtPosition(startX, startY, startZ, x, y, z))
                    return;
            }
        }

        // set first used pos in lists
        selector.InitializeAngle();

        // select in positions after current nodes (selection one by one)
        while (selector.NextUsedAngle(angle))                   // angle for used pos but maybe without LOS problem
        {
            me.GetNearPoint2DAroundPosition(startX, startY, x, y, distance2d, absAngle + angle);
            z = startZ;

            if (searcher)
                searcher->UpdateAllowedPositionZ(x, y, z);      // update to LOS height if available
            else
                me.UpdateGroundPositionZ(x, y, z);

            if (me.IsWithinLOSAtPosition(startX, startY, startZ, x, y, z))
                return;
        }

        // BAD BAD NEWS: all found pos (free and used) have LOS problem :(
        x = first_x;
        y = first_y;

        if (searcher)
            searcher->UpdateAllowedPositionZ(x, y, z);          // update to LOS height if available
        else
            me.UpdateGroundPositionZ(x, y, z);
    }

    float GetPointOnSegmentAtDFrom(Vector3& start, const Vector3& end, const Vector3& dest, const Vector3& unitDir, float segLen, float maxd)
    {
        float e = (end.y - start.y)/(end.x - start.x);
        float f = start.y - e* start.x;

        float a = 1 + e * e;
        float b = 2 * e * f - 2 * dest.x - 2 * e * dest.y;
        float c = dest.x * dest.x + f * f - 2 * dest.y * f + dest.y * dest.y - maxd * maxd;

        // 2 possible points
        float x1, x2, y1, y2, disc;
        disc = sqrt(b * b - 4 * a * c);
        x1 = (-b + disc) / 2 / a;
        y1 = e * x1 + f;
        x2 = (-b - disc) / 2 / a;
        y2 = e * x2 + f;

        // result is the point that lies on the [start,end] segment
        float segLenSqr = segLen * segLen;
        float segLen1Sqr = (x1 - start.x) * (x1 - start.x) + (y1 - start.y) * (y1 - start.y);
        float segLen2Sqr = (x1 - end.x) * (x1 - end.x) + (y1 - end.y) * (y1 - end.y);
        if (segLen1Sqr < segLenSqr && segLen2Sqr < segLenSqr)
        {
            start.x = x1;
            start.y = y1;
        }
        else
        {
            start.x = x2;
            start.y = y2;
            segLen1Sqr = (x2 - start.x) * (x2 - start.x) + (y2 - start.y) * (y2 - start.y);
        }
        start.z += unitDir.z * (segLen * (segLen1Sqr / segLenSqr));
        return segLen * (segLen1Sqr / segLenSqr);
    }

    void Path_CutPathToD(PathInfo& path, WorldObject* target, float d)
    {
        if (path.getPathType() & PathType::PATHFIND_NOPATH)
            return;

        Movement::PointsArray& points = const_cast<Movement::PointsArray&>(path.getPath());
        Vector3 dest(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
        // search for the cut position
        for (size_t i = 1; i < points.size(); ++i)
        {
            if (target->IsWithinDist3d(points[i].x, points[i].y, points[i].z, d, SizeFactor::None) &&
                target->IsWithinLOS(points[i].x, points[i].y, points[i].z))
            {
                Vector3 startPoint = points[i - 1];
                Vector3 endPoint = points[i];
                Vector3 dirVect = endPoint - startPoint;
                float targetDist1 = target->GetDistance(points[i].x, points[i].y, points[i].z, SizeFactor::None);
                float targetDist2 = target->GetDistance(points[i - 1].x, points[i - 1].y, points[i - 1].z, SizeFactor::None);
                if ((targetDist2 > targetDist1) && (targetDist2 > d))
                {
                    float directionLength = dirVect.length();
                    float step = .5f;
                    //float curD = step;
                    Vector3 dir = dirVect / directionLength;
                    float curD = GetPointOnSegmentAtDFrom(startPoint, endPoint, dest, dir, directionLength, d);
                    dir *= step;

                    while (curD < directionLength)
                    {
                        if (target->IsWithinDist3d(startPoint.x, startPoint.y, startPoint.z, d, SizeFactor::None) &&
                            target->IsWithinLOS(startPoint.x, startPoint.y, startPoint.z))
                        {
                            points[i] = startPoint;
                            points.resize(i + 1);
                            return;
                        }
                        startPoint += dir;
                        curD += step;
                    }
                }

                if (target->IsWithinDist3d(startPoint.x, startPoint.y, startPoint.z, d, SizeFactor::None) &&
                    target->IsWithinLOS(startPoint.x, startPoint.y, startPoint.z))
                    points[i] = startPoint;
                points.resize(i + 1);
                return;
            }
        }
    }

    void Path_CutPathWithDynamicLoS(PathInfo& path, Unit* unit)
    {
        if (path.getPathType() & PathType::PATHFIND_NOPATH)
            return;

        Movement::PointsArray& points = const_cast<Movement::PointsArray&>(path.getPath());
        uint32 maxIndex = points.size() - 1;
        Vector3 out;
        // We have always keep at least 2 points (else, there is no mvt !)
        for (uint32 i = 1; i <= maxIndex; ++i)
        {
            Vector3 a(points[i-1]);
            a.z += 1.f;
            Vector3 b(points[i]);
            b.z += 1.f;
            if (unit->GetMap()->GetDynamicObjectHitPos(a, b, out, -0.1f))
            {
                out.z -= 1.f;
                points[i] = out;
                points.resize(i + 1);
                break;
            }
        }
    }
}

//-----------------------------------------------//
template<class T, typename D>
void LuaAITargetedMovementGeneratorMedium<T, D>::_setTargetLocation(T &owner)
{
    // Note: Any method that accesses the target's movespline here must be
    // internally locked by the target's spline lock
    if (!i_target.isValid() || !i_target->IsInWorld())
        return;

    if (owner.HasUnitState(UNIT_STAT_NO_FREE_MOVE | UNIT_STAT_POSSESSED) || IsFalling(owner.ToPlayer()))
        return;

    float x, y, z;
    bool losChecked = false;
    bool losResult = false;

    GenericTransport* transport = owner.GetTransport();

    // Can switch transports during follow movement.
    if (this->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE)
    {
        transport = i_target.getTarget()->GetTransport();

        if (transport != owner.GetTransport())
        {
            if (owner.GetTransport())
                owner.GetTransport()->RemoveFollowerFromTransport(i_target.getTarget(), &owner);

            if (transport)
                transport->AddFollowerToTransport(i_target.getTarget(), &owner);
        }
    }

    m_bTargetOnTransport = i_target.getTarget()->GetTransport();
    i_target->GetPosition(m_fTargetLastX, m_fTargetLastY, m_fTargetLastZ, i_target.getTarget()->GetTransport());

    // Can't path to target if transports are still different.
    if (owner.GetTransport() != i_target.getTarget()->GetTransport())
    {
        m_bReachable = false;
        return;
    }

    if (!m_fOffset)
    {
        if (owner.CanReachWithMeleeAutoAttack(i_target.getTarget()))
        {
            losResult = owner.IsWithinLOSInMap(i_target.getTarget());
            losChecked = true;
            if (losResult)
                return;
        }

        // NOSTALRIUS: Eviter les collisions entre mobs.
        // Cette fonction prend un angle aleatoire.
        if (!i_target->GetRandomAttackPoint(&owner, x, y, z))
        {
            m_bReachable = false;
            return;
        }
    }
    else
    {
        // to at m_fOffset distance from target and m_fAngle from target facing
        float srcX, srcY, srcZ;
        i_target->GetSafePosition(srcX, srcY, srcZ, transport);
        if (transport)
            transport->CalculatePassengerPosition(srcX, srcY, srcZ);

        // TRANSPORT_VMAPS
        if (transport)
        {
            i_target->GetNearPoint2D(x, y, m_fOffset, m_fAngle + i_target->GetOrientation());
            z = srcZ;
        }
        else
        {
            float o;
            if (!(sWorld.getConfig(CONFIG_BOOL_ENABLE_MOVEMENT_EXTRAPOLATION_PET) &&
                  i_target->ExtrapolateMovement(i_target->m_movementInfo, (WorldTimer::getMSTime() - i_target->m_movementInfo.stime) + 500, x, y, z, o)))
            {
                i_target->GetPosition(x, y, z);
                o = i_target->GetOrientation();
            }
            
            if (GetMovementGeneratorType() == CHASE_MOTION_TYPE)
            {
                bool bSimpleChase = false;
                if (Player* pOwner = owner.ToPlayer())
                    if (LuaAgent* agent = pOwner->GetLuaAI())
                        bSimpleChase = agent->IsSimpleChaseMode();

                float angle;
                if (m_bUseAngle && !bSimpleChase)
                {
                    angle = m_bUseAbsAngle ? m_fAbsAngle : i_target->GetOrientation();
                    if (!m_bUseAbsAngle && (!i_target->GetVictim() || i_target->GetVictim()->GetObjectGuid() != owner.GetObjectGuid()))
                        angle += m_fAngle;
                }
                else
                    angle = i_target->GetAngle(&owner);
                if (!m_bIsRanged)
                {
                    float offset;
                    float combinedBoundingRadius = i_target->GetObjectBoundingRadius() + owner.GetObjectBoundingRadius();
                    float D = i_target->GetDistance(&owner, SizeFactor::None);

                    float dMin = m_fOffset + combinedBoundingRadius - m_offsetMin;
                    float dMax = m_fOffset + combinedBoundingRadius + m_offsetMax;

                    if (dMin < 0.00001f)
                        dMin = 0.00001f;
                    // closest allowed if too close, offset if too far, and same distance if correcting angle only
                    if (D < dMin)
                        offset = m_fOffset - m_offsetMin + .1f;
                    else if (D > dMax)
                        offset = m_fOffset;
                    else
                        offset = D;

                    GetNearPointAroundPositionA(*i_target.getTarget(), &owner, x, y, z, owner.GetObjectBoundingRadius(), offset, angle);
                }
            }
            else
                i_target->GetNearPointAroundPosition(&owner, x, y, z, owner.GetObjectBoundingRadius(), m_fOffset, o + m_fAngle);
        }

        if (!i_target->m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) && !i_target->IsInWater())
            if (!owner.GetMap()->GetWalkHitPosition(transport, srcX, srcY, srcZ, x, y, z))
                i_target->GetSafePosition(x, y, z);
    }

    PathFinder path(&owner);
    
    // allow pets following their master to cheat while generating paths
    bool petFollowing = owner.HasUnitState(UNIT_STAT_FOLLOW);
    Movement::MoveSplineInit init(owner, "TargetedMovementGenerator");
    path.SetTransport(transport);
    path.calculate(x, y, z, petFollowing);

    PathType pathType = path.getPathType();
    m_bReachable = pathType & (PATHFIND_NORMAL | PATHFIND_DEST_FORCED);

    if (false) {
        float ax, ay, az, px, py, pz;
        path.getActualEndPosition(ax, ay, az);
        path.getEndPosition(px, py, pz);
        float d2a = Geometry::GetDistance3D(ax, ay, az, px, py, pz);
        float d2e = Geometry::GetDistance3D(owner.GetPosition(), Vector3(x, y, z));
        printf("%s: Path L,T (%.2f, %d) x,y,z (%.2f,%.2f,%.2f) path a/f (%.2f,%.2f,%.2f) (%.2f,%.2f,%.2f) D(%.2f,%.2f) %.2f\n",
            owner.GetName(), path.Length(), int(pathType), x, y, z, ax, ay, az, px, py, pz, d2a, d2e, i_target->GetAngle(&owner));
        auto entry = sSpellMgr.GetSpellEntry(19821);
        Spell* spell = new Spell(&owner, entry, true);
        SpellCastTargets targets;
        targets.setDestination(x, y, z);
        spell->prepare(std::move(targets));
        owner.RemoveSpellCooldown(*entry, false);
    }
    
    if (!petFollowing)
    {
        if (pathType == PATHFIND_NOPATH)
            return;

        // prevent pets from going through closed doors
        Path_CutPathWithDynamicLoS(path, &owner);
        if (path.getPath().size() == 2 && path.Length() < 0.1f)
        {
            m_bReachable = false;
            return;
        }
    }

    if (!m_bReachable && !!(pathType & PATHFIND_INCOMPLETE) && owner.HasUnitState(UNIT_STAT_ALLOW_INCOMPLETE_PATH))
        m_bReachable = true;

    m_bRecalculateTravel = false;
    if (this->GetMovementGeneratorType() == CHASE_MOTION_TYPE && !transport && m_bIsRanged)
        Path_CutPathToD(path, i_target.getTarget(), m_fOffset + i_target->GetObjectBoundingRadius() + owner.GetObjectBoundingRadius());

    // Try to prevent redundant micro-moves
    float pathLength = path.Length();
    if (pathLength < 0.4f ||
            (pathLength < 4.0f && (i_target->GetPositionZ() - owner.GetPositionZ()) > 10.0f) || // He is flying too high for me. Moving a few meters wont change anything.
            (pathType & PATHFIND_NOPATH && !petFollowing) ||
            (pathType & PATHFIND_INCOMPLETE && !owner.HasUnitState(UNIT_STAT_ALLOW_INCOMPLETE_PATH) && !petFollowing) ||
            (!petFollowing && !m_bReachable && !(owner.IsPlayer() && owner.HasUnitState(UNIT_STAT_FOLLOW))))
    {
        if (!losChecked)
            losResult = owner.IsWithinLOSInMap(i_target.getTarget());
        if (losResult)
        {
            if (!owner.movespline->Finalized())
                owner.StopMoving();
            return;
        }
    }

    D::_addUnitStateMove(owner);
    m_bTargetReached = false;

    init.Move(&path);
    if (petFollowing)
    {
        float dist = path.Length();
        init.SetWalk(false);
        float speedupDistance = m_fOffset + .5f + owner.GetObjectBoundingRadius() + i_target->GetObjectBoundingRadius();
        if (dist > speedupDistance)
        {
            Unit* pOwner = i_target.getTarget();
            if (pOwner && (!pOwner->IsInCombat() && !owner.IsInCombat() || pOwner->IsPlayer() && pOwner->IsMounted()))
            {
                float distFactor = 1.0f;
                if (pOwner->IsMounted())
                    distFactor += 0.04 * (dist - speedupDistance * 2);
                else
                    distFactor += 0.04 * (dist - speedupDistance);
                if (distFactor < 1.0f) distFactor = 1.0f;
                if (distFactor > 2.1f) distFactor = 2.1f;
                init.SetVelocity(distFactor * owner.GetSpeed(MOVE_RUN));
            }
        }
        else if (dist < 2.0f)
            init.SetWalk(true);
        init.SetFacing(i_target->GetOrientation());
    }
    else
    { 
        init.SetWalk(((D*)this)->EnableWalking());

        // Make player face target he is chasing (player does not automatically face target like creature).
        if (owner.IsPlayer() && this->GetMovementGeneratorType() == CHASE_MOTION_TYPE)
            init.SetFacingGUID(i_target->GetGUID());
    }

    init.Launch();
    m_checkDistanceTimer.Reset(100);

    // Fly-hack
    if (Player* player = i_target->ToPlayer())
    {
        float allowed_dist = owner.GetCombatReach(false) + i_target->GetCombatReach(false) + 5.0f;
        G3D::Vector3 dest = owner.movespline->FinalDestination();
        if ((player->GetPositionZ() - allowed_dist - 5.0f) > dest.z)
            player->GetCheatData()->OnUnreachable(&owner);
    }
}

template<>
void LuaAITargetedMovementGeneratorMedium<Player, LuaAIChaseMovementGenerator<Player> >::UpdateFinalDistance(float /*fDistance*/)
{
    // nothing to do for Player
}

template<>
void LuaAITargetedMovementGeneratorMedium<Player, LuaAIFollowMovementGenerator<Player> >::UpdateFinalDistance(float /*fDistance*/)
{
    // nothing to do for Player
}

template<>
void LuaAITargetedMovementGeneratorMedium<Creature, LuaAIChaseMovementGenerator<Creature> >::UpdateFinalDistance(float fDistance)
{
    m_fOffset = fDistance;
    m_bRecalculateTravel = true;
}

template<>
void LuaAITargetedMovementGeneratorMedium<Creature, LuaAIFollowMovementGenerator<Creature> >::UpdateFinalDistance(float fDistance)
{
    m_fOffset = fDistance;
    m_bRecalculateTravel = true;
}

template<class T, typename D>
void LuaAITargetedMovementGeneratorMedium<T, D>::UpdateAsync(T &owner, uint32 /*diff*/)
{
    if (!m_bRecalculateTravel)
        return;
    // All these cases will be handled at next sync update
    if (!i_target.isValid() || !i_target->IsInWorld() || !owner.IsAlive() || owner.HasUnitState(UNIT_STAT_NO_FREE_MOVE | UNIT_STAT_POSSESSED)
            || static_cast<D*>(this)->_lostTarget(owner)
            || owner.IsNoMovementSpellCasted()
            || IsFalling(owner.ToPlayer()))
        return;

    // Lock async updates for safety, see Unit::asyncMovesplineLock doc
    std::unique_lock<std::mutex> guard(owner.asyncMovesplineLock);
    _setTargetLocation(owner);
}

template<class T>
bool LuaAIChaseMovementGenerator<T>::IsAngleBad(T& owner, bool mutualChase)
{
    if (m_bUseAbsAngle)
        return std::abs(i_target->GetAngle(&owner) - m_fAbsAngle) > .2f;

    if (mutualChase || !m_bUseAngle)
        return false;

    float relAngle = MapManager::NormalizeOrientation(i_target->GetAngle(&owner) - i_target->GetOrientation());
    float angleDiff = std::abs(relAngle - m_fAngle);
    return std::min(angleDiff, M_PI_F * 2.f - angleDiff) > m_angleT;
}

template<class T>
bool LuaAIChaseMovementGenerator<T>::IsDistBad(T& owner, bool mutualChase)
{
    // must be within (offset - offsetMin, offset + offsetMax)
    float combinedBoundingRadius = i_target->GetObjectBoundingRadius() + owner.GetObjectBoundingRadius();
    float distanceToTarget = i_target->GetDistance(&owner, SizeFactor::None);

    float dMin = m_fOffset + combinedBoundingRadius - m_offsetMin;
    float dMax = m_fOffset + combinedBoundingRadius + m_offsetMax;

    if (m_noMinOffsetIfMutual && mutualChase)
        dMin = combinedBoundingRadius - 1.0f;

    if (dMin < 0.00001f)
        dMin = 0.00001f;

    //float relAngle = MapManager::NormalizeOrientation(i_target->GetAngle(&owner) - i_target->GetOrientation());
    //float angleDiff = std::abs(relAngle - m_fAngle);
    //angleDiff = std::min(angleDiff, (float) (M_PI) * 2 - angleDiff);
    //printf("Dist = %.3f dMax = %.3f dMin = %.3f dReq = %.3f Angle = %.3f adiff = %.3f aReq = %.3f aT = %.3f\n", distanceToTarget, dMax, dMin, combinedBoundingRadius, relAngle, angleDiff, m_fAngle, m_angleT);

    // min dist for path finder is 0.4
    if (distanceToTarget > dMax)
        return distanceToTarget - m_fOffset > 0.4f;

    if (distanceToTarget < dMin)
        return dMin + .1f - distanceToTarget > 0.4f;

    if (m_fOffset != .0f)
        return !i_target->IsWithinLOSInMap(&owner);
    return false;
}

template<class T>
bool LuaAIChaseMovementGenerator<T>::Update(T &owner, uint32 const&  time_diff)
{
    if (!i_target.isValid() || !i_target->IsInWorld())
        return false;

    if (!owner.IsAlive())
        return true;

    if (owner.movespline->IsUninterruptible() && !owner.movespline->Finalized())
        return true;

    if (owner.HasUnitState(UNIT_STAT_NO_FREE_MOVE | UNIT_STAT_POSSESSED) || IsFalling(owner.ToPlayer()))
    {
        _clearUnitStateMove(owner);
        return true;
    }

    // prevent movement while casting spells with cast time or channel time
    // don't stop creature movement for spells without interrupt movement flags
    if (owner.IsNoMovementSpellCasted())
    {
        if (!owner.IsStopped())
            owner.StopMoving();
        return true;
    }

    // prevent crash after creature killed pet
    if (_lostTarget(owner))
    {
        _clearUnitStateMove(owner);
        return true;
    }

    bool interrupted = false;
    m_checkDistanceTimer.Update(time_diff);
    if (m_checkDistanceTimer.Passed())
    {
        m_checkDistanceTimer.Reset(50);
        //More distance let have better performance, less distance let have more sensitive reaction at target move.
        if (!owner.movespline->Finalized() && i_target->IsWithinDist(&owner, 0.0f) && !m_fOffset)
        {
            owner.movespline->_Interrupt();
            interrupted = true;
        }
        else
        {
            float allowed_dist = owner.GetMaxChaseDistance(i_target.getTarget()) - 0.5f;
            bool targetMoved = false;
            G3D::Vector3 dest(m_fTargetLastX, m_fTargetLastY, m_fTargetLastZ);
            if (GenericTransport* ownerTransport = owner.GetTransport())
            {
                if (m_bTargetOnTransport)
                    ownerTransport->CalculatePassengerPosition(dest.x, dest.y, dest.z);
                else
                    targetMoved = true;
            }
            else if (m_bTargetOnTransport)
                targetMoved = true;

            bool bSimpleChase = false;
            if (!m_bIsRanged)
                if (Player* pOwner = owner.ToPlayer())
                    if (LuaAgent* agent = pOwner->GetLuaAI())
                        bSimpleChase = agent->IsSimpleChaseMode();

            if (bSimpleChase)
            {
                targetMoved = !owner.CanReachWithMeleeAutoAttack(i_target.getTarget());
            }
            else
            {
                bool mutualChase = false;
                if (Unit* victim = i_target->GetVictim())
                    if (victim->GetObjectGuid() == owner.GetObjectGuid())
                        mutualChase = true;

                if (!targetMoved)
                    targetMoved = IsDistBad(owner, mutualChase);

                if (!targetMoved)
                    targetMoved = IsAngleBad(owner, mutualChase);
            }

            if (targetMoved)
            {
                m_bRecalculateTravel = true;
                owner.GetMotionMaster()->SetNeedAsyncUpdate();
            }
        }
    }

    if (owner.movespline->Finalized())
    {
        MovementInform(owner);

        if (!m_bTargetReached)
        {
            m_bTargetReached = true;
            _reachTarget(owner);
        }

        if (interrupted)
            owner.StopMoving();

        if (interrupted || !owner.HasInArc(i_target.getTarget(), 0.25f))
            owner.SetFacingTo(owner.GetAngle(i_target.getTarget()));
    }
    else if (m_bRecalculateTravel)
    {
        owner.GetMotionMaster()->SetNeedAsyncUpdate();
    }

    return true;
}

//-----------------------------------------------//
template<class T>
void LuaAIChaseMovementGenerator<T>::_reachTarget(T &owner)
{
}

template<>
void LuaAIChaseMovementGenerator<Player>::Initialize(Player &owner)
{
    owner.AddUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    owner.SetWalk(false, true);
    //m_bRecalculateTravel = true;
    //owner.GetMotionMaster()->SetNeedAsyncUpdate();
}

template<>
void LuaAIChaseMovementGenerator<Creature>::Initialize(Creature &owner)
{
    owner.SetWalk(false, false);
    owner.AddUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    m_bRecalculateTravel = true;
    owner.GetMotionMaster()->SetNeedAsyncUpdate();
}

template<class T>
void LuaAIChaseMovementGenerator<T>::Finalize(T &owner)
{
    owner.ClearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    //MovementInform(owner);
}

template<class T>
void LuaAIChaseMovementGenerator<T>::Interrupt(T &owner)
{
    owner.ClearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
}

template<class T>
void LuaAIChaseMovementGenerator<T>::Reset(T &owner)
{
    Initialize(owner);
}

template<class T>
void LuaAIChaseMovementGenerator<T>::MovementInform(T& /*unit*/)
{
}

template<>
void LuaAIChaseMovementGenerator<Creature>::MovementInform(Creature& unit)
{
    if (!unit.IsAlive())
        return;

    // Pass back the GUIDLow of the target. If it is pet's owner then PetAI will handle
    if (unit.AI())
        unit.AI()->MovementInform(CHASE_MOTION_TYPE, i_target.getTarget()->GetGUIDLow());

    if (unit.IsTemporarySummon())
    {
        TemporarySummon* pSummon = (TemporarySummon*)(&unit);
        if (pSummon->GetSummonerGuid().IsCreature())
        {
            if (Creature* pSummoner = unit.GetMap()->GetCreature(pSummon->GetSummonerGuid()))
                if (pSummoner->AI())
                    pSummoner->AI()->SummonedMovementInform(&unit, CHASE_MOTION_TYPE, i_target.getTarget()->GetGUIDLow());
        }
        else
        {
            if (GameObject* pSummoner = unit.GetMap()->GetGameObject(pSummon->GetSummonerGuid()))
                if (pSummoner->AI())
                    pSummoner->AI()->SummonedMovementInform(&unit, CHASE_MOTION_TYPE, i_target.getTarget()->GetGUIDLow());
        }
    }
}

//-----------------------------------------------//

template<class T>
bool LuaAIFollowMovementGenerator<T>::Update(T &owner, uint32 const&  time_diff)
{
    if (!i_target.isValid() || !i_target->IsInWorld())
    return false;

    if (!owner.IsAlive())
        return true;

    if (owner.HasUnitState(UNIT_STAT_NO_FREE_MOVE | UNIT_STAT_POSSESSED) || IsFalling(owner.ToPlayer()))
    {
        _clearUnitStateMove(owner);
        return true;
    }

    // prevent movement while casting spells with cast time or channel time
    // don't stop creature movement for spells without interrupt movement flags
    if (owner.IsNoMovementSpellCasted())
    {
        if (!owner.IsStopped())
            owner.StopMoving();
        return true;
    }

    // prevent crash after creature killed pet
    if (_lostTarget(owner))
    {
        _clearUnitStateMove(owner);
        return true;
    }

    bool interrupted = false;
    m_checkDistanceTimer.Update(time_diff);
    if (m_checkDistanceTimer.Passed())
    {
        m_checkDistanceTimer.Reset(100);
        //More distance let have better performance, less distance let have more sensitive reaction at target move.
        if (!owner.movespline->Finalized() && 
                 ((!m_fOffset && i_target->IsWithinDist(&owner, 0.0f)) ||
                 (i_target->IsPlayer() && !i_target->IsMoving() &&
                 Geometry::GetDistance3D(owner.movespline->FinalDestination(), i_target->GetPosition()) > (m_fOffset + owner.GetObjectBoundingRadius() + i_target->GetObjectBoundingRadius() + 0.5f) &&
                 Geometry::GetDistance3D(owner.GetPosition(), i_target->GetPosition()) <= (m_fOffset + owner.GetObjectBoundingRadius() + i_target->GetObjectBoundingRadius() + 0.5f))))
        {
            owner.movespline->_Interrupt();
            interrupted = true;
        }
        else
        {
            bool targetMoved = false;
            G3D::Vector3 dest(m_fTargetLastX, m_fTargetLastY, m_fTargetLastZ);
            if (GenericTransport* ownerTransport = owner.GetTransport())
            {
                if (m_bTargetOnTransport)
                    ownerTransport->CalculatePassengerPosition(dest.x, dest.y, dest.z);
                else
                    targetMoved = true;
            }
            else if (m_bTargetOnTransport)
                targetMoved = true;

            if (!targetMoved)
                targetMoved = !i_target->IsWithinDist3d(dest.x, dest.y, dest.z, 0.5f);

            if (!targetMoved)
                targetMoved = std::abs(i_target->GetDistance(&owner) - m_fOffset) > .1f;

            if (!targetMoved)
            {
                float relAngle = MapManager::NormalizeOrientation(i_target->GetAngle(&owner) - i_target->GetOrientation());
                float angleDiff = std::abs(relAngle - m_fAngle);
                targetMoved = std::min(angleDiff, M_PI_F * 2 - angleDiff) > .1f;
            }

            if (targetMoved)
            {
                m_bRecalculateTravel = true;
                owner.GetMotionMaster()->SetNeedAsyncUpdate();
            }
            else
            {
                if (!owner.movespline->Finalized())
                {
                    owner.movespline->_Interrupt();
                    interrupted = true;
                }
            }
        }
    }

    if (owner.movespline->Finalized())
    {
        MovementInform(owner);

        if (m_fAngle == 0.f && !owner.HasInArc(i_target.getTarget(), 0.01f))
            owner.SetInFront(i_target.getTarget());

        if (!m_bTargetReached)
        {
            m_bTargetReached = true;
            _reachTarget(owner);
        }
        if (interrupted)
            owner.StopMoving(true);
    }
    else if (m_bRecalculateTravel)
        owner.GetMotionMaster()->SetNeedAsyncUpdate();
        
    return true;
}

template<>
bool LuaAIFollowMovementGenerator<Creature>::EnableWalking() const
{
    return i_target.isValid() && i_target->IsWalking();
}

template<>
bool LuaAIFollowMovementGenerator<Player>::EnableWalking() const
{
    return false;
}

template<>
void LuaAIFollowMovementGenerator<Player>::_updateSpeed(Player &/*u*/)
{
    // nothing to do for Player
}

template<>
void LuaAIFollowMovementGenerator<Creature>::_updateSpeed(Creature &u)
{
    if (!i_target.isValid() || i_target->GetObjectGuid() != u.GetOwnerGuid())
        return;

    u.UpdateSpeed(MOVE_RUN, false);
    u.UpdateSpeed(MOVE_WALK, false);
    u.UpdateSpeed(MOVE_SWIM, false);
}

template<>
void LuaAIFollowMovementGenerator<Player>::Initialize(Player &owner)
{
    owner.SetWalk(false, true);
    owner.AddUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    _updateSpeed(owner);
    _setTargetLocation(owner);
}

template<>
void LuaAIFollowMovementGenerator<Creature>::Initialize(Creature &owner)
{
    owner.AddUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    _updateSpeed(owner);
    _setTargetLocation(owner);
}

template<class T>
void LuaAIFollowMovementGenerator<T>::Finalize(T &owner)
{
    owner.ClearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    _updateSpeed(owner);
    owner.StopMoving();
    //MovementInform(owner);
}

template<class T>
void LuaAIFollowMovementGenerator<T>::Interrupt(T &owner)
{
    owner.ClearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    _updateSpeed(owner);
}

template<class T>
void LuaAIFollowMovementGenerator<T>::Reset(T &owner)
{
    Initialize(owner);
}

template<class T>
void LuaAIFollowMovementGenerator<T>::MovementInform(T& /*unit*/)
{
}

template<>
void LuaAIFollowMovementGenerator<Creature>::MovementInform(Creature& unit)
{
    if (!unit.IsAlive())
        return;

    // Pass back the GUIDLow of the target. If it is pet's owner then PetAI will handle
    if (unit.AI())
        unit.AI()->MovementInform(FOLLOW_MOTION_TYPE, i_target.getTarget()->GetGUIDLow());

    if (unit.IsTemporarySummon())
    {
        TemporarySummon* pSummon = (TemporarySummon*)(&unit);
        if (pSummon->GetSummonerGuid().IsCreature())
        {
            if (Creature* pSummoner = unit.GetMap()->GetCreature(pSummon->GetSummonerGuid()))
                if (pSummoner->AI())
                    pSummoner->AI()->SummonedMovementInform(&unit, FOLLOW_MOTION_TYPE, i_target.getTarget()->GetGUIDLow());
        }
        else
        {
            if (GameObject* pSummoner = unit.GetMap()->GetGameObject(pSummon->GetSummonerGuid()))
                if (pSummoner->AI())
                    pSummoner->AI()->SummonedMovementInform(&unit, FOLLOW_MOTION_TYPE, i_target.getTarget()->GetGUIDLow());
        }
    }
}

//-----------------------------------------------//
template void LuaAITargetedMovementGeneratorMedium<Player, LuaAIChaseMovementGenerator<Player> >::_setTargetLocation(Player &);
template void LuaAITargetedMovementGeneratorMedium<Player, LuaAIFollowMovementGenerator<Player> >::_setTargetLocation(Player &);
template void LuaAITargetedMovementGeneratorMedium<Creature, LuaAIChaseMovementGenerator<Creature> >::_setTargetLocation(Creature &);
template void LuaAITargetedMovementGeneratorMedium<Creature, LuaAIFollowMovementGenerator<Creature> >::_setTargetLocation(Creature &);
template void LuaAITargetedMovementGeneratorMedium<Player, LuaAIChaseMovementGenerator<Player> >::UpdateAsync(Player &, uint32);
template void LuaAITargetedMovementGeneratorMedium<Player, LuaAIFollowMovementGenerator<Player> >::UpdateAsync(Player &, uint32);
template void LuaAITargetedMovementGeneratorMedium<Creature, LuaAIChaseMovementGenerator<Creature> >::UpdateAsync(Creature &, uint32);
template void LuaAITargetedMovementGeneratorMedium<Creature, LuaAIFollowMovementGenerator<Creature> >::UpdateAsync(Creature &, uint32);

template bool LuaAIChaseMovementGenerator<Player>::Update(Player &, uint32 const&);
template bool LuaAIChaseMovementGenerator<Creature>::Update(Creature &, uint32 const&);
template void LuaAIChaseMovementGenerator<Player>::_reachTarget(Player &);
template void LuaAIChaseMovementGenerator<Creature>::_reachTarget(Creature &);
template void LuaAIChaseMovementGenerator<Player>::Finalize(Player &);
template void LuaAIChaseMovementGenerator<Creature>::Finalize(Creature &);
template void LuaAIChaseMovementGenerator<Player>::Interrupt(Player &);
template void LuaAIChaseMovementGenerator<Creature>::Interrupt(Creature &);
template void LuaAIChaseMovementGenerator<Player>::Reset(Player &);
template void LuaAIChaseMovementGenerator<Creature>::Reset(Creature &);
template void LuaAIChaseMovementGenerator<Player>::MovementInform(Player&);
template bool LuaAIChaseMovementGenerator<Player>::IsDistBad(Player &, bool);
template bool LuaAIChaseMovementGenerator<Creature>::IsDistBad(Creature &, bool);
template bool LuaAIChaseMovementGenerator<Player>::IsAngleBad(Player &, bool);
template bool LuaAIChaseMovementGenerator<Creature>::IsAngleBad(Creature &, bool);

template bool LuaAIFollowMovementGenerator<Player>::Update(Player &, uint32 const&);
template bool LuaAIFollowMovementGenerator<Creature>::Update(Creature &, uint32 const&);
template void LuaAIFollowMovementGenerator<Player>::Finalize(Player &);
template void LuaAIFollowMovementGenerator<Creature>::Finalize(Creature &);
template void LuaAIFollowMovementGenerator<Player>::Interrupt(Player &);
template void LuaAIFollowMovementGenerator<Creature>::Interrupt(Creature &);
template void LuaAIFollowMovementGenerator<Player>::Reset(Player &);
template void LuaAIFollowMovementGenerator<Creature>::Reset(Creature &);
template void LuaAIFollowMovementGenerator<Player>::MovementInform(Player&);
