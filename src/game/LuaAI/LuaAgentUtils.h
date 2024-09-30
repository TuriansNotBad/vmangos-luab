#ifndef MANGOS_LuaAgentUtils_H
#define MANGOS_LuaAgentUtils_H

struct lua_State;

int lua_dopcall(lua_State* L, int narg, int nres);
bool luaL_checkboolean(lua_State* L, int idx);
void lua_pushplayerornil(lua_State* L, Player* u);
void lua_pushunitornil(lua_State* L, Unit* u);
void* luaL_checkudwithfield(lua_State* L, int idx, const char* fieldName);
bool luaL_setnfromfield(lua_State* L, int idx, const char* fieldName, float& n);
bool luaL_setsfromfield(lua_State* L, int idx, const char* fieldName, std::string& s);

class lua_RefHolder
{
	int m_ref;
	int m_type;
public:
	lua_RefHolder(int type);
	bool Ref(lua_State* L);
	int Push(lua_State* L);
	void Unref(lua_State* L);
};

#endif
