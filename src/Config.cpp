#include "Config.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

std::string KeyToName(int key) {
    if (key >= KEY_A && key <= KEY_Z) return std::string(1, (char)('A' + (key - KEY_A)));
    if (key >= KEY_ZERO && key <= KEY_NINE) return std::string(1, (char)('0' + (key - KEY_ZERO)));
    if (key >= KEY_F1 && key <= KEY_F12) return "F" + std::to_string(key - KEY_F1 + 1);

    switch (key) {
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_ESCAPE: return "ESCAPE";
        case KEY_TAB: return "TAB";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT_SHIFT: return "LEFT_SHIFT";
        case KEY_RIGHT_SHIFT: return "RIGHT_SHIFT";
        case KEY_LEFT_CONTROL: return "LEFT_CONTROL";
        case KEY_RIGHT_CONTROL: return "RIGHT_CONTROL";
        case KEY_LEFT_ALT: return "LEFT_ALT";
        case KEY_RIGHT_ALT: return "RIGHT_ALT";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_SEMICOLON: return ";";
        case KEY_COMMA: return ",";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_MINUS: return "-";
        case KEY_EQUAL: return "=";
        default: return "KEY_" + std::to_string(key);
    }
}

int NameToKey(const std::string& name) {
    if (name.size() == 1) {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') return KEY_A + (c - 'A');
        if (c >= '0' && c <= '9') return KEY_ZERO + (c - '0');
        switch (c) {
            case '[': return KEY_LEFT_BRACKET;
            case ']': return KEY_RIGHT_BRACKET;
            case ';': return KEY_SEMICOLON;
            case ',': return KEY_COMMA;
            case '.': return KEY_PERIOD;
            case '/': return KEY_SLASH;
            case '-': return KEY_MINUS;
            case '=': return KEY_EQUAL;
        }
    }
    if (name.size() >= 2 && name[0] == 'F') {
        bool allDigits = true;
        for (size_t i = 1; i < name.size(); i++) if (!isdigit((unsigned char)name[i])) allDigits = false;
        if (allDigits) {
            int n = std::stoi(name.substr(1));
            if (n >= 1 && n <= 12) return KEY_F1 + (n - 1);
        }
    }
    if (name == "SPACE") return KEY_SPACE;
    if (name == "ENTER") return KEY_ENTER;
    if (name == "ESCAPE") return KEY_ESCAPE;
    if (name == "TAB") return KEY_TAB;
    if (name == "BACKSPACE") return KEY_BACKSPACE;
    if (name == "LEFT") return KEY_LEFT;
    if (name == "RIGHT") return KEY_RIGHT;
    if (name == "UP") return KEY_UP;
    if (name == "DOWN") return KEY_DOWN;
    if (name == "LEFT_SHIFT") return KEY_LEFT_SHIFT;
    if (name == "RIGHT_SHIFT") return KEY_RIGHT_SHIFT;
    if (name == "LEFT_CONTROL") return KEY_LEFT_CONTROL;
    if (name == "RIGHT_CONTROL") return KEY_RIGHT_CONTROL;
    if (name == "LEFT_ALT") return KEY_LEFT_ALT;
    if (name == "RIGHT_ALT") return KEY_RIGHT_ALT;
    if (name.rfind("KEY_", 0) == 0) {
        try { return std::stoi(name.substr(4)); } catch (...) { return -1; }
    }
    return -1;
}

bool LoadKeyBindings(const std::string& path, KeyBindings& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    json j;
    try {
        file >> j;
    } catch (...) {
        return false;
    }

    KeyBindings kb = out;
    auto readKey = [&](const char* field, int& target) {
        if (j.contains(field) && j[field].is_string()) {
            int k = NameToKey(j[field].get<std::string>());
            if (k >= 0) target = k;
        }
    };
    readKey("move_up", kb.moveUp);
    readKey("move_down", kb.moveDown);
    readKey("move_left", kb.moveLeft);
    readKey("move_right", kb.moveRight);
    readKey("jump", kb.jump);
    readKey("reset_level", kb.resetLevel);
    readKey("back", kb.back);
    readKey("toggle_music", kb.toggleMusic);
    readKey("interact", kb.interact);
    readKey("rotate_left", kb.rotateLeft);
    readKey("rotate_right", kb.rotateRight);
    if (j.contains("mouse_sensitivity") && j["mouse_sensitivity"].is_number()) {
        kb.mouseSensitivity = j["mouse_sensitivity"].get<float>();
    }

    out = kb;
    return true;
}

bool SaveKeyBindings(const std::string& path, const KeyBindings& kb) {
    json j;
    j["move_up"] = KeyToName(kb.moveUp);
    j["move_down"] = KeyToName(kb.moveDown);
    j["move_left"] = KeyToName(kb.moveLeft);
    j["move_right"] = KeyToName(kb.moveRight);
    j["jump"] = KeyToName(kb.jump);
    j["reset_level"] = KeyToName(kb.resetLevel);
    j["back"] = KeyToName(kb.back);
    j["toggle_music"] = KeyToName(kb.toggleMusic);
    j["interact"] = KeyToName(kb.interact);
    j["rotate_left"] = KeyToName(kb.rotateLeft);
    j["rotate_right"] = KeyToName(kb.rotateRight);
    j["mouse_sensitivity"] = kb.mouseSensitivity;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}