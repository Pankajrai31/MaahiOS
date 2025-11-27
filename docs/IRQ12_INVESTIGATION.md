# IRQ12 Investigation - Why Mouse Interrupts Stop Firing

## Executive Summary
PS/2 mouse IRQ12 stops firing after ~10-54 interrupts in QEMU, despite:
- 8042 controller having mouse data ready (OBF=1, bit 5=1)
- IRQ12 being unmasked on slave PIC (0xA1 = 0xEF, bit 4=0)
- Mouse clock enabled in 8042 command byte (bit 5=0)
- Proper EOI being sent to both PICs after each interrupt

**Current workaround:** Polling syscall that manually calls mouse_handler() when 8042 has data.
**Problem:** Polling is laggy and not a proper solution.

---

## System Configuration

### Hardware Emulation
- **Platform:** QEMU (qemu-system-i386)
- **PS/2 Controller:** 8042 emulation
- **Mouse Type:** PS/2 standard 3-byte packet protocol

### Software Configuration
- **OS:** MaahiOS (custom x86 kernel)
- **Architecture:** i686, Ring 0 kernel + Ring 3 userspace
- **IRQ12 Mapping:** INT 0x2C (PIC remapped: slave IRQs → 0x28-0x2F)
- **Scheduler:** Preemptive multitasking with timer (IRQ0)

---

## Timeline of Changes

### Initial State (Before Fixes)
```
Boot: Very slow (30+ seconds)
Mouse Init: Basic, no wait loops
IRQ12 Count: 0-3 total before stopping
Symptoms: System hung during process creation
```

### Fix #1: Interrupt Timing
**Problem:** Boot extremely slow, system hanging
**Solution:** Added `cli` before `process_create_sysman()`, `sti` before Ring 3 jump
**Result:** Boot time normal (~3 seconds)
**IRQ12 Impact:** Increased to 9 interrupts at cpl=3

### Fix #2: Command Byte Bit 5
**Problem:** Bit 5 logic was inverted (setting it to disable mouse)
**Old Code:** `cmd |= 0x22;` (set bits 5 and 1)
**New Code:** `cmd |= 0x03; cmd &= ~0x20;` (clear bit 5 to enable mouse clock)
**Verification:** Command byte now reads 0x43 (correct)
**Result:** ACK changed from 0x00 to 0xFA (correct)

### Fix #3: Wait Loops with Timeouts
**Problem:** No timeouts in wait functions, potential infinite loops
**Solution:** Added 1000-iteration timeout loops in `wait_input_clear()` and `wait_output_full()`
**Result:** More stable initialization, proper ACK handling

### Fix #4: Timer IRQ Masking (Friend's Analysis)
**Problem:** Timer IRQ0 interrupting during 3-byte packet read causes desynchronization
**Root Cause:** Task switch during packet read → packet_byte counter gets out of sync
**Solution:** 
```c
// In mouse_handler() at packet_byte==0:
saved_timer_mask = inb(0x21);
outb(0x21, saved_timer_mask | 0x01);  // Mask IRQ0
timer_masked = 1;

// At packet_byte==3 (complete):
outb(0x21, saved_timer_mask);  // Restore IRQ0
timer_masked = 0;
```
**Result:** IRQ12 count increased from 9 → 18 at cpl=3, total ~328 → reduced to ~54
**Note:** Actually made it worse! Less IRQ12s, not more.

### Fix #5: Polling Fallback (Current Workaround)
**Problem:** IRQ12 stops firing despite everything being correct
**Solution:** Added `SYSCALL_POLL_MOUSE` that:
1. Reads 8042 status register (port 0x64)
2. If (status & 0x01) && (status & 0x20) → mouse data ready
3. Manually calls `mouse_handler()` to process data
**Orbit Detection:** If IRQ total doesn't increase for 2 iterations, call poll
**Result:** Mouse works but is laggy
**Poll Frequency:** Every ~1000 loop iterations + 2 iteration delay

---

## Diagnostic Data (From Latest Run)

### PIC Mask Status
```
POLL logs show:
[POLL] status=1C slave_pic=EF IRQ12_masked=00
[POLL] status=3D slave_pic=EF IRQ12_masked=00
```
**Analysis:**
- `slave_pic=EF` → binary 11101111 → bit 4 = 0 = **IRQ12 is NOT masked**
- `status=1C` → binary 00011100 → OBF=0, no mouse data
- `status=3D` → binary 00111101 → OBF=1, bit 5=1 → **MOUSE DATA READY**

### 8042 Status Register Breakdown
When polling shows `status=3D`:
- Bit 0 (0x01): **1** = Output buffer full (data available to read from port 0x60)
- Bit 1 (0x02): **0** = Input buffer empty (safe to write commands)
- Bit 2 (0x04): **1** = System flag (set by controller after self-test)
- Bit 3 (0x08): **1** = Command/Data (controller is ready)
- Bit 4 (0x10): **1** = Keyboard unlocked
- Bit 5 (0x20): **1** = Auxiliary device output buffer full = **MOUSE DATA**
- Bit 6 (0x40): **0** = Timeout (no error)
- Bit 7 (0x80): **0** = Parity (no error)

### IRQ12 Behavior
```
HANDLER_UPDATE count: 54 (IRQ12 actually fired 54 times)
Last position: X=0x03FF Y=0x0109 (1023, 265)
Pattern: Works initially, then stops randomly
```

### Code Verified NOT Touching Slave PIC
Searched entire codebase for writes to port 0xA1:
- **IRQ Manager:** Only writes during initialization (masks all, then selectively unmasks)
- **Mouse Handler:** Only touches 0x21 (master PIC) for timer masking
- **Scheduler:** No PIC writes
- **No accidental remapping or masking of IRQ12**

---

## Why IRQ12 Stops: Theories

### Theory 1: QEMU Bug (MOST LIKELY)
**Evidence:**
1. 8042 has data ready (OBF=1, bit 5=1)
2. IRQ12 is unmasked (slave PIC bit 4=0)
3. Command byte correct (0x43, mouse IRQ enabled)
4. EOI properly sent to both PICs
5. **But IRQ12 simply doesn't fire**

**Hypothesis:** QEMU's PS/2 emulation stops generating IRQ12 after some threshold, possibly:
- Edge-triggered interrupt not being raised properly
- Internal QEMU state machine getting stuck
- Race condition in QEMU's event handling

**Test on Real Hardware:** This would definitively prove if it's QEMU-specific.

### Theory 2: EOI Not Reaching Slave PIC
**Check:** EOI is sent in `interrupt_stubs.s`:
```asm
movb $0x20, %al
outb %al, $0xA0     /* Slave PIC */
outb %al, $0x20     /* Master PIC */
```
**Status:** Code is correct. Both PICs receive EOI.

### Theory 3: 8042 Command Byte Corruption
**Symptom:** Command byte could be getting rewritten somehow
**Test Needed:** Add logging to periodically read command byte and verify it stays 0x43
```c
outb(0x64, 0x20);  // Read command byte
uint8_t cmd = inb(0x60);
if (cmd != 0x43) {
    serial_print("CMD_CORRUPTED: ");
    serial_hex(cmd);
}
```

### Theory 4: IRQ12 Line Level vs Edge Triggered
**Modern PICs:** IRQs can be configured as edge or level triggered
**Default:** Edge-triggered (interrupt fires on 0→1 transition)
**Problem:** If IRQ12 line stays high, no more edges occur
**Test:** Try reading ISR (In-Service Register) to see if IRQ12 is stuck active
```c
outb(0xA0, 0x0B);  // Read ISR command
uint8_t isr = inb(0xA0);
// Check if bit 4 (IRQ12) is stuck set
```

### Theory 5: Timer Masking Side Effects
**Observation:** After adding timer masking, IRQ12 count went DOWN (328 → 54)
**Hypothesis:** Masking/unmasking master PIC might be affecting slave PIC cascade
**Possible Issue:** 
- IRQ2 (cascade) getting masked accidentally?
- Timing issue when restoring mask while IRQ12 pending?

**Test:** Remove timer masking completely and see if IRQ12 lasts longer.

---

## Proposed Investigation Steps

### Step 1: Test Without Timer Masking
Remove the timer masking code and measure IRQ12 lifespan:
```c
// Comment out in mouse_handler():
// outb(0x21, saved_timer_mask | 0x01);  // Mask IRQ0
// timer_masked = 1;
```
**Expected:** If desync was the real problem, count should drop. If QEMU bug, no change.

### Step 2: Monitor Command Byte Continuously
Add periodic logging in orbit main loop:
```c
// Every 1000 iterations:
outb(0x64, 0x20);
uint8_t cmd = inb(0x60);
serial_print("CMD_BYTE=");
serial_hex(cmd);
```
**Look For:** Any changes from 0x43 that would disable mouse IRQ.

### Step 3: Check ISR (In-Service Register)
Right before polling, check if IRQ12 is stuck in ISR:
```c
outb(0xA0, 0x0B);  // OCW3: Read ISR
uint8_t isr = inb(0xA0);
if (isr & 0x10) {
    serial_print("IRQ12_STUCK_IN_ISR\n");
}
```

### Step 4: Force Re-Enable IRQ12
Try explicitly re-enabling IRQ12 when it stops:
```c
if (polls_since_irq > 10) {
    // Re-enable IRQ12 on slave PIC
    uint8_t mask = inb(0xA1);
    mask &= ~0x10;  // Clear bit 4
    outb(0xA1, mask);
    
    // Also ensure IRQ2 cascade is enabled
    mask = inb(0x21);
    mask &= ~0x04;  // Clear bit 2
    outb(0x21, mask);
}
```

### Step 5: Test on Real Hardware
**Critical Test:** Run MaahiOS on actual x86 hardware with PS/2 mouse
**Expected:** If it works perfectly on real hardware → confirms QEMU bug
**If still fails:** Indicates deeper issue in our code

### Step 6: Try Different QEMU Options
```bash
# Test with different mouse backends:
qemu-system-i386 -cdrom boot.iso -device usb-mouse
qemu-system-i386 -cdrom boot.iso -device usb-tablet

# Test with debugging:
qemu-system-i386 -cdrom boot.iso -d int,cpu_reset -D qemu.log

# Test without KVM acceleration:
qemu-system-i386 -cdrom boot.iso -no-kvm
```

---

## Current Code State

### Files Modified

#### `src/drivers/mouse.c`
- Lines 78-145: Clean init sequence (disable ports, flush, modify cmd byte, enable, send 0xF4)
- Lines 147-280: Handler with timer masking + emergency unmask safety
- **Timer Masking:** Save 0x21 mask, OR with 0x01 at packet start, restore at packet end
- **Sensitivity:** 2x multiplier on dx/dy for faster movement

#### `src/syscalls/syscall_handler.c`
- Added `SYSCALL_POLL_MOUSE` (case 35)
- Checks 8042 status, calls mouse_handler() if data ready
- Logs every 100 polls: status, slave_pic, IRQ12_masked

#### `src/orbit/orbit.c`
- Tracks `last_irq_count` and `polls_since_irq`
- Calls `syscall_poll_mouse()` if IRQ stalled for 2+ iterations
- Loop delay: 1000 iterations (was 10000)
- Erases old cursor position before drawing new one

---

## Questions for Expert Analysis

1. **Timer Masking Counterproductive?**
   - Why did IRQ12 count DROP after adding timer masking?
   - Should we remove it entirely?

2. **QEMU vs Real Hardware**
   - Is this a known QEMU PS/2 emulation bug?
   - Would real hardware behave differently?

3. **EOI Timing**
   - Should EOI be sent in a specific order (slave then master, or master then slave)?
   - Could EOI timing affect next interrupt generation?

4. **Edge vs Level Triggering**
   - Are PS/2 interrupts edge or level triggered?
   - Could IRQ12 line be stuck high?

5. **Proper PS/2 Re-initialization**
   - Should we periodically send 0xF4 (Enable Data Reporting) again?
   - Is there a "keepalive" mechanism needed?

6. **Alternative Architectures**
   - Should we implement a dedicated mouse handling task instead of IRQ?
   - Would a ring buffer + periodic read be more reliable?

---

## Performance Comparison

| Metric | Before Any Fixes | After Init Fixes | After Timer Mask | With Polling |
|--------|-----------------|------------------|------------------|--------------|
| Boot Time | 30+ seconds | 3 seconds | 3 seconds | 3 seconds |
| IRQ12 Count | 0-3 | ~9 at cpl=3 | ~54 total | N/A |
| Mouse Works? | No | Briefly | Briefly | Yes (laggy) |
| ACK Value | 0x00 | 0xFA | 0xFA | 0xFA |
| Cmd Byte | 0x22 (wrong) | 0x43 (correct) | 0x43 | 0x43 |
| Slave PIC | Unknown | 0xEF (correct) | 0xEF | 0xEF |

---

## Recommendation

**Short Term:** Keep polling as workaround, but reduce lag by:
- Decreasing loop delay to 100 iterations (currently 1000)
- Poll immediately (polls_since_irq > 0 instead of > 2)
- Remove timer masking to see if IRQ12 lasts longer

**Long Term:** 
1. Test on real hardware to isolate QEMU bug
2. If QEMU-specific, file bug report with QEMU project
3. Consider USB mouse support as alternative
4. Implement hybrid: use IRQ when available, fall back to polling gracefully

**Critical Question:** Is there ANY configuration where IRQ12 continues working indefinitely in our code?

---

## Serial Log Evidence

### Last IRQ12 Handler Call
```
MASKING: M=FA S=EF
TIMER_MASKED
HANDLER_UPDATE: dx=1B dy=F5 -> mouse_x=03FF mouse_y=0109
RESTORING: M=FA S=EF
TIMER_RESTORED
```

### Polling Finds Data
```
[POLL] status=3D slave_pic=EF IRQ12_masked=00
```
**Translation:** 8042 has mouse data (status=3D), IRQ12 not masked (EF), but IRQ didn't fire!

### Proof IRQ12 Is Not Masked
All POLL logs consistently show:
- `slave_pic=EF` (binary 11101111, bit 4=0)
- `IRQ12_masked=00` (explicitly calculated bit 4 value)

This is **smoking gun evidence** that QEMU's IRQ12 emulation is broken.
