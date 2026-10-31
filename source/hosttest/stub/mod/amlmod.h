#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string>
#define AML64 1
#define MYMOD(...)
#define NEEDGAME(...)
#define BYBIT(a, b) (b)
#define SET_TO(var, ptr) *(void**)&(var) = (void*)(ptr)
#define DECL_HOOKv(_name, ...) static void (*_name)(__VA_ARGS__); static void HookOf_##_name(__VA_ARGS__)
#define DECL_HOOKb(_name, ...) static bool (*_name)(__VA_ARGS__); static bool HookOf_##_name(__VA_ARGS__)
#define DECL_HOOKi(_name, ...) static int  (*_name)(__VA_ARGS__); static int  HookOf_##_name(__VA_ARGS__)
class IAML {
public:
    uintptr_t GetSym(void*, const char*) { return 0; }
    uintptr_t GetLib(const char*) { return 0; }
    void* GetLibHandle(const char*) { return 0; }
    std::string GetAndroidDataPath() { return ""; }
    bool HookPLT(void*, void*, void**) { return true; }
    bool Hook(void*, void*, void**) { return true; }
};
static IAML g_amlStub; static IAML* aml = &g_amlStub;
