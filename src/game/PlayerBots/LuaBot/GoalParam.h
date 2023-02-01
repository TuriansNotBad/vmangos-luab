#pragma once
#include <lua.hpp>

// Abstract class to store a goal param value. (which can be a lua's nil, number, string, boolean, table, function)
class GoalParam {

public:
	// Returns a string representation of the value stored inside the object.
	virtual std::string ToString() = 0;
	// Calls the appropriate lua_push function for the value stored.
	virtual void PushToLuaStack( lua_State* L ) = 0;
	~GoalParam();

};

//-----------------------------------------------
// Boolean param
//-----------------------------------------------

class GoalParamBoolean : public GoalParam {

	bool value;

public:
	// Constructs a GoalParamBoolean object with passed value.
	GoalParamBoolean( bool value );
	// Returns a string representation of the value stored inside the object. (false or true)
	std::string ToString();
	// Calls lua_pushboolean on the value stored.
	void PushToLuaStack( lua_State* L );

};

//-----------------------------------------------
// String param
//-----------------------------------------------

class GoalParamString : public GoalParam {

	std::string value;

public:
	// Constructs a GoalParamString object with passed value.
	GoalParamString( const char* value );
	// Returns a copy of the string stored inside the object.
	std::string ToString();
	// Calls lua_pushstring on the value stored.
	void PushToLuaStack( lua_State* L );

};

//-----------------------------------------------
// Number param
//-----------------------------------------------

class GoalParamNumber : public GoalParam {

	lua_Number value;

public:
	// Constructs a GoalParamNumber object with passed value.
	GoalParamNumber( lua_Number value );
	// Returns a string representation of a number stored inside by calling std::to_string.
	std::string ToString();
	// Calls lua_pushnumber on the value stored.
	void PushToLuaStack( lua_State* L );

};

//-----------------------------------------------
// Nil param
//-----------------------------------------------

class GoalParamNil : public GoalParam {

public:
	// Constructs a GoalParamNil.
	GoalParamNil();
	// Returns a string "nil".
	std::string ToString();
	// Calls lua_pushnil.
	void PushToLuaStack( lua_State* L );

};
