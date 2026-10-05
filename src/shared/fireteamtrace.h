#ifndef __FIRETEAM_TRACE_H__
#define __FIRETEAM_TRACE_H__
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

inline void FTTraceReset() { DeleteFileA("fireteam-trace.log"); }

inline void FTTraceLog(const char* module, const char* format, ...)
{
    char message[2048];
    message[0] = 0;

    va_list args;
    va_start(args, format);
    _vsnprintf(message, sizeof(message) - 1, format, args);
    va_end(args);
    message[sizeof(message) - 1] = 0;

    FILE* fp = fopen("fireteam-trace.log", "a");
    if(fp)
    {
        fprintf(fp, "%010lu [%s] %s\n",
            (unsigned long)GetTickCount(),
            module ? module : "?",
            message);
        fflush(fp);
        fclose(fp);
    }
}
#endif