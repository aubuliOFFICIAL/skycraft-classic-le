// Generated from sheets/systems.json. Do not edit.
#pragma once
namespace classic::spec {
struct Bootstrap {
static constexpr const char* id = "bootstrap";
static constexpr const char* owner = "host";
static constexpr const char* route = "x86 DirectInput proxy";
static constexpr const char* source = "TESV.exe import table and repeated live dungeon launches";
static constexpr const char* requires = "";
static constexpr const char* acceptance = "Original Skyrim loads proxy; system DirectInput remains usable";
static constexpr const char* verification = "verified";
};
struct Transport {
static constexpr const char* id = "transport";
static constexpr const char* owner = "both";
static constexpr const char* route = "fixed-width shared memory";
static constexpr const char* source = "protocol/skycraft_protocol.h; live host/JVM state and HUD exchange; build/protocol-evidence.json";
static constexpr const char* requires = "bootstrap";
static constexpr const char* acceptance = "x86 host and x64 JVM exchange consistent heartbeat and state";
static constexpr const char* verification = "verified";
};
struct Rendering {
static constexpr const char* id = "rendering";
static constexpr const char* owner = "host";
static constexpr const char* route = "Direct3D 9 native world pass and HUD";
static constexpr const char* source = "skse/src/WorldRender.cpp; skse/src/Overlay.cpp";
static constexpr const char* requires = "transport";
static constexpr const char* acceptance = "Minecraft sword, bow, arrows and HUD appear in Skyrim with correct depth";
static constexpr const char* verification = "pending";
};
struct Input {
static constexpr const char* id = "input";
static constexpr const char* owner = "host";
static constexpr const char* route = "respect host bindings; forward guest actions";
static constexpr const char* source = "skse/src/Input.cpp";
static constexpr const char* requires = "transport";
static constexpr const char* acceptance = "Sword attack, charged bow release, inventory and native menus respond";
static constexpr const char* verification = "pending";
};
struct Movement {
static constexpr const char* id = "movement";
static constexpr const char* owner = "guest";
static constexpr const char* route = "real Minecraft movement with host pose adapter";
static constexpr const char* source = "skse/src/Game.cpp";
static constexpr const char* requires = "input,collision";
static constexpr const char* acceptance = "Walk, jump and transition through dungeon door without desync";
static constexpr const char* verification = "pending";
};
struct Collision {
static constexpr const char* id = "collision";
static constexpr const char* owner = "host";
static constexpr const char* route = "LE Havok collision decoded from the installed game's ESM/BSA/NIF files in RAM";
static constexpr const char* source = "tools/collision_bridge.py; tools/collision_assets.py; pinned protocol";
static constexpr const char* requires = "transport";
static constexpr const char* acceptance = "Player and arrows respect dungeon walls, floor and stairs";
static constexpr const char* verification = "pending";
};
struct Combat {
static constexpr const char* id = "combat";
static constexpr const char* owner = "both";
static constexpr const char* route = "Minecraft hits and projectiles; host actor proxy table";
static constexpr const char* source = "skse/src/Combat.cpp";
static constexpr const char* requires = "transport,collision,movement";
static constexpr const char* acceptance = "Sword and arrow damage a Skyrim enemy; enemy damages player; death works";
static constexpr const char* verification = "pending";
};
struct Dungeon {
static constexpr const char* id = "dungeon";
static constexpr const char* owner = "host";
static constexpr const char* route = "isolated test save and raid objective";
static constexpr const char* source = "creator design";
static constexpr const char* requires = "combat,rendering";
static constexpr const char* acceptance = "Solo spawn outside dungeon; sword, bow, arrows available; loot goal reachable";
static constexpr const char* verification = "pending";
};
struct Lifecycle {
static constexpr const char* id = "lifecycle";
static constexpr const char* owner = "both";
static constexpr const char* route = "owned guest process only; version and heartbeat checks";
static constexpr const char* source = "docs/CONTRACT.md";
static constexpr const char* requires = "bootstrap,transport";
static constexpr const char* acceptance = "One-click start and clean stop leave other games and saves alone";
static constexpr const char* verification = "pending";
};
}
