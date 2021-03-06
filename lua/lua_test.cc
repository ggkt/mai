#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <list>
#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

	static int l_cppfunction(lua_State *L) {
		double arg = luaL_checknumber(L,1);
		lua_pushnumber(L, arg * 0.5);
		return 1;
	}

	static int l_list_push(lua_State *L) { // Push elements from LUA
		assert(lua_gettop(L) == 2); // check that the number of args is exactly 2 
		std::list<int> **ud = static_cast<std::list<int> **>(luaL_checkudata(L,1, "ListMT")); // first arg is the list
		int v =luaL_checkinteger(L,2); // seconds argument is the integer to be pushed to the std::list<int>
		(*ud)->push_back(v); // perform the push on C++ object through the pointer stored in user data
		return 0; // we return 0 values in the lua stack
	}
	static int l_list_pop(lua_State *L) {
		assert(lua_gettop(L) == 1); // check that the number of args is exactly 1
		std::list<int> **ud = static_cast<std::list<int> **>(luaL_checkudata(L, 1, "ListMT")); // first arg is the userdata
		if ((*ud)->empty()) {
			lua_pushnil(L);
			return 1; // if list is empty the function will return nil
		}
		lua_pushnumber(L,(*ud)->front()); // push the value to pop in the lua stack
		// it will be the return value of the function in lua
		(*ud)->pop_front(); // remove the value from the list
		return 1; //we return 1 value in the stack
	}
}

static void test_vars(lua_State *L) {
	std::cout << "** Make a insert a global var into Lua from C++" << std::endl;
	lua_pushnumber(L, 1.1);
	lua_setglobal(L, "cppvar");

	std::cout << "** Execute the Lua chunk" << std::endl;
	if (lua_pcall(L,0, LUA_MULTRET, 0)) {
		std::cerr << "Something went wrong during execution" << std::endl;
		std::cerr << lua_tostring(L, -1) << std::endl;
		lua_pop(L,1);
	}

	std::cout << "** Read a global var from Lua into C++" << std::endl;
	lua_getglobal(L, "luavar");
	double luavar = lua_tonumber(L,-1);
	lua_pop(L,1);
	std::cout << "C++ can read the value set from Lua luavar = " << luavar << std::endl;

}

static std::list<int> *create_object(lua_State *L) {
	auto listObj = new std::list<int>();
	for (int i = 0; i < 5; ++i)
		listObj->push_back(i);
	std::cout << "Set the list object in lua" << std::endl;
	luaL_newmetatable(L, "ListMT");
	lua_pushvalue(L,-1);
	lua_setfield(L,-2, "__index"); // ListMT .__index = ListMT
	lua_pushcfunction(L, l_list_push);
	lua_setfield(L,-2, "push"); // push in lua will call l_list_push in C++
	lua_pushcfunction(L, l_list_pop);
	lua_setfield(L,-2, "pop"); // pop in lua will call l_list_pop in C++

	std::cout << "creating an instance of std::list in lua" << std::endl;
	auto ud = static_cast<std::list<int> **>(lua_newuserdata(L, sizeof(std::list<int> *)));
	*(ud) = listObj;
	luaL_setmetatable(L, "ListMT"); // set userdata metatable
	lua_setglobal(L, "the_list"); // the_list in lua points to the new userdata

	return listObj;
}

static void test_lua_func(lua_State *L) {
	std::cout << "** Execute a Lua function from C++" << std::endl;
	auto listObj = create_object(L);
	lua_getglobal(L, "myluafunction");
	lua_pushnumber(L, 5);
	lua_pcall(L, 1, 1, 0);
	std::cout << "The return value of the function was " << lua_tostring(L, -1) << std::endl;
	lua_pop(L,1);
	for (auto iter = listObj->begin(); iter != listObj->end(); ++iter)
		std::cout << *iter << ", ";
	std::cout << std::endl;
	delete listObj;
}

static void test_cpp_func(lua_State *L) {
	std::cout << "** Execute a C++ function from Lua" << std::endl;
	std::cout << "**** First register the function in Lua" << std::endl;
	lua_pushcfunction(L,l_cppfunction);
	lua_setglobal(L, "cppfunction");

	std::cout << "**** Call a Lua function that uses the C++ function" << std::endl;
	lua_getglobal(L, "myfunction");
	lua_pushnumber(L, 5);
	lua_pcall(L, 1, 1, 0);
	std::cout << "The return value of the function was " << lua_tonumber(L, -1) << std::endl;
	lua_pop(L,1);
}

static void test_buf_run(lua_State *L) {
	char buf[1024];
	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		auto error = luaL_loadbuffer(L, buf, strlen(buf), "line") ||
			lua_pcall(L, 0, 0, 0);
		if (error) {
			fprintf(stderr, "%s", lua_tostring(L, -1));
			lua_pop(L, 1);  /* pop error message from the stack */
		}
	}

}

lua_State *init_lua() {
	std::cout << "** Init Lua" << std::endl;
	lua_State *L;
	L = luaL_newstate();
	std::cout << "** Load the (optional) standard libraries, to have the print function" << std::endl;
	luaL_openlibs(L);
	std::cout << "** Load chunk. without executing it" << std::endl;
	if (luaL_loadfile(L, "test.lua")) {
		std::cerr << "Something went wrong loading the chunk (syntax error?)" << std::endl;
		std::cerr << lua_tostring(L, -1) << std::endl;
		lua_pop(L,1);
	}
	return L;
}

int main() {
	std::cout << "** Test Lua embedding" << std::endl;
	auto L = init_lua();

	test_vars(L);
	test_lua_func(L);
	test_cpp_func(L);

	std::cout << "** Release the Lua enviroment" << std::endl;
	lua_close(L);
}
