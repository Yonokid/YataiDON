#include "background.h"

Background::Background(PlayerNum player_num, float bpm, const std::string& scene_preset) {
    sol::state& lua = script_manager.lua;

    std::string script_path = script_manager.get_lua_script_path("background");
    auto result = lua.script_file(script_path);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error loading background.lua: {}", err.what());
    }

    sol::table background_class = lua["Background"];
    sol::protected_function new_func = background_class["new"];

    auto call_result = new_func(static_cast<int>(player_num), bpm, scene_preset);
    if (!call_result.valid()) {
        sol::error err = call_result;
        spdlog::error("Error calling Background.new: {}", err.what());
    } else {
        lua_object = call_result;
    }
}

Background::~Background() {
    sol::optional<sol::protected_function> destroy = lua_object["destroy"];
    if (destroy) {
        auto result = destroy.value()(lua_object);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Error calling destroy: {}", err.what());
        }
    }
}

void Background::update(double current_ms) {
    sol::protected_function update_func = lua_object["update"];
    auto result = update_func(lua_object, current_ms);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling update: {}", err.what());
    }
}

void Background::handle_good(PlayerNum player_num) {
    sol::protected_function func = lua_object["handle_good"];
    auto result = func(lua_object, static_cast<int>(player_num));
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling handle_good: {}", err.what());
    }
}

void Background::handle_ok(PlayerNum player_num) {
    sol::protected_function func = lua_object["handle_ok"];
    auto result = func(lua_object, static_cast<int>(player_num));
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling handle_ok: {}", err.what());
    }
}

void Background::handle_bad(PlayerNum player_num) {
    sol::protected_function func = lua_object["handle_bad"];
    auto result = func(lua_object, static_cast<int>(player_num));
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling handle_bad: {}", err.what());
    }
}

void Background::handle_drumroll(PlayerNum player_num) {
    sol::protected_function func = lua_object["handle_drumroll"];
    auto result = func(lua_object, static_cast<int>(player_num));
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling handle_drumroll: {}", err.what());
    }
}

void Background::handle_balloon(PlayerNum player_num) {
    sol::protected_function func = lua_object["handle_balloon"];
    auto result = func(lua_object, static_cast<int>(player_num));
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling handle_balloon: {}", err.what());
    }
}

void Background::handle_gauge(PlayerNum player_num, float progress, bool is_clear, bool is_rainbow) {
    sol::protected_function func = lua_object["handle_gauge"];
    auto result = func(lua_object, static_cast<int>(player_num), progress, is_clear, is_rainbow);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling handle_gauge: {}", err.what());
    }
}

void Background::draw_back() {
    sol::protected_function func = lua_object["draw_back"];
    auto result = func(lua_object);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling draw_back: {}", err.what());
    }
}

void Background::draw_fore() {
    sol::protected_function func = lua_object["draw_fore"];
    auto result = func(lua_object);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Error calling draw_fore: {}", err.what());
    }
}
