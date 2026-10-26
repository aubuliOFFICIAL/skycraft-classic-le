// Generated from sheets/input_bindings.json
#pragma once
#include <array>
namespace classic::input {
struct Forward { static constexpr unsigned vk=87,code=26,kind=1; };
struct Left { static constexpr unsigned vk=65,code=4,kind=1; };
struct Back { static constexpr unsigned vk=83,code=22,kind=1; };
struct Right { static constexpr unsigned vk=68,code=7,kind=1; };
struct Inventory { static constexpr unsigned vk=69,code=8,kind=1; };
struct Drop { static constexpr unsigned vk=81,code=20,kind=1; };
struct SwapHands { static constexpr unsigned vk=70,code=9,kind=1; };
struct Jump { static constexpr unsigned vk=32,code=44,kind=1; };
struct Sneak { static constexpr unsigned vk=160,code=225,kind=1; };
struct Sprint { static constexpr unsigned vk=162,code=224,kind=1; };
struct View { static constexpr unsigned vk=116,code=62,kind=1; };
struct Options { static constexpr unsigned vk=79,code=18,kind=1; };
struct Slot1 { static constexpr unsigned vk=49,code=30,kind=1; };
struct Slot2 { static constexpr unsigned vk=50,code=31,kind=1; };
struct Slot3 { static constexpr unsigned vk=51,code=32,kind=1; };
struct Slot4 { static constexpr unsigned vk=52,code=33,kind=1; };
struct Slot5 { static constexpr unsigned vk=53,code=34,kind=1; };
struct Slot6 { static constexpr unsigned vk=54,code=35,kind=1; };
struct Slot7 { static constexpr unsigned vk=55,code=36,kind=1; };
struct Slot8 { static constexpr unsigned vk=56,code=37,kind=1; };
struct Slot9 { static constexpr unsigned vk=57,code=38,kind=1; };
struct Attack { static constexpr unsigned vk=1,code=1,kind=2; };
struct Use { static constexpr unsigned vk=2,code=3,kind=2; };
struct Middle { static constexpr unsigned vk=4,code=2,kind=2; };
struct GuestOptions { static constexpr unsigned vk=79,code=41,kind=1; };
struct NativeActivate { static constexpr unsigned vk=71,code=10,kind=1; };
struct Binding { unsigned vk,code,kind; };
inline constexpr std::array<Binding,26> bindings = {{
{Forward::vk,Forward::code,Forward::kind},
{Left::vk,Left::code,Left::kind},
{Back::vk,Back::code,Back::kind},
{Right::vk,Right::code,Right::kind},
{Inventory::vk,Inventory::code,Inventory::kind},
{Drop::vk,Drop::code,Drop::kind},
{SwapHands::vk,SwapHands::code,SwapHands::kind},
{Jump::vk,Jump::code,Jump::kind},
{Sneak::vk,Sneak::code,Sneak::kind},
{Sprint::vk,Sprint::code,Sprint::kind},
{View::vk,View::code,View::kind},
{Options::vk,Options::code,Options::kind},
{Slot1::vk,Slot1::code,Slot1::kind},
{Slot2::vk,Slot2::code,Slot2::kind},
{Slot3::vk,Slot3::code,Slot3::kind},
{Slot4::vk,Slot4::code,Slot4::kind},
{Slot5::vk,Slot5::code,Slot5::kind},
{Slot6::vk,Slot6::code,Slot6::kind},
{Slot7::vk,Slot7::code,Slot7::kind},
{Slot8::vk,Slot8::code,Slot8::kind},
{Slot9::vk,Slot9::code,Slot9::kind},
{Attack::vk,Attack::code,Attack::kind},
{Use::vk,Use::code,Use::kind},
{Middle::vk,Middle::code,Middle::kind},
{GuestOptions::vk,GuestOptions::code,GuestOptions::kind},
{NativeActivate::vk,NativeActivate::code,NativeActivate::kind},
}};
}