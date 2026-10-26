// Generated from sheets/bootstrap.json
#pragma once
namespace classic::spec {
struct DiagnosticBootstrap {
static constexpr const char* id = "DiagnosticBootstrap";
static constexpr const char* hostExe = "TESV.exe";
static constexpr unsigned hostMachine = 332;
static constexpr const char* proxyName = "dinput8.dll";
static constexpr const char* exportName = "DirectInput8Create";
static constexpr unsigned expectedPointerBits = 32;
static constexpr const char* logDirectory = "SkyCraftClassic";
static constexpr const char* logFile = "bootstrap.log";
static constexpr bool gameplayImplemented = false;
static constexpr const char* source = "Installed TESV.exe PE imports and machine; diagnostic-only scope";
};
}