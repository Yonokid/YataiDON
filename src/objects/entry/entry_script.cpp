#include "entry_script.h"
#include "../../libs/script.h"
#include <spdlog/spdlog.h>

EntryScript::EntryScript() {
    sol::state& lua = *script_manager.lua;

    if (!lua["Entry"].valid()) {
        std::string script_path = script_manager.get_lua_script_path("entry");
        auto result = lua.script_file(script_path);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Error loading entry.lua: {}", err.what());
            return;
        }
    }

    sol::table entry_class = lua["Entry"];
    sol::protected_function new_func = entry_class["new"];
    auto call_result = new_func();
    if (!call_result.valid()) {
        sol::error err = call_result;
        spdlog::error("Error calling Entry.new: {}", err.what());
        return;
    }

    lua_object              = call_result;
    fn_update               = lua_object["update"];
    fn_start_side_select    = lua_object["start_side_select"];
    fn_restart_side_select  = lua_object["restart_side_select"];
    fn_get_side_select_fade = lua_object["get_side_select_fade"];
    fn_draw_background      = lua_object["draw_background"];
    fn_draw_side_select     = lua_object["draw_side_select"];
    fn_draw_footer          = lua_object["draw_footer"];
    fn_draw_player_entry    = lua_object["draw_player_entry"];
}

void EntryScript::update(double current_ms) {
    auto result = fn_update(lua_object, current_ms);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:update: {}", err.what());
    }
}

void EntryScript::start_side_select() {
    auto result = fn_start_side_select(lua_object);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:start_side_select: {}", err.what());
    }
}

void EntryScript::restart_side_select() {
    auto result = fn_restart_side_select(lua_object);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:restart_side_select: {}", err.what());
    }
}

float EntryScript::get_side_select_fade() {
    auto result = fn_get_side_select_fade(lua_object);
    if (!result.valid()) return 1.0f;
    return result.get<float>();
}

void EntryScript::draw_background() {
    auto result = fn_draw_background(lua_object);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:draw_background: {}", err.what());
    }
}

void EntryScript::draw_side_select(int side) {
    auto result = fn_draw_side_select(lua_object, side);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:draw_side_select: {}", err.what());
    }
}

void EntryScript::draw_footer(bool p1_joined, bool p2_joined) {
    auto result = fn_draw_footer(lua_object, p1_joined, p2_joined);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:draw_footer: {}", err.what());
    }
}

void EntryScript::draw_player_entry() {
    auto result = fn_draw_player_entry(lua_object);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling Entry:draw_player_entry: {}", err.what());
    }
}
