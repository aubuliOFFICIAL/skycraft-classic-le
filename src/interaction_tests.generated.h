#pragma once
#include <array>
namespace classic::interactiontest {
struct TreasureChest {static constexpr unsigned action=18,reference=225769;static constexpr float pitch=0.0f,height=35.0f;};
struct Floor {static constexpr unsigned action=20,reference=0;static constexpr float pitch=0.85f,height=0.0f;};
struct InnContainer {static constexpr unsigned action=22,reference=342500;static constexpr float pitch=0.0f,height=35.0f;};
struct InnDialogue {static constexpr unsigned action=23,reference=78982;static constexpr float pitch=0.0f,height=85.0f;};
struct InnOwnedFood {static constexpr unsigned action=28,reference=393459;static constexpr float pitch=0.0f,height=2.0f;};
struct Test {unsigned action,reference;float pitch,height;};
inline constexpr std::array<Test,5> tests={{
{TreasureChest::action,TreasureChest::reference,TreasureChest::pitch,TreasureChest::height},
{Floor::action,Floor::reference,Floor::pitch,Floor::height},
{InnContainer::action,InnContainer::reference,InnContainer::pitch,InnContainer::height},
{InnDialogue::action,InnDialogue::reference,InnDialogue::pitch,InnDialogue::height},
{InnOwnedFood::action,InnOwnedFood::reference,InnOwnedFood::pitch,InnOwnedFood::height},
}};
}
