#pragma once

#include <vector>
#include <deque>
#include <array>
#include <memory>
#include <time.h>
#include "GoalManager.h"
#include "GoalParam.h"

class GoalManager;

// size of numbers array in Goal objects
const uint8_t GOAL_NUMBER_COUNT_MAX = 10;
// size of timers array in Goal objects
const uint8_t GOAL_TIMER_COUNT_MAX = 10;

// TODO: Index boundary checks errors. Perhaps replace number type with lua_Number.

const int GOAL_RESULT_Continue = 0;
const int GOAL_RESULT_Success = 1;
const int GOAL_RESULT_Failed = 2;

enum class GoalResult : uint8_t {
	Continue,
	Success,
	Failed
};

typedef std::shared_ptr<GoalParam> GoalParamP;
class Goal {

	lua_State* L;

	GoalManager* manager;

	Goal* parent;
	int goalId;
	int userDataRef;
	int userTblRef;
	double life;
	double timeExpire;
	GoalResult lastResult;

	bool bCanExpire;
	bool bSuccessOnLifeEnd;
	bool bActivated;
	bool bTerminated;
	bool bTerminationState;


	std::vector<GoalParamP> params;
	std::array<double, GOAL_NUMBER_COUNT_MAX> numbers;
	std::array<double, GOAL_TIMER_COUNT_MAX> timers;
	std::deque<std::shared_ptr<Goal>> subgoals; // we own the subgoals. if we die we take them with us.

public:

	static std::vector<GoalParamP> NOPARAMS;

	// Creates a new goal object with specified designer id and life time. Vector passed here is moved to internal vector.
	Goal(int goalId, double life, std::vector<GoalParamP>& goalParams, GoalManager* manager, lua_State* L, Goal* parent = NULL);
	~Goal();
	// Returns the designer goal id.
	int GetGoalId();
	// Returns life time of the goal.
	double GetLife();
	bool CanExpire() { return bCanExpire; }
	void CreateUsertable() {
		if (userTblRef == LUA_NOREF) {
			lua_newtable(L);
			userTblRef = luaL_ref(L, LUA_REGISTRYINDEX);
		}
	}
	bool GetActivated();
	bool GetTerminated();
	Goal* GetParent() { return parent; }
	void SetActivated(bool v);
	void SetTerminated(bool v);
	void SetTerminationState() { bTerminationState = true; }
	// Returns userdata key in the lua's registry. Returns LUA_NOREF if never assigned.
	int GetRef();
	// Sets userdata key in the lua's registry.
	void SetRef(int v);
	// Returns user table ref
	int GetUserTblRef() { return userTblRef; }
	// Unref
	void Unref(lua_State* L);

	// Returns pointer to a param at specified index.
	GoalParam* GetParam(int index);

	// Subgoals

	/// <summary>
	/// Creates a new goal object at the back of the member deque.
	/// </summary>
	/// <param name="goalId">Designer ID.</param>
	/// <param name="life">Life time of the goal in seconds.</param>
	/// <param name="goalParams">Reference to goal param vector. It's contents are moved during the call to constructor.</param>
	/// <returns>Pointer to newly created Goal object.</returns>
	Goal* AddSubGoal(int goalId, double life, std::vector<GoalParamP>& goalParams);
	// Pops all subgoals from the queue.
	void ClearSubGoal();
	// Pops front subgoal.
	void ClearSubGoalFront();
	// Returns a pointer to subgoal at the front of the member deque.
	Goal* GetActiveSubGoal();
	// Returns how many subgoals we're keeping.
	int GetSubGoalNum();
	// tells goalmanager we should set to success on expire
	bool GetSuccessOnLifeEnd() { return bSuccessOnLifeEnd; }
	void SetSuccessOnLifeEnd(bool b) { bSuccessOnLifeEnd = b; }
	// 
	void UpdateSubGoal();
	void Terminate(lua_State* L);

	// Result

	// Returns last goal result.
	GoalResult GetLastResult();
	// Sets last goal result.
	void SetLastResult(GoalResult result);

	// Numbers

	// Returns number stored at specified index. Index must be within [0, GOAL_NUMBER_COUNT_MAX].
	double GetNumber(int index);
	// Sets number at index to value. Index must be within [0, GOAL_NUMBER_COUNT_MAX].
	void SetNumber(int index, double value);

	// Timers

	// Returns the time left on a timer in seconds. Index must be within [0, GOAL_TIMER_COUNT_MAX].
	double GetTimer(int index);
	// Starts the timer at specified index. Time is in seconds. Index must be within [0, GOAL_TIMER_COUNT_MAX].
	void SetTimer(int index, double time);
	// Returns true if the timer at specified index has finished. Index must be within [0, GOAL_TIMER_COUNT_MAX].
	bool HasTimerFinished(int index);

	// Debug print
	void Print(const char* indent = "");

};

// ---------------------------------------------------------
//    LUA BINDS
// ---------------------------------------------------------

namespace LuaBindsAI {

	int Goal_AddSubGoal(lua_State* L);
	int Goal_ClearSubGoal(lua_State* L);
	int Goal_GetLife(lua_State* L);
	int Goal_GetNumber(lua_State* L);
	int Goal_GetParam(lua_State* L);
	int Goal_GetSubGoalNum(lua_State* L);
	int Goal_GetTimer(lua_State* L);
	int Goal_GetUserTbl(lua_State* L);
	int Goal_IsFinishTimer(lua_State* L);
	int Goal_SetLifeEndSuccess(lua_State* L);
	int Goal_SetNumber(lua_State* L);
	int Goal_SetTimer(lua_State* L);

	void Goal_CreateMetatable(lua_State* L);
	Goal** Goal_CreateGoalUD(lua_State* L, Goal* goal);
	void Goal_GrabParams(lua_State* L, int nArgs, std::vector<GoalParamP>& params);
	Goal** Goal_GetGoalObject(lua_State* L);
	void BindGoal(lua_State* L);
	static const struct luaL_Reg Goal_BindLib[]{
		{"AddSubGoal", Goal_AddSubGoal},
		{"ClearSubGoal", Goal_ClearSubGoal},
		{"GetLife", Goal_GetLife},
		{"GetNumber", Goal_GetNumber},
		{"GetParam", Goal_GetParam},
		{"GetSubGoalNum", Goal_GetSubGoalNum},
		{"GetTimer", Goal_GetTimer},
		{"GetUserTbl", Goal_GetUserTbl},
		{"IsFinishTimer", Goal_IsFinishTimer},
		{"SetLifeEndSuccess", Goal_SetLifeEndSuccess},
		{"SetNumber", Goal_SetNumber},
		{"SetTimer", Goal_SetTimer},
		{NULL, NULL}
	};

}
