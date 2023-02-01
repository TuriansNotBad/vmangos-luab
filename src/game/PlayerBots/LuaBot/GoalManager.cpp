#include "GoalManager.h"
#include "LuaBotAI.h"

std::unordered_map<int, GoalInfo> GoalManager::goalInfoData;

GoalManager::GoalManager() {
	// implement me
}

void GoalManager::RegisterGoal( int goalId, std::string name ) {
	goalInfoData.emplace( goalId,
		GoalInfo( name, name + GOAL_ACTIVATE_POSTFIX, name + GOAL_UPDATE_POSTFIX, name + GOAL_TERMINATE_POSTFIX ) );
}

void GoalManager::Activate( lua_State* L, LuaBotAI* ai ) {
	while ( !activationStack.empty() ) {
		Goal* goal = activationStack.top();
		// remove from stack
		activationStack.pop();
		// goal->Print();
		// terminated goal can sneak onto the stack if parent goal dies before all of its kids are executed
		// when parent is executed the topmost child is also but the termination function puts next child on top
		// and that child is queued for activation
		if ( !goal->GetActivated() && !goal->GetTerminated() ) {

			// call the activation function, set the flag
			goal->SetActivated( true );
			lua_getglobal( L, goalInfoData[goal->GetGoalId()].fActivate.c_str() );

			// get ai userdata
			lua_rawgeti( L, LUA_REGISTRYINDEX, ai->GetRef() );

			// temp, check if userdata is valid and create if needed
			if ( goal->GetRef() == LUA_NOREF || goal->GetRef() == LUA_REFNIL ) {
				LuaBindsAI::Goal_CreateGoalUD( L, goal ); // put ud on top of the stack
				lua_pushvalue( L, -1 ); // dup ud
				goal->SetRef( luaL_ref( L, LUA_REGISTRYINDEX ) ); // pop ud, save for later
			}
			else
				// get goal's userdata
				lua_rawgeti( L, LUA_REGISTRYINDEX, goal->GetRef() );

			if ( lua_pcall( L, 2, 0, 0 ) != LUA_OK ) {
				sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Lua error activating goal id %d: %s\n", goal->GetGoalId(), lua_tostring( L, -1 ) );
				lua_pop( L, 1 ); // pop the error object
				ai->CeaseUpdates();
				return;
			}

		}
		else
			printf( "Trying to activate unqualified goal %d: A: %d T: %d\n", goal->GetGoalId(), goal->GetActivated(), goal->GetTerminated() );
	}
}

void GoalManager::UpdateRecursive( lua_State* L, LuaBotAI* ai, Goal* goal ) {

	// check if valid
	if ( !goal->GetActivated() || goal->GetTerminated() ) {
		printf( "Trying to update a goal that isn't active or terminated\n" );
		return;
	}

	// call for each active subgoal
	if ( goal->GetSubGoalNum() > 0 )
		UpdateRecursive( L, ai, goal->GetActiveSubGoal() );

	// there was a lua error in one of the update calls
	if (ai->ceaseUpdates) return;

	// printf( "%d goal life: %f\n", goal->GetGoalId(), goal->GetLife() );
	// check life expired or result concluded
	if ( (goal->GetLife() <= 0 && goal->CanExpire()) || goal->GetLastResult() != GoalResult::Continue) {
		// terminate
		// flag as done
		if ( goal->GetLastResult() == GoalResult::Continue )
			goal->SetLastResult( GoalResult::Failed );
		// push to termination stack
		PushGoalOnTerminationQueue( goal );
	}
	else {
		// update
		lua_getglobal( L, goalInfoData[goal->GetGoalId()].fUpdate.c_str() );

		// get ai userdata
		lua_rawgeti( L, LUA_REGISTRYINDEX, ai->GetRef() );

		// temp, check if userdata is valid and create if needed
		if ( goal->GetRef() == LUA_NOREF || goal->GetRef() == LUA_REFNIL ) {
			LuaBindsAI::Goal_CreateGoalUD( L, goal ); // put ud on top of the stack
			lua_pushvalue( L, -1 ); // dup ud
			goal->SetRef( luaL_ref( L, LUA_REGISTRYINDEX ) ); // pop ud, save for later
		}
		else
			// get goal's userdata
			lua_rawgeti( L, LUA_REGISTRYINDEX, goal->GetRef() );

		if ( lua_pcall( L, 2, 1, 0 ) != LUA_OK ) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Lua error updating goal id %d: %s\n", goal->GetGoalId(), lua_tostring(L, -1));
			lua_pop( L, 1 ); // pop the error object
			ai->CeaseUpdates();
			return;
		}

		// grab return from the call
		int isnum;
		int result = lua_tonumberx( L, -1, &isnum );
		lua_pop(L, 1);
		if ( !isnum ) {
			//throw "Goal Update must return a number - " + goalInfoData[goal->GetGoalId()].fUpdate;
			printf("Goal Update must return a number - %s\n", goalInfoData[goal->GetGoalId()].fUpdate.c_str());
			ai->CeaseUpdates();
			return;
		}

		switch ( result ) {
		case GOAL_RESULT_Continue:
			goal->SetLastResult( GoalResult::Continue );
			break;
		case GOAL_RESULT_Success:
			goal->SetLastResult( GoalResult::Success );
			break;
		case GOAL_RESULT_Failed:
			goal->SetLastResult( GoalResult::Failed );
			break;
		default: {
			printf("Goal Update returned unknown value - %s\n", goalInfoData[goal->GetGoalId()].fUpdate.c_str());
			ai->CeaseUpdates();
		}
		}

	}

}

void GoalManager::Update( lua_State* L, LuaBotAI* ai ) {
	UpdateRecursive( L, ai, ai->GetTopGoal() );
}

void GoalManager::TerminateRecursive( lua_State* L, LuaBotAI* ai, Goal* goal ) {
	if ( goal->GetSubGoalNum() > 0 ) {
		Goal* subgoal = goal->GetActiveSubGoal();
		goal->SetTerminationState();
		TerminateRecursive( L, ai, subgoal );
	}
	if (ai->ceaseUpdates) return;
	// maybe its still possible for terminated goal to appear here if parent and its children expire together
	if ( goal->GetActivated() && !goal->GetTerminated() ) {
		lua_getglobal( L, goalInfoData[goal->GetGoalId()].fTerminate.c_str() );
		lua_rawgeti( L, LUA_REGISTRYINDEX, ai->GetRef() );

		// temp, check if userdata is valid and create if needed
		if (goal->GetRef() == LUA_NOREF || goal->GetRef() == LUA_REFNIL) {
			LuaBindsAI::Goal_CreateGoalUD(L, goal); // put ud on top of the stack
			lua_pushvalue(L, -1); // dup ud
			goal->SetRef(luaL_ref(L, LUA_REGISTRYINDEX)); // pop ud, save for later
		}
		else
			// get goal's userdata
			lua_rawgeti(L, LUA_REGISTRYINDEX, goal->GetRef());

		if ( lua_pcall( L, 2, 0, 0 ) != LUA_OK ) {
			sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Lua error terminating goal id %d: %s\n", goal->GetGoalId(), lua_tostring(L, -1));
			lua_pop( L, 1 ); // pop the error object
			ai->CeaseUpdates();
		}
	}
	else
		printf( "GoalManager: Trying to terminate goal that wasn't activated or has been terminated, id = %d, skipping\n", goal->GetGoalId() );
	goal->SetTerminated( true );
}

void GoalManager::Terminate( lua_State* L, LuaBotAI* ai ) {
	while ( !terminationQueue.empty() ) {
		Goal* goal = terminationQueue.front();
		terminationQueue.pop_front();
		if ( goal->GetLastResult() == GoalResult::Continue )
			goal->SetLastResult( GoalResult::Failed );
		// call recursive
		TerminateRecursive( L, ai, goal );
		// notify parent we've finished
		if ( goal->GetParent() != NULL )
			goal->GetParent()->UpdateSubGoal();
	}
}

void GoalManager::PushGoalOnActivationStack( Goal* goal ) {
	activationStack.push( goal );
}

void GoalManager::PushGoalOnTerminationQueue( Goal* goal ) {
	terminationQueue.push_back( goal );
	goal->SetTerminationState();
}

void GoalManager::PrintRegistered() {
	for ( auto v : goalInfoData )
	{
		printf( "ID: %d\n", v.first );
		printf( "Name: %s\n", v.second.name.c_str() );
		printf( "fA: %s\n", v.second.fActivate.c_str() );
		printf( "fU: %s\n", v.second.fUpdate.c_str() );
		printf( "fT: %s\n", v.second.fTerminate.c_str() );
		printf( "-------------------------------\n" );
	}
}

// ---------------------------------------------------------
//    LUA BINDS
// ---------------------------------------------------------

void LuaBindsAI::BindGoalManager( lua_State* L ) {
	lua_register( L, "REGISTER_GOAL", REGISTER_GOAL );
}

int LuaBindsAI::REGISTER_GOAL( lua_State* L ) {
	GoalManager::RegisterGoal( luaL_checknumber( L, 1 ), luaL_checkstring( L, 2 ) );
	return 0;
}
