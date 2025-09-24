/*
 * This file is in the public domain.
 *
 * Author: Jules
 */

#include <stdio.h>
#include <stdlib.h>
#include "def.h"
#include "funmap.h"
#include "kbd.h"

#ifdef HAVE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_LUA_CMDS 32

static lua_State *L = NULL;
static int lua_fn_refs[MAX_LUA_CMDS];
static int next_lua_fn_ref = 0;

/* Forward declarations */
int eval_lua_file_cmd(int, int);

/*
 * Dispatcher functions.
 */
#define LUA_CMD_FN(n) \
static int lua_cmd_ ## n(int f, int n_arg) { \
    if (L == NULL || lua_fn_refs[n] == LUA_REFNIL) return FALSE; \
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_fn_refs[n]); \
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) { \
        dobeep(); \
        ewprintf("Lua error: %s", lua_tostring(L, -1)); \
        lua_pop(L, 1); \
        return FALSE; \
    } \
    return TRUE; \
}

/* Generate the dispatcher functions */
LUA_CMD_FN(0) LUA_CMD_FN(1) LUA_CMD_FN(2) LUA_CMD_FN(3)
LUA_CMD_FN(4) LUA_CMD_FN(5) LUA_CMD_FN(6) LUA_CMD_FN(7)
LUA_CMD_FN(8) LUA_CMD_FN(9) LUA_CMD_FN(10) LUA_CMD_FN(11)
LUA_CMD_FN(12) LUA_CMD_FN(13) LUA_CMD_FN(14) LUA_CMD_FN(15)
LUA_CMD_FN(16) LUA_CMD_FN(17) LUA_CMD_FN(18) LUA_CMD_FN(19)
LUA_CMD_FN(20) LUA_CMD_FN(21) LUA_CMD_FN(22) LUA_CMD_FN(23)
LUA_CMD_FN(24) LUA_CMD_FN(25) LUA_CMD_FN(26) LUA_CMD_FN(27)
LUA_CMD_FN(28) LUA_CMD_FN(29) LUA_CMD_FN(30) LUA_CMD_FN(31)

static PF lua_cmd_fns[] = {
    lua_cmd_0, lua_cmd_1, lua_cmd_2, lua_cmd_3,
    lua_cmd_4, lua_cmd_5, lua_cmd_6, lua_cmd_7,
    lua_cmd_8, lua_cmd_9, lua_cmd_10, lua_cmd_11,
    lua_cmd_12, lua_cmd_13, lua_cmd_14, lua_cmd_15,
    lua_cmd_16, lua_cmd_17, lua_cmd_18, lua_cmd_19,
    lua_cmd_20, lua_cmd_21, lua_cmd_22, lua_cmd_23,
    lua_cmd_24, lua_cmd_25, lua_cmd_26, lua_cmd_27,
    lua_cmd_28, lua_cmd_29, lua_cmd_30, lua_cmd_31,
};

/*
 * Lua-callable function to define a new mg command.
 * mg.defun("my-command", function() ... end)
 */
static int
l_defun(lua_State *ls)
{
    const char *name;
    int fun_ref;

    if (next_lua_fn_ref >= MAX_LUA_CMDS) {
        lua_pushstring(ls, "Too many Lua commands defined");
        lua_error(ls);
        return 0;
    }

    name = luaL_checkstring(ls, 1);
    luaL_checktype(ls, 2, LUA_TFUNCTION);

    lua_pushvalue(ls, 2);
    fun_ref = luaL_ref(ls, LUA_REGISTRYINDEX);

    lua_fn_refs[next_lua_fn_ref] = fun_ref;
    funmap_add(lua_cmd_fns[next_lua_fn_ref], name, 0);

    next_lua_fn_ref++;

    return 0;
}

/*
 * mg.bind_key("map_name", "key", "function_name")
 */
static int
l_bind_key(lua_State *ls)
{
    const char *map_name, *key, *func_name;
    KEYMAP *map;

    map_name = luaL_checkstring(ls, 1);
    key = luaL_checkstring(ls, 2);
    func_name = luaL_checkstring(ls, 3);

    map = name_map(map_name);
    if (map == NULL) {
        lua_pushstring(ls, "Unknown keymap");
        lua_error(ls);
        return 0;
    }

    if (dobindkey(map, func_name, key) != TRUE) {
        lua_pushstring(ls, "Failed to bind key");
        lua_error(ls);
        return 0;
    }

    return 0;
}

/*
 * mg.new_keymap("map_name")
 */
static int
l_new_keymap(lua_State *ls)
{
    const char *name;
    KEYMAP *map;

    name = luaL_checkstring(ls, 1);

    map = malloc(sizeof(KEYMAP) + (MAPINIT - 1) * sizeof(struct map_element));
    if (map == NULL) {
        lua_pushstring(ls, "Out of memory");
        lua_error(ls);
        return 0;
    }
    map->map_num = 0;
    map->map_max = MAPINIT;
    map->map_default = rescan; /* Default to rescan for new maps */

    if (maps_add(map, name) != TRUE) {
        free(map);
        lua_pushstring(ls, "Failed to add keymap");
        lua_error(ls);
        return 0;
    }

    return 0;
}

/*
 * mg.set_mode("mode_name")
 */
static int
l_set_mode(lua_State *ls)
{
    const char *mode_name;
    struct maps_s *mp;

    mode_name = luaL_checkstring(ls, 1);
    mp = name_mode(mode_name);

    if (mp == NULL) {
        lua_pushstring(ls, "Unknown mode");
        lua_error(ls);
        return 0;
    }

    /* For simplicity, we only support one mode for now. */
    curbp->b_nmodes = 0;
    curbp->b_modes[0] = mp;
    curwp->w_flag |= WFMODE;

    return 0;
}


/*
 * Wrapper functions to be called from Lua.
 */

static int
lua_forwchar(lua_State *ls)
{
    forwchar(0, 1);
    return 0;
}

static int
lua_backchar(lua_State *ls)
{
    backchar(0, 1);
    return 0;
}

static int
lua_forwline(lua_State *ls)
{
    forwline(0, 1);
    return 0;
}

static int
lua_backline(lua_State *ls)
{
    backline(0, 1);
    return 0;
}

/*
 * The 'mg' module definition for Lua.
 */
static const struct luaL_Reg mglib[] = {
    {"defun", l_defun},
    {"bind_key", l_bind_key},
    {"new_keymap", l_new_keymap},
    {"set_mode", l_set_mode},
    {"forwchar", lua_forwchar},
    {"backchar", lua_backchar},
    {"forwline", lua_forwline},
    {"backline", lua_backline},
    {NULL, NULL} /* Sentinel */
};

/*
 * Load and execute a Lua file.
 */
void
lua_eval_file(const char *filename)
{
    if (L == NULL) {
        dobeep_msg("Lua not initialized");
        return;
    }
    if (luaL_dofile(L, filename)) {
        dobeep();
        ewprintf("Lua error: %s", lua_tostring(L, -1));
        lua_pop(L, 1); /* pop error message */
    }
}

/*
 * mg command to evaluate a Lua file.
 */
int
eval_lua_file_cmd(int f, int n)
{
    char filename[NFILEN];

    if (eread("Eval Lua file: ", filename, NFILEN, EFNEW | EFCR) == NULL)
        return (ABORT);
    if (filename[0] == '\0')
        return (FALSE);

    lua_eval_file(filename);

    return (TRUE);
}

/*
 * Initialize the Lua interpreter and create the 'mg' module.
 */
void
lua_init(void)
{
    int i;

    L = luaL_newstate();
    if (L == NULL) {
        dobeep_msg("Failed to initialize Lua");
        return;
    }
    luaL_openlibs(L);

    for (i = 0; i < MAX_LUA_CMDS; i++) {
        lua_fn_refs[i] = LUA_REFNIL;
    }

    /* Create the 'mg' module */
    luaL_newlib(L, mglib);
    lua_setglobal(L, "mg");

    /* Register the eval-lua-file command */
    funmap_add(eval_lua_file_cmd, "eval-lua-file", 1);
}

#endif /* HAVE_LUA */
