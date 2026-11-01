/* Common types for the platform-independent logic (no ESP-IDF, no FreeRTOS). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Monotonic time in milliseconds, passed in by the caller (injected clock). */
typedef int64_t cu_ms_t;

/* Signal quality (SPEC 3.2 signals). */
typedef enum {
    CU_Q_OK = 0,
    CU_Q_STALE,
    CU_Q_OPEN,
    CU_Q_SHORT,
    CU_Q_IMPLAUSIBLE,
    CU_Q_UNCALIBRATED,
} cu_quality_t;

/* A profile value that may still be "TODO_MEASURE". */
typedef struct {
    float v;
    bool cal;
} cu_cal_t;

#define CU_N_RELAYS 8
#define CU_N_DI 16
#define CU_N_AI 8
#define CU_N_LADDER 4

#define CU_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

const char *cu_quality_name(cu_quality_t q);
