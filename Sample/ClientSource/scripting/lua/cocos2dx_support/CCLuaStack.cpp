/****************************************************************************
 Copyright (c) 2011 cocos2d-x.org
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "CCLuaStack.h"

extern "C" {
#include "lua.h"
#include "tolua++.h"
#include "lualib.h"
#include "lauxlib.h"
#include "tolua_fix.h"
}

#include "LuaCocos2d.h"

#include "Cocos2dxLuaLoader.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
#include "platform/ios/CCLuaObjcBridge.h"
#endif
extern int  tolua_ScutScene_open (lua_State* tolua_S);
extern int  tolua_ScutDataLogic_open (lua_State* tolua_S);
extern int  tolua_ScutSystem_open (lua_State* tolua_S);
extern int  tolua_ScutUtility_open(lua_State* tolua_S);
extern int  luaopen_ScutAnimation(lua_State* tolua_S);
namespace {
int lua_print(lua_State * luastate)
{
    int nargs = lua_gettop(luastate);

    std::string t;
    for (int i=1; i <= nargs; i++)
    {
        if (lua_istable(luastate, i))
            t += "table";
        else if (lua_isnone(luastate, i))
            t += "none";
        else if (lua_isnil(luastate, i))
            t += "nil";
        else if (lua_isboolean(luastate, i))
        {
            if (lua_toboolean(luastate, i) != 0)
                t += "true";
            else
                t += "false";
        }
        else if (lua_isfunction(luastate, i))
            t += "function";
        else if (lua_islightuserdata(luastate, i))
            t += "lightuserdata";
        else if (lua_isthread(luastate, i))
            t += "thread";
        else
        {
            const char * str = lua_tostring(luastate, i);
            if (str)
                t += lua_tostring(luastate, i);
            else
                t += lua_typename(luastate, lua_type(luastate, i));
        }
        if (i!=nargs)
            t += "\t";
    }
    CCLOG("[LUA-print] %s", t.c_str());

    return 0;
}
}  // namespace {

NS_CC_BEGIN

CCLuaStack *CCLuaStack::create(void)
{
    CCLuaStack *stack = new CCLuaStack();
    stack->init();
    stack->autorelease();
    return stack;
}

CCLuaStack *CCLuaStack::attach(lua_State *L)
{
    CCLuaStack *stack = new CCLuaStack();
    stack->initWithLuaState(L);
    stack->autorelease();
    return stack;
}

bool CCLuaStack::init(void)
{
    m_state = lua_open();
    luaL_openlibs(m_state);
    tolua_Cocos2d_open(m_state);
    toluafix_open(m_state);
	tolua_ScutDataLogic_open(m_state);
	tolua_ScutScene_open(m_state);
	tolua_ScutSystem_open(m_state);
	tolua_ScutUtility_open(m_state);
	luaopen_ScutAnimation(m_state);
    // Register our version of the global "print" function
    const luaL_reg global_functions [] = {
        {"print", lua_print},
        {NULL, NULL}
    };
    luaL_register(m_state, "_G", global_functions);

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
    CCLuaObjcBridge::luaopen_luaoc(m_state);
#endif
    
    // add cocos2dx loader

    return true;
}

bool CCLuaStack::initWithLuaState(lua_State *L)
{
    m_state = L;
    return true;
}

void CCLuaStack::addSearchPath(const char* path)
{
    lua_getglobal(m_state, "package");                                  /* L: package */
    lua_getfield(m_state, -1, "path");                /* get package.path, L: package path */
    const char* cur_path =  lua_tostring(m_state, -1);
    lua_pushfstring(m_state, "%s;%s/?.lua", cur_path, path);            /* L: package path newpath */
    lua_setfield(m_state, -3, "path");          /* package.path = newpath, L: package path */
    lua_pop(m_state, 2);                                                /* L: - */
}

void CCLuaStack::addLuaLoader(lua_CFunction func)
{
    if (!func) return;
    
    // stack content after the invoking of the function
    // get loader table
    lua_getglobal(m_state, "package");                                  /* L: package */
    lua_getfield(m_state, -1, "loaders");                               /* L: package, loaders */
    
    // insert loader into index 2
    lua_pushcfunction(m_state, func);                                   /* L: package, loaders, func */
    for (int i = lua_objlen(m_state, -2) + 1; i > 2; --i)
    {
        lua_rawgeti(m_state, -2, i - 1);                                /* L: package, loaders, func, function */
        // we call lua_rawgeti, so the loader table now is at -3
        lua_rawseti(m_state, -3, i);                                    /* L: package, loaders, func */
    }
    lua_rawseti(m_state, -2, 2);                                        /* L: package, loaders */
    
    // set loaders into package
    lua_setfield(m_state, -2, "loaders");                               /* L: package */
    
    lua_pop(m_state, 1);
}


void CCLuaStack::removeScriptObjectByCCObject(CCObject* pObj)
{
    toluafix_remove_ccobject_by_refid(m_state, pObj->m_nLuaID);
}

void CCLuaStack::removeScriptHandler(int nHandler)
{
    toluafix_remove_function_by_refid(m_state, nHandler);
}

int CCLuaStack::executeString(const char *codes)
{
    luaL_loadstring(m_state, codes);
    return executeFunction(0);
}

int CCLuaStack::executeScriptFile(const char* filename)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    std::string code("require \"");
    code.append(filename);
    code.append("\"");
    return executeString(code.c_str());
#else
    std::string fullPath = CCFileUtils::sharedFileUtils()->fullPathForFilename(filename);
    ++m_callFromLua;
    int nRet = luaL_dofile(m_state, fullPath.c_str());
    --m_callFromLua;
    CC_ASSERT(m_callFromLua >= 0);
    // lua_gc(m_state, LUA_GCCOLLECT, 0);
    
    if (nRet != 0)
    {
        CCLOG("[LUA ERROR] %s", lua_tostring(m_state, -1));
        lua_pop(m_state, 1);
        return nRet;
    }
    return 0;
#endif
}

int CCLuaStack::executeGlobalFunction(const char* functionName)
{
    lua_getglobal(m_state, functionName);       /* query function by name, stack: function */
    if (!lua_isfunction(m_state, -1))
    {
        CCLOG("[LUA ERROR] name '%s' does not represent a Lua function", functionName);
        lua_pop(m_state, 1);
        return 0;
    }
    return executeFunction(0);
}

void CCLuaStack::clean(void)
{
    lua_settop(m_state, 0);
}

void CCLuaStack::pushInt(int intValue)
{
    lua_pushinteger(m_state, intValue);
}

void CCLuaStack::pushFloat(float floatValue)
{
    lua_pushnumber(m_state, floatValue);
}

void CCLuaStack::pushBoolean(bool boolValue)
{
    lua_pushboolean(m_state, boolValue);
}

void CCLuaStack::pushString(const char* stringValue)
{
    lua_pushstring(m_state, stringValue);
}

void CCLuaStack::pushString(const char* stringValue, int length)
{
    lua_pushlstring(m_state, stringValue, length);
}

void CCLuaStack::pushNil(void)
{
    lua_pushnil(m_state);
}

void CCLuaStack::pushCCObject(CCObject* objectValue, const char* typeName)
{
    toluafix_pushusertype_ccobject(m_state, objectValue->m_uID, &objectValue->m_nLuaID, objectValue, typeName);
}

void CCLuaStack::pushCCLuaValue(const CCLuaValue& value)
{
    const CCLuaValueType type = value.getType();
    if (type == CCLuaValueTypeInt)
    {
        return pushInt(value.intValue());
    }
    else if (type == CCLuaValueTypeFloat)
    {
        return pushFloat(value.floatValue());
    }
    else if (type == CCLuaValueTypeBoolean)
    {
        return pushBoolean(value.booleanValue());
    }
    else if (type == CCLuaValueTypeString)
    {
        return pushString(value.stringValue().c_str());
    }
    else if (type == CCLuaValueTypeDict)
    {
        pushCCLuaValueDict(value.dictValue());
    }
    else if (type == CCLuaValueTypeArray)
    {
        pushCCLuaValueArray(value.arrayValue());
    }
    else if (type == CCLuaValueTypeCCObject)
    {
        pushCCObject(value.ccobjectValue(), value.getCCObjectTypename().c_str());
    }
}

void CCLuaStack::pushCCLuaValueDict(const CCLuaValueDict& dict)
{
    lua_newtable(m_state);                                              /* L: table */
    for (CCLuaValueDictIterator it = dict.begin(); it != dict.end(); ++it)
    {
        lua_pushstring(m_state, it->first.c_str());                     /* L: table key */
        pushCCLuaValue(it->second);                                     /* L: table key value */
        lua_rawset(m_state, -3);                     /* table.key = value, L: table */
    }
}

void CCLuaStack::pushCCLuaValueArray(const CCLuaValueArray& array)
{
    lua_newtable(m_state);                                              /* L: table */
    int index = 1;
    for (CCLuaValueArrayIterator it = array.begin(); it != array.end(); ++it)
    {
        pushCCLuaValue(*it);                                            /* L: table value */
        lua_rawseti(m_state, -2, index);          /* table[index] = value, L: table */
        ++index;
    }
}

bool CCLuaStack::pushFunctionByHandler(int nHandler)
{
    toluafix_get_function_by_refid(m_state, nHandler);                  /* L: ... func */
    if (!lua_isfunction(m_state, -1))
    {
        CCLOG("[LUA ERROR] function refid '%d' does not reference a Lua function", nHandler);
        lua_pop(m_state, 1);
        return false;
    }
    return true;
}

int CCLuaStack::executeFunction(int numArgs)
{
    int functionIndex = -(numArgs + 1);
    if (!lua_isfunction(m_state, functionIndex))
    {
        CCLOG("value at stack [%d] is not function", functionIndex);
        lua_pop(m_state, numArgs + 1); // remove function and arguments
        return 0;
    }

    int traceback = 0;
    lua_getglobal(m_state, "__G__TRACKBACK__");                         /* L: ... func arg1 arg2 ... G */
    if (!lua_isfunction(m_state, -1))
    {
        lua_pop(m_state, 1);                                            /* L: ... func arg1 arg2 ... */
    }
    else
    {
        lua_insert(m_state, functionIndex - 1);                         /* L: ... G func arg1 arg2 ... */
        traceback = functionIndex - 1;
    }
    
    int error = 0;
    ++m_callFromLua;
    error = lua_pcall(m_state, numArgs, 1, traceback);//ndlua_pcall(m_state, numArgs, 1);//lua_pcall(m_state, numArgs, 1, traceback);                  /* L: ... [G] ret */
    --m_callFromLua;
    if (error)
    {
        if (traceback == 0)
        {
            CCLOG("[LUA ERROR] %s", lua_tostring(m_state, - 1));        /* L: ... error */
            lua_pop(m_state, 1); // remove error message from stack
        }
        else                                                            /* L: ... G error */
        {
            lua_pop(m_state, 2); // remove __G__TRACKBACK__ and error message from stack
        }
        return 0;
    }
    
    // get return value
    int ret = 0;
    if (lua_isnumber(m_state, -1))
    {
        ret = lua_tointeger(m_state, -1);
    }
    else if (lua_isboolean(m_state, -1))
    {
        ret = lua_toboolean(m_state, -1);
    }
    // remove return value from stack
    lua_pop(m_state, 1);                                                /* L: ... [G] */
    
    if (traceback)
    {
        lua_pop(m_state, 1); // remove __G__TRACKBACK__ from stack      /* L: ... */
    }
    
    return ret;
}

int CCLuaStack::executeFunctionByHandler(int nHandler, int numArgs)
{
    int ret = 0;
    if (pushFunctionByHandler(nHandler))                                /* L: ... arg1 arg2 ... func */
    {
        if (numArgs > 0)
        {
            lua_insert(m_state, -(numArgs + 1));                        /* L: ... func arg1 arg2 ... */
        }
        ret = executeFunction(numArgs);
    }
    lua_settop(m_state, 0);
    return ret;
}

bool CCLuaStack::handleAssert(const char *msg)
{
    if (m_callFromLua == 0) return false;
    
    lua_pushfstring(m_state, "ASSERT FAILED ON LUA EXECUTE: %s", msg ? msg : "unknown");
    lua_error(m_state);
    return true;
}

int CCLuaStack::reallocateScriptHandler(int nHandler)
{
    LUA_FUNCTION  nNewHandle = -1;
    
    if (pushFunctionByHandler(nHandler))
    {
       nNewHandle = toluafix_ref_function(m_state,lua_gettop(m_state),0);
    }
/*
    toluafix_get_function_by_refid(m_state,nNewHandle);
    if (!lua_isfunction(m_state, -1))
    {
        CCLOG("Error!");
    }
    lua_settop(m_state, 0);
*/
    return nNewHandle;

}

bool CCLuaStack::pushfunc(const std::string & strFunc)
{
	size_t found = strFunc.find('.');
	if (found == std::string::npos)
	{
		found = strFunc.find(':');
	}
	if (found == std::string::npos)
	{
		lua_getglobal(m_state, strFunc.c_str());
	}
	else
	{
		std::string strTable = strFunc.substr(0, found);
		std::string strFuncName  = strFunc.substr(found + 1);

		lua_getglobal(m_state, strTable.c_str());                        // Get the table instance
		int r_obj = luaL_ref(m_state, LUA_REGISTRYINDEX);

		lua_rawgeti(m_state, LUA_REGISTRYINDEX, r_obj);   // Get the object instance
		lua_getfield(m_state, -1, strFuncName.c_str());    // Get the function
		int r_func = luaL_ref(m_state, LUA_REGISTRYINDEX);
		lua_pop(m_state, 1);
		lua_rawgeti(m_state, LUA_REGISTRYINDEX, r_func);
	}

	// is it a function
	if ( !lua_isfunction(m_state,-1) )
	{
		lua_settop( m_state, 0 );
		std::string msg = "(CCLuaScriptModule): "+strFunc +" name does not represent a Lua function"+"\n";
		CCLog("%s  %d", msg.c_str(), __LINE__);
		return false;
	}
	return true;
}

void CCLuaStack::executeLogEvent( std::string& func, std::string& errlog )
{
	if(!pushfunc(func))
	{
		return;
	}

	lua_pushstring(m_state, errlog.c_str());

	if (lua_pcall(m_state, 1, 0, 0) != 0)
	{
		CCLog("[LUA ERROR] %s", lua_tostring(m_state, - 1));
		lua_pop(m_state, 1); // clean error message
		return;
	}

	return;
}

NS_CC_END

#include "CCLuaEngine.h"

int nderror_handler(lua_State* L)
{
	lua_Debug debug_info;
	int level = 0;
	std::string err;
	char tmp[10];

	if(lua_gettop(L) > 0 && lua_isstring(L, -1))
	{
		err += lua_tostring(L, -1);
	}

	while(lua_getstack(L, level++, &debug_info))
	{
		lua_getinfo(L, "l", &debug_info);
		lua_getinfo(L, "n", &debug_info);
		lua_getinfo(L, "S", &debug_info);

		err += '\n';
		err += '[';
		err += debug_info.what;
		err += "][";
		err += debug_info.namewhat;
		err += "][";
		//err += _itoa(debug_info.currentline, tmp, 10);
		err += "][";
		if(debug_info.name) err += debug_info.name;
		err += "]@[";
		err += debug_info.source;
		err += ']';
	}

	std::string& err_handler = cocos2d::CCDirector::sharedDirector()->GetErrorHandler();
	if(err_handler.size() > 0)
	{
		cocos2d::CCLuaEngine::defaultEngine()->getLuaStack()->executeLogEvent(err_handler, err);
	}

	return 1;
}


extern "C" LUA_DLL int Scutlua_pcall(lua_State *L, int nargs, int nresults)
{
	lua_pushcfunction(L, nderror_handler);
	lua_insert(L, -2-nargs);

	return lua_pcall(L, nargs, nresults, -2-nargs);
}

