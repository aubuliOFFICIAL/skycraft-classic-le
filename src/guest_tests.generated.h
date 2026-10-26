#pragma once
#include <array>
namespace classic::guesttest {
struct DropTorch {
static constexpr unsigned action=16;
static constexpr unsigned select=35;
static constexpr unsigned key=20;
static constexpr unsigned kind=1;
static constexpr unsigned extraKey=0;
static constexpr unsigned holdMs=200;
};
struct View {
static constexpr unsigned action=17;
static constexpr unsigned select=0;
static constexpr unsigned key=62;
static constexpr unsigned kind=1;
static constexpr unsigned extraKey=0;
static constexpr unsigned holdMs=200;
};
struct PlacePlanks {
static constexpr unsigned action=19;
static constexpr unsigned select=34;
static constexpr unsigned key=3;
static constexpr unsigned kind=2;
static constexpr unsigned extraKey=0;
static constexpr unsigned holdMs=200;
};
struct MinePlank {
static constexpr unsigned action=21;
static constexpr unsigned select=31;
static constexpr unsigned key=1;
static constexpr unsigned kind=2;
static constexpr unsigned extraKey=0;
static constexpr unsigned holdMs=4000;
};
struct JumpOntoBlock {
static constexpr unsigned action=26;
static constexpr unsigned select=0;
static constexpr unsigned key=44;
static constexpr unsigned kind=1;
static constexpr unsigned extraKey=26;
static constexpr unsigned holdMs=300;
};
struct Sneak {
static constexpr unsigned action=27;
static constexpr unsigned select=0;
static constexpr unsigned key=225;
static constexpr unsigned kind=1;
static constexpr unsigned extraKey=0;
static constexpr unsigned holdMs=4000;
};
struct Test {unsigned action,select,key,kind,extraKey,holdMs;};
inline constexpr std::array<Test,6> tests={{
{DropTorch::action,DropTorch::select,DropTorch::key,DropTorch::kind,DropTorch::extraKey,DropTorch::holdMs},
{View::action,View::select,View::key,View::kind,View::extraKey,View::holdMs},
{PlacePlanks::action,PlacePlanks::select,PlacePlanks::key,PlacePlanks::kind,PlacePlanks::extraKey,PlacePlanks::holdMs},
{MinePlank::action,MinePlank::select,MinePlank::key,MinePlank::kind,MinePlank::extraKey,MinePlank::holdMs},
{JumpOntoBlock::action,JumpOntoBlock::select,JumpOntoBlock::key,JumpOntoBlock::kind,JumpOntoBlock::extraKey,JumpOntoBlock::holdMs},
{Sneak::action,Sneak::select,Sneak::key,Sneak::kind,Sneak::extraKey,Sneak::holdMs},
}};
}
