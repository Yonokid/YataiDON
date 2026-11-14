#include "story_script.h"

void StoryScript::bind() {
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_played = lua_object["played"];
}

void StoryScript::update(double current_ms) {
    call(fn_update, "StoryScript:update", current_ms);
}

void StoryScript::input(Action action) {
    call(fn_input, "StoryScript:input", static_cast<int>(action));
}

void StoryScript::draw() {
    call(fn_draw, "StoryScript:draw");
}

bool StoryScript::is_done() {
    return call_r<bool>(fn_is_done, "StoryScript:is_done").value_or(true);
}

std::vector<std::string> StoryScript::played() {
    if (!fn_played.valid()) {
        return {};
    }
    return call_r<std::vector<std::string>>(fn_played, "StoryScript:played").value_or(std::vector<std::string>{});
}
