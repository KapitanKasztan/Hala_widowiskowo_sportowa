#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define REPORTS_DIR "reports"

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
} LogLevel;

typedef struct {
    FILE *file;
    char filename[256];
    int process_id;
    const char *process_name;
} Logger;

static inline const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

static inline void create_reports_dir() {
    struct stat st = {0};
    if (stat(REPORTS_DIR, &st) == -1) {
        mkdir(REPORTS_DIR, 0755);
    }
}

static inline Logger* reporter_init(const char *process_name, int process_id) {
    create_reports_dir();

    Logger *r = (Logger*)malloc(sizeof(Logger));
    if (!r) return NULL;

    r->process_id = process_id;
    r->process_name = process_name;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if (process_id >= 0) {
        snprintf(r->filename, sizeof(r->filename),
                "%s/%s_%d_%04d%02d%02d_%02d%02d%02d.log",
                REPORTS_DIR, process_name, process_id,
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        snprintf(r->filename, sizeof(r->filename),
                "%s/%s_%04d%02d%02d_%02d%02d%02d.log",
                REPORTS_DIR, process_name,
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
    }

    r->file = fopen(r->filename, "w");
    if (!r->file) {
        free(r);
        return NULL;
    }

    return r;
}

static inline void reporter_log(Logger *r, LogLevel level, const char *format, ...) {
    if (!r || !r->file) return;

    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // zapis
    if (r->process_id >= 0) {
        fprintf(r->file, "[%s] [%s-%d] [%s] %s\n",
                timestr, r->process_name, r->process_id,
                level_to_string(level), message);
    } else {
        fprintf(r->file, "[%s] [%s] [%s] %s\n",
                timestr, r->process_name,
                level_to_string(level), message);
    }

    fflush(r->file);
}

static inline void reporter_close(Logger *r) {
    if (r) {
        if (r->file) {
            fflush(r->file);
            fclose(r->file);
        }
        free(r);
    }
}

static inline void reporter_debug(Logger *r, const char *format, ...) {
    if (!r) return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    reporter_log(r, LOG_DEBUG, "%s", message);
}

static inline void reporter_info(Logger *r, const char *format, ...) {
    if (!r) return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    reporter_log(r, LOG_INFO, "%s", message);
}

static inline void reporter_warning(Logger *r, const char *format, ...) {
    if (!r) return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    reporter_log(r, LOG_WARNING, "%s", message);
}

static inline void reporter_error(Logger *r, const char *format, ...) {
    if (!r) return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    reporter_log(r, LOG_ERROR, "%s", message);
}

static inline void reporter_critical(Logger *r, const char *format, ...) {
    if (!r) return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    reporter_log(r, LOG_CRITICAL, "%s", message);
}