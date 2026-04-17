# Kernel Time Manager

## Files
- `managers/time/time_manager.c/.h` — Time tracking
- `managers/timer/pit.c/.h` — PIT hardware driver (50Hz)
- `src/drivers/rtc/rtc.c/.h` — Real-Time Clock (date/time)

## API
- `kernel_time_get_datetime(dt)` — current date/time from RTC
- `kernel_time_get_unix()` — Unix timestamp
- `kernel_time_get_uptime()` — seconds since boot
- `kernel_time_get_ticks()` — PIT tick count
- `pit_get_ticks()` — raw PIT counter

## PIT Configuration
- Frequency: 50 Hz (20ms per tick)
- Used for: scheduler time slices, sleep(), uptime

## Known Issues
*(Agents add issues here)*
