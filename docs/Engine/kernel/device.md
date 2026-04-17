# Kernel Device Manager

## Files
- `managers/device/device_manager.c/.h`

## Purpose
Unified interface for all hardware devices. Drivers register with a
`device_ops_t` function pointer table. User-space accesses devices
through SYS_DEV_* syscalls which route to the device manager.

## API
- `kernel_device_open(dev_id)` → driver.ops.open()
- `kernel_device_close(dev_id)` → driver.ops.close()
- `kernel_device_read(dev_id, buf, size)` → driver.ops.read()
- `kernel_device_write(dev_id, buf, size)` → driver.ops.write()
- `kernel_device_ioctl(dev_id, cmd, arg)` → driver.ops.ioctl()
- `kernel_device_poll(dev_id)` → driver.ops.poll()
- `kernel_device_list(buf, max)` — enumerate registered devices

## Registered Devices
Devices are registered at boot by kernel.c. Each gets an integer ID.

## Known Issues
*(Agents add issues here)*
