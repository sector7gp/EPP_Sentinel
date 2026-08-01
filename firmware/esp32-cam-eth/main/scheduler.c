#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int parse_time_seconds(const char *value)
{
    int h = 0;
    int m = 0;
    int s = 0;
    sscanf(value, "%d:%d:%d", &h, &m, &s);
    return h * 3600 + m * 60 + s;
}

bool scheduler_is_within_schedule(const schedule_config_t *schedule, time_t now)
{
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    // tm_wday de C usa domingo=0..sábado=6; el backend (y agent/scheduler.py,
    // basado en datetime.weekday() de Python) usa lunes=0..domingo=6.
    int python_weekday = (tm_now.tm_wday + 6) % 7;

    bool day_enabled = false;
    char days_copy[32];
    strlcpy(days_copy, schedule->enabled_days[0] ? schedule->enabled_days : "0,1,2,3,4,5,6",
             sizeof(days_copy));
    char *saveptr = NULL;
    for (char *tok = strtok_r(days_copy, ",", &saveptr); tok; tok = strtok_r(NULL, ",", &saveptr)) {
        if (atoi(tok) == python_weekday) {
            day_enabled = true;
            break;
        }
    }
    if (!day_enabled) {
        return false;
    }

    int start = parse_time_seconds(schedule->start_time[0] ? schedule->start_time : "07:00:00");
    int end = parse_time_seconds(schedule->end_time[0] ? schedule->end_time : "18:00:00");
    int current = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;

    if (start <= end) {
        return current >= start && current <= end;
    }
    return current >= start || current <= end;
}

int scheduler_interval_seconds(const schedule_config_t *schedule)
{
    int value = schedule->interval_value > 0 ? schedule->interval_value : 5;
    if (strcmp(schedule->interval_unit, "seconds") == 0) {
        return value < 1 ? 1 : value;
    }
    int seconds = value * 60;
    return seconds < 1 ? 1 : seconds;
}
