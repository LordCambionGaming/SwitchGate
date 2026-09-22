#include "JsonUtil.h"
#include "raymath.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>

using json = nlohmann::json;

std::string ToLowerStr(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

Vector3 ParseVec3(const json& j, Vector3 fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    try {
        return Vector3{ j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
    } catch (...) {
        return fallback;
    }
}

json Vec3ToJson(Vector3 v) {
    return json::array({ v.x, v.y, v.z });
}

Color ParseColor(const json& j, Color fallback) {
    if (j.is_string()) {
        static const std::unordered_map<std::string, Color> table = {
            { "red", RED }, { "rosso", RED },
            { "blue", BLUE }, { "blu", BLUE },
            { "green", GREEN }, { "verde", GREEN },
            { "yellow", YELLOW }, { "giallo", YELLOW },
            { "orange", ORANGE }, { "arancione", ORANGE },
            { "purple", PURPLE }, { "viola", PURPLE }, { "violet", VIOLET },
            { "pink", PINK }, { "rosa", PINK },
            { "gold", GOLD }, { "oro", GOLD },
            { "lime", LIME },
            { "skyblue", SKYBLUE }, { "azzurro", SKYBLUE },
            { "white", WHITE }, { "bianco", WHITE },
            { "black", BLACK }, { "nero", BLACK },
            { "gray", GRAY }, { "grey", GRAY }, { "grigio", GRAY },
            { "lightgray", LIGHTGRAY }, { "grigiochiaro", LIGHTGRAY },
            { "darkgray", DARKGRAY }, { "grigioscuro", DARKGRAY },
            { "brown", BROWN }, { "marrone", BROWN },
            { "darkbrown", DARKBROWN },
            { "maroon", MAROON }, { "bordeaux", MAROON },
            { "beige", BEIGE },
        };
        auto it = table.find(ToLowerStr(j.get<std::string>()));
        if (it != table.end()) return it->second;
        return fallback;
    }
    if (j.is_array() && j.size() >= 3) {
        // Consente anche [r,g,b] o [r,g,b,a] con valori 0-255
        unsigned char r = (unsigned char)Clamp(j[0].get<float>(), 0, 255);
        unsigned char g = (unsigned char)Clamp(j[1].get<float>(), 0, 255);
        unsigned char b = (unsigned char)Clamp(j[2].get<float>(), 0, 255);
        unsigned char a = j.size() >= 4 ? (unsigned char)Clamp(j[3].get<float>(), 0, 255) : 255;
        return Color{ r, g, b, a };
    }
    return fallback;
}

static bool ColorsEqual(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

json ColorToJson(Color c) {
    struct Named { const char* name; Color color; };
    static const Named table[] = {
        { "red", RED }, { "blue", BLUE }, { "green", GREEN }, { "yellow", YELLOW },
        { "orange", ORANGE }, { "purple", PURPLE }, { "violet", VIOLET }, { "pink", PINK },
        { "gold", GOLD }, { "lime", LIME }, { "skyblue", SKYBLUE }, { "white", WHITE },
        { "black", BLACK }, { "gray", GRAY }, { "lightgray", LIGHTGRAY }, { "darkgray", DARKGRAY },
        { "brown", BROWN }, { "darkbrown", DARKBROWN }, { "maroon", MAROON }, { "beige", BEIGE },
    };
    for (const auto& n : table) {
        if (ColorsEqual(c, n.color)) return json(n.name);
    }
    if (c.a == 255) return json::array({ c.r, c.g, c.b });
    return json::array({ c.r, c.g, c.b, c.a });
}
