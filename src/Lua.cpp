#include "Lua.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
}

template <SDispatchResult (*DispatcherFunc)(std::string)>
int luaDispatcherWrapper(lua_State* L) {
    const std::string arg = luaL_optstring(L, 1, "");
    SDispatchResult result = DispatcherFunc(arg);
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.error.c_str());
        return 2;
    }
    return 1;
}

void registerLuaBindings(HANDLE handle) {
    if (Config::mgr() && Config::mgr()->type() == Config::CONFIG_LUA) {
        HyprlandAPI::addLuaFunction(handle, "overview", "toggle", luaDispatcherWrapper<Dispatchers::dispatchToggleOverview>);
        HyprlandAPI::addLuaFunction(handle, "overview", "open", luaDispatcherWrapper<Dispatchers::dispatchOpenOverview>);
        HyprlandAPI::addLuaFunction(handle, "overview", "close", luaDispatcherWrapper<Dispatchers::dispatchCloseOverview>);
    }
}