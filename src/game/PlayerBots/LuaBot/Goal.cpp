#include "Goal.h"
#include "LuaUtils.h"

std::vector<GoalParamP> Goal::NOPARAMS{};

// Creates a new goal object with specified designer id and life time. Vector passed here is moved to internal vector.
Goal::Goal(int goalId, double life, std::vector<GoalParamP>& goalParams,
	GoalManager* manager, lua_State* L, Goal* parent)

	: goalId(goalId), life(life), numbers{}, timers{}, lastResult(GoalResult::Continue), parent(parent),
	bActivated(false), bTerminated(false), timeExpire(-1.0), userDataRef(LUA_NOREF), manager(manager),
	L(L), bTerminationState(false), bCanExpire(true), bSuccessOnLifeEnd(false)
{
	//printf( "Creating goal %d, life %f, params %d\n", goalId, life, (int)goalParams.size() );
	if (goalParams.size() > 0)
		params = std::move(goalParams);
	if (life < 0)
		bCanExpire = false;
	userTblRef = LUA_NOREF;
}

Goal::~Goal() {
	//printf( "Goal %d destroyed\n", goalId );
	if (L) {
		if (userDataRef != LUA_NOREF && userDataRef != LUA_REFNIL)
			luaL_unref(L, LUA_REGISTRYINDEX, userDataRef);
		if (userTblRef != LUA_NOREF && userTblRef != LUA_REFNIL)
			luaL_unref(L, LUA_REGISTRYINDEX, userTblRef);
	}
}

void Goal::Unref(lua_State* L) {
	if (userDataRef != LUA_NOREF && userDataRef != LUA_REFNIL)
		luaL_unref(L, LUA_REGISTRYINDEX, userDataRef);
	userDataRef = LUA_NOREF;
}

// Returns the designer goal id.
int Goal::GetGoalId() {
	return goalId;
}

// Returns life time of the goal.
double Goal::GetLife() {
	return timeExpire - clock() / (double) CLOCKS_PER_SEC;
}

bool Goal::GetActivated() {
	return bActivated;
}

bool Goal::GetTerminated() {
	return bTerminated;
}

int Goal::GetRef() {
	return userDataRef;
}

void Goal::SetRef(int v) {
	userDataRef = v;
}

void Goal::SetActivated(bool v) {
	if (!bActivated && v) {
		bActivated = v;
		timeExpire = clock() / (double) CLOCKS_PER_SEC + life;
	}
}

void Goal::SetTerminated(bool v) {
	bTerminated = v;
}

// SUBGOALS -----------------------------------------------------------------------

void Goal::UpdateSubGoal() {
	subgoals.pop_front();
	if (!subgoals.empty() && !bTerminationState)
		manager->PushGoalOnActivationStack(subgoals.front().get());
}

Goal* Goal::AddSubGoal(int goalId, double life, std::vector<GoalParamP>& goalParams) {
	subgoals.push_back(std::make_shared<Goal>(goalId, life, goalParams, manager, L, this));
	Goal* result = subgoals.back().get();
	// if this is the only goal queue for activation
	if (subgoals.size() == 1)
		manager->PushGoalOnActivationStack(result);
	return result;
}
// Pops all subgoals from the queue.
void Goal::ClearSubGoal() {
	subgoals.clear();
}
// Pops front subgoal.
void Goal::ClearSubGoalFront() {
	subgoals.pop_front();
}
// Returns a pointer to subgoal at the front of the member deque.
Goal* Goal::GetActiveSubGoal() {
	if (GetSubGoalNum() > 0)
		return subgoals.front().get();
	return NULL;
}
// Returns how many subgoals we're keeping.
int Goal::GetSubGoalNum() {
	return subgoals.size();
}
void Goal::Terminate(lua_State* L) {

	if (parent != NULL) {
		if (parent->GetSubGoalNum() > 0) {
			// can't do this cos it will destroy this object
			//parent->ClearSubGoalFront();
		}
	}
}

// RESULTS -----------------------------------------------------------------------

GoalResult Goal::GetLastResult() {
	return lastResult;
}

void Goal::SetLastResult(GoalResult result) {
	lastResult = result;
}


// Returns pointer to param at specified index.
GoalParam* Goal::GetParam(int index) {
	if (index >= 0 && index < params.size())
		return params.at(index).get();
	return nullptr;
}

// NUMBERS -----------------------------------------------------------------------

// Returns number stored at specified index. Index must be within [0, GOAL_NUMBER_COUNT_MAX].
double Goal::GetNumber(int index) {
	//if ( index > -1 && index < numbers.size() )
	return numbers[index];
	//return -1.0;
}

// Sets number at index to value. Index must be within [0, GOAL_NUMBER_COUNT_MAX].
void Goal::SetNumber(int index, double value) {
	//if ( index > -1 && index < numbers.size() )
	numbers[index] = value;
}

// TIMERS -----------------------------------------------------------------------

// Returns the time left on a timer in seconds. Index must be within [0, GOAL_TIMER_COUNT_MAX].
double  Goal::GetTimer(int index) {
	//if ( index > -1 && index < timers.size() )
	return timers[index] - clock() / (double) CLOCKS_PER_SEC;
	//return -1.0;
}
// Starts the timer at specified index. Time is in seconds. Index must be within [0, GOAL_TIMER_COUNT_MAX].
void Goal::SetTimer(int index, double time) {
	//if ( index > -1 && index < timers.size() )
	// only store the time at which we should be finished
	timers[index] = clock() / (double) CLOCKS_PER_SEC + time;
}
// Returns true if the timer at specified index has finished. Index must be within [0, GOAL_TIMER_COUNT_MAX].
bool Goal::HasTimerFinished(int index) {
	return (clock() / (double) CLOCKS_PER_SEC) >= timers[index];
}


// Debug print of the contents of the goal object.
void Goal::Print(const char* indent) {
	printf("%sGoal:\n", indent);
	printf("%s-ID = %d\n", indent, goalId);
	printf("%s-Life = %f\n", indent, life);
	printf("%s-Params:\n", indent);
	for (int i = 0; i < params.size(); i++)
		printf("%s-  Param[%d] = %s\n", indent, i, params[i]->ToString().c_str());
	printf("%s-Numbers:\n", indent);
	for (int i = 0; i < numbers.size(); i++)
		printf("%s-  Number[%d] = %f\n", indent, i, numbers[i]);
	printf("%s-Subgoals:\n", indent);
	auto i = subgoals.begin();
	while (i != subgoals.end()) {
		printf("--------------------------------------------\n");
		(*i)->Print((std::string(indent) + "    ").c_str());
		i++;
	}
}

// ---------------------------------------------------------
//    LUA BINDS
// ---------------------------------------------------------

void LuaBindsAI::BindGoal(lua_State* L) {
	Goal_CreateMetatable(L);
}

void LuaBindsAI::Goal_CreateMetatable(lua_State* L) {
	luaL_newmetatable(L, "Object.Goal"); // registry[Object.Goal] = {}
	lua_pushvalue(L, -1); // duplicate the table cos setfield will pop it
	lua_setfield(L, -2, "__index"); // mt.__index = mt
	luaL_setfuncs(L, Goal_BindLib, 0); // import all funcs from Goal_BindLib
	lua_pop(L, 1); // pop mt from stack
}

// Leaves userdata on top of the stack.
Goal** LuaBindsAI::Goal_CreateGoalUD(lua_State* L, Goal* goal) {
	Goal** goalPointer = static_cast<Goal**>(lua_newuserdatauv(L, sizeof(Goal*), 0));
	*goalPointer = goal;
	luaL_getmetatable(L, "Object.Goal"); // push mt on top of the stack
	lua_setmetatable(L, -2); // goalPointer was pushed down to 2nd position from the top
	return goalPointer;
}

Goal** LuaBindsAI::Goal_GetGoalObject(lua_State* L) {
	return (Goal**) luaL_checkudata(L, 1, "Object.Goal");
}

void LuaBindsAI::Goal_GrabParams(lua_State* L, int nArgs, std::vector<GoalParamP>& params) {
	if (nArgs > 3)
		for (int i = 4; i <= nArgs; i++)
			if (lua_type(L, i) == LUA_TNUMBER)
				params.push_back(std::make_shared<GoalParamNumber>(lua_tonumber(L, i)));
			else if (lua_type(L, i) == LUA_TBOOLEAN)
				params.push_back(std::make_shared<GoalParamBoolean>(lua_toboolean(L, i)));
			else if (lua_isnil(L, i))
				params.push_back(std::make_shared<GoalParamNil>());
			else if (lua_type(L, i) == LUA_TSTRING)
				// GoalParamString takes care of making a copy
				params.push_back(std::make_shared<GoalParamString>(lua_tostring(L, i)));
			else
				luaL_error(L, "Goal.AddSubGoal - unknown argument type at position %d", i);
}

int LuaBindsAI::Goal_AddSubGoal(lua_State* L) {

	int nArgs = lua_gettop(L);

	if (nArgs < 3) {
		luaL_error(L, "Goal.AddSubGoal - invalid number of arguments. 3 min, %d given", nArgs);
		//return 0;
	}

	Goal** goal = Goal_GetGoalObject(L);
	int goalId = luaL_checknumber(L, 2);
	double life = luaL_checknumber(L, 3);

	// (*goal)->Print();

	std::vector<GoalParamP> params;
	Goal_GrabParams(L, nArgs, params);
	// printf( "%d\n", params.size() );

	Goal** goalUserdata = Goal_CreateGoalUD(L, (*goal)->AddSubGoal(goalId, life, params)); // ud on top of the stack
	// duplicate userdata for return result
	lua_pushvalue(L, -1);
	// save userdata
	(*goalUserdata)->SetRef(luaL_ref(L, LUA_REGISTRYINDEX)); // pops the object as well
	(*goalUserdata)->CreateUsertable();
	return 1;

}

int LuaBindsAI::Goal_ClearSubGoal(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	goal->ClearSubGoal();
	return 0;
}

int LuaBindsAI::Goal_GetLife(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	lua_pushnumber(L, goal->GetLife());
	return 1;
}

int LuaBindsAI::Goal_GetNumber(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	int i = luaL_checkinteger(L, 2);
	if (i > -1 && i < GOAL_NUMBER_COUNT_MAX)
		lua_pushnumber(L, goal->GetNumber(i));
	else
		luaL_error(L, "Goal.GetNumber - number index out of bounds. Allowed - [0, %d)", GOAL_NUMBER_COUNT_MAX);
	return 1;
}

int LuaBindsAI::Goal_GetParam(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	int paramIdx = luaL_checknumber(L, 2);
	if (GoalParam* param = goal->GetParam(paramIdx))
		param->PushToLuaStack(L);
	else
		lua_pushnil(L);
	return 1;
}

int LuaBindsAI::Goal_GetSubGoalNum(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	lua_pushinteger(L, goal->GetSubGoalNum());
	return 1;
}

int LuaBindsAI::Goal_GetTimer(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	int i = luaL_checkinteger(L, 2);
	if (i > -1 && i < GOAL_TIMER_COUNT_MAX)
		lua_pushnumber(L, goal->GetTimer(i));
	else
		luaL_error(L, "Goal.GetTimer - number index out of bounds. Allowed - [0, %d)", GOAL_TIMER_COUNT_MAX);
	return 1;
}

int LuaBindsAI::Goal_GetUserTbl(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, goal->GetUserTblRef());
	return 1;
}

int LuaBindsAI::Goal_IsFinishTimer(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	int i = luaL_checkinteger(L, 2);
	if (i > -1 && i < GOAL_TIMER_COUNT_MAX)
		lua_pushboolean(L, goal->HasTimerFinished(i));
	else
		luaL_error(L, "Goal.IsFinishTimer - number index out of bounds. Allowed - [0, %d)", GOAL_TIMER_COUNT_MAX);
	return 1;
}

int LuaBindsAI::Goal_SetLifeEndSuccess(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	bool b = luaL_checkboolean(L, 2);
	goal->SetSuccessOnLifeEnd(b);
	return 0;
}

int LuaBindsAI::Goal_SetNumber(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	int i = luaL_checkinteger(L, 2);
	double n = luaL_checknumber(L, 3);
	if (i > -1 && i < GOAL_NUMBER_COUNT_MAX)
		goal->SetNumber(i, n);
	else
		luaL_error(L, "Goal.SetNumber - number index out of bounds. Allowed - [0, %d)", GOAL_NUMBER_COUNT_MAX);
	return 0;
}

int LuaBindsAI::Goal_SetTimer(lua_State* L) {
	Goal* goal = *Goal_GetGoalObject(L);
	int i = luaL_checkinteger(L, 2);
	double n = luaL_checknumber(L, 3);
	if (i > -1 && i < GOAL_TIMER_COUNT_MAX)
		goal->SetTimer(i, n);
	else
		luaL_error(L, "Goal.SetTimer - number index out of bounds. Allowed - [0, %d)", GOAL_TIMER_COUNT_MAX);
	return 0;
}




