#pragma once

#include <stdbool.h>
#include <time.h>

typedef struct {
    char start_time[16];    // "HH:MM:SS" o "HH:MM"
    char end_time[16];
    int interval_value;
    char interval_unit[16]; // "seconds" | "minutes"
    char enabled_days[32];  // días habilitados, convención Python: lunes=0..domingo=6
} schedule_config_t;

/** Réplica de agent/scheduler.py::is_within_schedule. */
bool scheduler_is_within_schedule(const schedule_config_t *schedule, time_t now);

/** Réplica de agent/scheduler.py::interval_seconds. */
int scheduler_interval_seconds(const schedule_config_t *schedule);
