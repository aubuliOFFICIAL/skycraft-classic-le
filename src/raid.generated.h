#pragma once
#include <array>
namespace classic::raid::spec {
struct EntryChamber {
static constexpr unsigned cell=225758;
static constexpr unsigned chest=225769;
static constexpr unsigned reachUnits=175;
static constexpr const char* title="Bleak Falls Raid";
static constexpr const char* briefing="Defeat both bandits, then reach the treasure chest.";
static constexpr const char* completeText="Chamber cleared - treasure reached";
static constexpr std::array<unsigned,2> enemies={237540,237538};
};
struct Panel {
static constexpr unsigned x=28u;
static constexpr unsigned y=100u;
static constexpr unsigned width=768u;
static constexpr unsigned height=116u;
static constexpr unsigned fontSize=24u;
static constexpr unsigned padding=12u;
static constexpr unsigned background=2281701376u;
static constexpr unsigned textColor=4294962113u;
static constexpr const char* controls="E: inventory  |  G: Skyrim doors and loot";
};
}
