#pragma once
#include <stdio.h>
#include <stdarg.h>
struct Logger {
    void SetTag(const char*) {}
    void Info(const char* f, ...) { if(!getenv("PCM_VERBOSE")) return; va_list a; va_start(a, f); printf("   [log] "); vprintf(f, a); printf("\n"); va_end(a); }
    void Error(const char* f, ...) { va_list a; va_start(a, f); printf("   [ERR] "); vprintf(f, a); printf("\n"); va_end(a); }
};
extern Logger* logger;
