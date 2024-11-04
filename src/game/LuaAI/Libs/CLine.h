
#ifndef MANGOS_LUAAGENTCLINE_H
#define MANGOS_LUAAGENTCLINE_H

#include <fstream>

struct lua_State;

struct CLinePoint
{
	ObjectGuid helper;
	G3D::Vector3 pos;
	float minz;
	float R;

	CLinePoint() : pos(), minz(0.f), helper(), R(2.f) {}
	CLinePoint(G3D::Vector3 pos, float minz, const ObjectGuid& helper) : pos(pos), minz(minz), helper(helper), R(2.f) {}

	void Move(G3D::Vector3 newpos, Player* gm, uint32 helperId)
	{
		if (Creature* go = gm->GetMap()->GetCreature(helper))
		{
			helper = ObjectGuid();
			go->Kill(go, nullptr, false);
		}
		pos = newpos;
		SummonHelper(gm, helperId);
	}

	void SummonHelper(Player* gm, uint32 helperId)
	{
		if (helperId)
			if (Creature* go = gm->SummonCreature(helperId, pos.x, pos.y, pos.z, 0.f, TEMPSUMMON_DEAD_DESPAWN, 1))
				helper = go->GetObjectGuid();
	}

	void UnsummonHelper(Player* gm)
	{
		if (gm && !helper.IsEmpty())
			if (Creature* go = gm->GetMap()->GetCreature(helper))
			{
				helper = ObjectGuid();
				go->Kill(go, nullptr, false);
			}
	}
};

struct CLine
{

	int mapId{-1};
	std::vector<CLinePoint> pts{};

	void Point(const G3D::Vector3& point, const ObjectGuid& helper)
	{
		pts.emplace_back(point, -1000.f, helper);
	}

	bool GetPointInLosAtD(Unit* me, Unit* target, Unit* losTarget, G3D::Vector3& result, int S, float minD, float maxD, float step, float pctStart, bool reverse) const
	{
		float ltx = losTarget->GetPositionX(), lty = losTarget->GetPositionY(), ltz = losTarget->GetPositionZ();
		G3D::Vector3 resultPoint(result);

		int inc = reverse ? -1 : 1;
		int finalS = reverse ? 0 : (pts.size() - 1);

		// in case of reverse must correct segment and % returned by ClosestP
		if (reverse)
		{
			++S;
			pctStart = 1.f - pctStart;
		}

		float pctStep;
		if (S != finalS)
		{
			const G3D::Vector3& A = pts[S].pos;
			const G3D::Vector3& B = pts[S + inc].pos;
			pctStep = step / (A - B).magnitude();
		}

		G3D::Vector3 lastGood;
		float pct = pctStart;
		// is last point
		for (;;)
		{
			if (S == finalS)
			{
				result = std::move(resultPoint);
				float D = target->GetDistance(result.x, result.y, result.z);
				return D <= maxD && me->IsWithinLOSAtPosition(result.x, result.y, result.z, ltx, lty, ltz);
			}

			float D = target->GetDistance(resultPoint.x, resultPoint.y, resultPoint.z);
			bool minMatch = minD <= D;
			bool maxMatch = maxD >= D;
			bool losMatch = maxMatch && me->IsWithinLOSAtPosition(resultPoint.x, resultPoint.y, resultPoint.z, ltx, lty, ltz);
			if (minMatch && losMatch)
			{
				result = std::move(resultPoint);
				return true;
			}

			if (losMatch)
				lastGood = resultPoint;
			else
			{
				result = std::move(lastGood);
				return lastGood.x != 0.f;
			}

			const G3D::Vector3& A = pts[S].pos;
			const G3D::Vector3& B = pts[S + inc].pos;
			G3D::Vector3 AB = B - A;
			resultPoint = G3D::Vector3(A.x + AB.x * pct, A.y + AB.y * pct, A.z + AB.z * pct);

			// get next point
			pct += pctStep;
			if (pct > 1.0f)
			{
				S += inc;
				if (S != finalS)
				{
					pct = 0.f;
					const G3D::Vector3& A = pts[S].pos;
					const G3D::Vector3& B = pts[S + inc].pos;
					pctStep = step / (A - B).magnitude();
				}
			}
		}
		return false;
	}

	G3D::Vector3 ClosestSegP(const G3D::Vector3& from, const G3D::Vector3& A, const G3D::Vector3& B, float& pct) const
	{
		G3D::Vector3 Afrom = from - A;
		G3D::Vector3 AB = B - A;
		float ndot = Afrom.dot(AB) / AB.squaredMagnitude();
		// bind ndot to segment AB
		if (ndot <= 0.f)
		{
			pct = 0.f;
			return A;
		}
		if (ndot >= 1.f)
		{
			pct = 1.f;
			return B;
		}
		pct = ndot;
		return G3D::Vector3(A.x + AB.x * ndot, A.y + AB.y * ndot, A.z + AB.z * ndot);
	}

	float D2(const G3D::Vector3& A, const G3D::Vector3& B) const
	{
		return (B - A).squaredMagnitude();
	}

	void ClosestP(const G3D::Vector3& from, G3D::Vector3& resultPoint, float& resultDistance, int& resultSegment, float& pct) const
	{
		assert(pts.size() > 1);
		if (pts.size() == 2)
		{
			resultPoint = ClosestSegP(from, pts[0].pos, pts[1].pos, pct);
			resultDistance = D2(from, resultPoint);
			resultSegment = 0;
			return;
		}
		G3D::Vector3 nearestP = pts[0].pos;
		float nearestD = std::numeric_limits<float>::max();
		int nearestS = 0;
		float percent;

		for (size_t i = 1; i < pts.size(); ++i)
		{
			const G3D::Vector3& A = pts[i - 1].pos;
			const G3D::Vector3& B = pts[i].pos;
			const G3D::Vector3 C = ClosestSegP(from, A, B, percent);

			if (C == nearestP)
				continue;
			
			float d2 = D2(from, C);
			if (d2 < nearestD)
			{
				nearestD = d2;
				nearestP = C;
				nearestS = i - 1;
				pct = percent;
			}
		}

		resultPoint = nearestP;
		resultDistance = nearestD;
		resultSegment = nearestS;
	}

	CLinePoint* FindPoint(const ObjectGuid& guid)
	{
		for (auto& it : pts)
			if (it.helper == guid)
				return &it;
		return nullptr;
	}

	void DelSeg(Map* map, const ObjectGuid& guid)
	{
		if (guid.IsEmpty() || !map || !pts.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "CLine::DelSeg: empty guid (%d) or !map (%d) or !pts.size (%d)", guid.IsEmpty(), !map, !pts.size());
			return;
		}
		for (auto& it = pts.begin(); it != pts.end(); ++it)
		{
			if (it->helper == guid)
			{
				if (Creature* go = map->GetCreature(it->helper))
					go->Kill(go, nullptr, false);
				it = pts.erase(it);
				return;
			}
		}
		sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "CLine::DelSeg: guid not found %llu", guid.GetRawValue());
	}

	void Write(const std::string& name) const
	{
		std::ofstream out(name, std::ofstream::trunc);
		Write(out);
		out.close();
	}

	void Write(std::ofstream& out) const
	{
		if (!pts.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "CLine::Write: attempt to write empty cline");
			return;
		}
		for (auto& it : pts)
		{
			out.write(reinterpret_cast<const char*>(&it.pos.x), sizeof it.pos.x);
			out.write(reinterpret_cast<const char*>(&it.pos.y), sizeof it.pos.y);
			out.write(reinterpret_cast<const char*>(&it.pos.z), sizeof it.pos.z);
			out.write(reinterpret_cast<const char*>(&it.minz),  sizeof it.minz);
			out.write(reinterpret_cast<const char*>(&it.R),     sizeof it.R);
		}
	}

	void WriteAsTable(const std::string& name) const
	{
		std::ofstream out(name, std::ofstream::trunc);
		WriteAsTable(out);
		out.close();
	}

	void WriteAsTable(std::ofstream& out, const std::string& prefix = "\t\t") const
	{
		if (!pts.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "CLine::WriteAsTable: attempt to write empty cline");
			return;
		}
		for (int i = 0; i < pts.size(); ++i)
		{
			auto& it = pts[i];
			out << prefix << "{" << it.pos.x << ", " << it.pos.y << ", " << it.pos.z << ", " << it.minz << ", " << it.R << "}," << " -- " << i + 1 << '\n';
		}
	}
};

struct DungeonData
{
	struct EncounterData
	{
		ObjectGuid guid{};
		std::string name{};
		G3D::Vector3 tankPos{};
		bool forceTankPos{false};
		EncounterData(const ObjectGuid& guid, const std::string& name, const G3D::Vector3& tankPos, bool forceTankPos)
			: guid(guid), name(name), tankPos(tankPos), forceTankPos(forceTankPos) {}
	};

	std::unordered_map<std::string, EncounterData> encounters;
	int mapId{-1};
	std::vector<CLine> lines;

	bool GetPointInLosAtD(Unit* me, Unit* target, Unit* losTarget, G3D::Vector3& result, float minD, float maxD, float step, bool reverse, int lineIdx = -1)
	{
		const G3D::Vector3 from(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
		float D, pct = 0.f;
		int S;
		int closestLine = ClosestP(from, result, D, S, pct);
		if (lineIdx > 0 && closestLine != lineIdx) return false;
		return lines[closestLine].GetPointInLosAtD(me, target, losTarget, result, S, minD, maxD, step, pct, reverse);
	}

	int ClosestP(const G3D::Vector3& from, G3D::Vector3& resultPoint, float& resultDistance, int& resultSegment, float& pct)
	{
		assert(lines.size() > 0);

		G3D::Vector3 nearestP = lines.front().pts[0].pos;
		float nearestD = std::numeric_limits<float>::max();
		int nearestS = 0;
		int nearestL = 0;

		G3D::Vector3 P;
		for (int i = 0; i < lines.size(); ++i)
		{
			const CLine& line = lines[i];
			float D2, percent;
			int S;
			line.ClosestP(from, P, D2, S, percent);
			if (D2 < nearestD)
			{
				nearestP = P;
				nearestD = D2;
				nearestS = S;
				nearestL = i;
				pct = percent;
			}
		}
		
		resultPoint = nearestP;
		resultDistance = sqrt(nearestD);
		resultSegment = nearestS;
		return nearestL;
	}

	CLinePoint* FindPoint(const ObjectGuid& helper)
	{
		for (auto& line : lines)
			if (CLinePoint* p = line.FindPoint(helper))
				return p;
		return nullptr;
	}

	void SummonHelpers(Player* gm, uint32 helperId)
	{
		if (!gm || !helperId || gm->GetMapId() != mapId) return;
		for (auto& line : lines)
			for (auto& point : line.pts)
				point.SummonHelper(gm, helperId);
	}

	void UnsummonHelpers(Player* gm)
	{
		if (!gm) return;
		for (auto& line : lines)
			for (auto& point : line.pts)
				point.UnsummonHelper(gm);
	}

	EncounterData* GetEncounter(const std::string& name)
	{
		auto& it = encounters.find(name);
		return it != encounters.end() ? &it->second : nullptr;
	}

	void Write(const std::string& name)
	{
		if (!lines.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::Write lines vector is empty");
			return;
		}
		std::ofstream out(name, std::ofstream::trunc | std::ofstream::binary);
		if (!out.is_open())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::Write could not open file for writing %s", name.c_str());
			return;
		}
		// don't save trailing empty line
		if (lines.size() && lines.back().pts.empty())
			lines.pop_back();
		uint32 nlines = lines.size();
		out.write(reinterpret_cast<const char*>(&mapId),  sizeof mapId);
		out.write(reinterpret_cast<const char*>(&nlines), sizeof nlines);
		for (auto& line : lines)
		{
			uint32 npts = line.pts.size();
			out.write(reinterpret_cast<const char*>(&npts), sizeof npts);
			line.Write(out);
		}
		out.close();
	}

	void WriteAsTable(const std::string& name)
	{
		if (!lines.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::Write lines vector is empty");
			return;
		}
		std::ofstream out(name, std::ofstream::trunc);
		if (!out.is_open())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::Write could not open file for writing %s", name.c_str());
			return;
		}
		// don't save trailing empty line
		if (lines.size() && lines.back().pts.empty())
			lines.pop_back();
		out << '[' << mapId << "] = {\n";
		for (int i = 0; i < lines.size(); ++i)
		{
			auto& line = lines[i];
			out << "\t-- Line " << i + 1 << "\n\t{\n";
			line.WriteAsTable(out, "\t\t");
			out << "\t},\n";
		}
		out << "};\n";
		out.close();
	}

	void Load(const std::string& name, Player* gm, uint32 helper)
	{
		if (lines.size())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::Load lines vector is not empty");
			return;
		}
		std::ifstream in(name, std::ifstream::binary);
		if (!in.is_open())
		{
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "DungeonData::Load could not open file for reading %s", name.c_str());
			return;
		}
		uint32 nlines;
		in.read(reinterpret_cast<char*>(&mapId),  sizeof mapId);
		in.read(reinterpret_cast<char*>(&nlines), sizeof nlines);
		for (int i = 0; i < nlines; ++i)
		{
			lines.emplace_back();
			uint32 npoints;
			in.read(reinterpret_cast<char*>(&npoints), sizeof npoints);
			CLine& line = lines.back();
			for (int j = 0; j < npoints; ++j)
			{
				line.pts.emplace_back();
				CLinePoint& point = line.pts.back();
				in.read(reinterpret_cast<char*>(&point.pos.x), sizeof point.pos.x);
				in.read(reinterpret_cast<char*>(&point.pos.y), sizeof point.pos.y);
				in.read(reinterpret_cast<char*>(&point.pos.z), sizeof point.pos.z);
				in.read(reinterpret_cast<char*>(&point.minz),  sizeof point.minz);
				in.read(reinterpret_cast<char*>(&point.R),     sizeof point.R);
				if (gm)
					point.SummonHelper(gm, helper);
			}
		}

		in.close();
	}

	void LoadFromTable(lua_State* L, uint32 mapId, Player* gm = nullptr, uint32 helperId = 0u);
};

#endif
