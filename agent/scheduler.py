from datetime import datetime, time
from typing import Any


def parse_time(value: str) -> time:
    parts = value.split(":")
    return time(int(parts[0]), int(parts[1]))


def is_within_schedule(config: dict[str, Any], now: datetime | None = None) -> bool:
    now = now or datetime.now()
    schedule = config.get("schedule", {})
    enabled_days = schedule.get("enabled_days", "0,1,2,3,4,5,6")
    allowed = {int(d.strip()) for d in enabled_days.split(",") if d.strip().isdigit()}
    if now.weekday() not in allowed:
        return False

    start = parse_time(str(schedule.get("start_time", "07:00:00"))[:8])
    end = parse_time(str(schedule.get("end_time", "18:00:00"))[:8])
    current = now.time()
    if start <= end:
        return start <= current <= end
    return current >= start or current <= end


def interval_seconds(config: dict[str, Any]) -> int:
    schedule = config.get("schedule", {})
    value = int(schedule.get("interval_value", 5))
    unit = schedule.get("interval_unit", "minutes")
    if unit == "seconds":
        return max(1, value)
    return max(1, value * 60)
