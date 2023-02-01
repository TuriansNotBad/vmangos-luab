#include "GoalParam.h"

// debug
GoalParam::~GoalParam() {
	//printf( "GoalParam destroyed\n" );
}

//-----------------------------------------------
// Boolean param
//-----------------------------------------------

// Constructs a GoalParamBoolean object with passed value.
GoalParamBoolean::GoalParamBoolean( bool value ) : value( value ) {}

// Returns a string representation of the value stored inside the object. (false or true)
std::string GoalParamBoolean::ToString() {
	if ( value == false )
		return std::string( "false" );
	else
		return std::string( "true" );
}

// Calls lua_pushboolean on the value stored.
void GoalParamBoolean::PushToLuaStack( lua_State* L ) {
	lua_pushboolean( L, value );
}

//-----------------------------------------------
// String param
//-----------------------------------------------

// Constructs a GoalParamString object with passed value.
GoalParamString::GoalParamString( const char* value ) : value( value ) {}

// Returns a copy of the string stored inside the object.
std::string GoalParamString::ToString() {
	return std::string( value );
}

// Calls lua_pushstring on the value stored.
void GoalParamString::PushToLuaStack( lua_State* L ) {
	lua_pushstring( L, value.c_str() );
}

//-----------------------------------------------
// Number param
//-----------------------------------------------

// Constructs a GoalParamNumber object with passed value.
GoalParamNumber::GoalParamNumber( lua_Number value ) : value( value ) {}

// Returns a string representation of a number stored inside by calling std::to_string.
std::string GoalParamNumber::ToString() {
	return std::to_string( value );
}

// Calls lua_pushnumber on the value stored.
void GoalParamNumber::PushToLuaStack( lua_State* L ) {
	lua_pushnumber( L, value );
}

//-----------------------------------------------
// Nil param
//-----------------------------------------------

// Constructs a GoalParamNil object.
GoalParamNil::GoalParamNil() {};

// Returns a string "nil".
std::string GoalParamNil::ToString() {
	return "nil";
}

// Calls lua_pushnil.
void GoalParamNil::PushToLuaStack( lua_State* L ) {
	lua_pushnil( L );
}

