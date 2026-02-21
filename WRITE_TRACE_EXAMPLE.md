# setRaumSoll Write Trace Example

## Complete Console Output Flow

This document shows the exact console output sequence when a user adjusts the room temperature setpoint in Home Assistant, from MQTT command reception through device confirmation.

### Scenario
**Home Assistant user sets room temperature to 22.5°C at T≈0 ms**

---

## Console Output Sequence

### Stage 1: VitoWiFi Initialization (at setup())

```
════════════════════════════════════════════════════════
[INIT] VitoWiFi initialized and ready
[INIT] Starting event-driven read scheduler...
[INIT] Polling cycle started
════════════════════════════════════════════════════════
```

---

### Stage 2: MQTT Command Received from Home Assistant

**User adjusts setpoint in HA dashboard** → MQTT broker publishes command → ArduinoHA routes to `setRaumSoll()` callback

```
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
[MQTT] setRaumSoll command received from Home Assistant
[MQTT] Parsed value: 22.5°C
[MQTT] Validation: value in range [10.0...30.0]
[MQTT] Calling vitoWIFI.write(setTempRaumSoll, 22.5) at T=12345 ms
```

---

### Stage 3: Write Request Queued

**vitoWIFI.write() called** → Returns `true` (request queued) → Write pending flag set → Read polling paused

```
[VITO] Write QUEUED: RaumSoll=22.5°C at T=12346 ms
[VITO] Round-trip latency: 1 ms (init → queue)
[VITO] Status: vitoWritePending=true, read polling PAUSED
[VITO] Waiting for onVitoResponse() or onVitoError()...
```

```
[MQTT] State reported back to Home Assistant
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
```

---

### Stage 4: Serial Transmission Over Optolink

**VitoWiFi state machine in loop()** → Serial data sent to Vitocal device (9600 baud) → Device processes request → Device responds

```
[Timeline internally in VitoWiFi library, not logged]
T=12346 ms: Write frame sent on Optolink serial (4800 baud)
T=12346-12600 ms: Device processing (~250 ms typical for write)
T=12600 ms: Device response received by ESP32-C3
```

---

### Stage 5: Write Confirmation

**onVitoResponse() callback fires** → Detects write datapoint → Logs confirmation → Clears pending flag → Resumes read polling

```
══════════════════════════════════════════════════════
[VITO] ✓ WRITE CONFIRMED: setTempRaumSoll at T=12600 ms
[VITO] Device accepted and processed the write request
[VITO] Resuming read polling (vitoWritePending=false)
══════════════════════════════════════════════════════
```

---

### Stage 6: Polling Resumes

**scheduleNextRead() reactivates** → Polling groups cycle normally

```
[onResponse continues normally with read processing...]
```

---

## Alternative: Write Error Scenario

If the device **rejects** or **doesn't respond** to the write:

### Stage 5 (Alternative): Write Error

**onVitoError() callback fires** → Error type logged → Clears pending flag → Resumes read polling

```
══════════════════════════════════════════════════════
[VITO] ✗ ERROR on datapoint 'setTempRaumSoll' at T=12650 ms
[VITO] Error type: TIMEOUT
[VITO] This was a WRITE operation
[VITO] Write failed - device rejected or did not respond
[VITO] Resuming read polling, Home Assistant may need to retry write
══════════════════════════════════════════════════════
```

---

## Timing Characteristics

| Event | Typical Time | Notes |
|-------|---|---|
| MQTT command parsing | ~1 ms | Callback dispatch |
| vitoWIFI.write() queue | ~0.5 ms | Non-blocking call |
| Serial transmission overhead | ~100 ms | 9600 baud at 4800 actual |
| Device processing | ~150–250 ms | Depends on device |
| Response received | ~250–350 ms total | From write queue to response |
| Total write latency | ~250–350 ms | MQTT input to confirmation |

**Without write priority mechanism (reads not paused):**
- Total write latency would be: ~2700–3000 ms (queued behind reads)

**With write priority mechanism (current):**
- Total write latency: ~250–350 ms (read polling paused)

---

## Console Tags Explained

| Tag | Source | Meaning |
|---|---|---|
| `[MQTT]` | `HA_mqtt_addin.h` (ArduinoHA callback) | Home Assistant command processing |
| `[VITO]` | `Vitocal_Optolink-esp32C3.ino` (VitoWiFi response/error) | VitoWiFi communication and status |
| `[INIT]` | `Vitocal_Optolink-esp32C3.ino` (setup()) | Initialization phase |

---

## How to Monitor in WebSerial

1. Open device IP in browser: `http://<esp32-ip>/webserial`
2. Adjust room temperature in Home Assistant
3. Watch console logs appear in real-time, showing the entire trace

Example WebSerial console output:
```
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
[MQTT] setRaumSoll command received from Home Assistant
[MQTT] Parsed value: 22.5°C
[MQTT] Validation: value in range [10.0...30.0]
[MQTT] Calling vitoWIFI.write(setTempRaumSoll, 22.5) at T=47832 ms
[VITO] Write QUEUED: RaumSoll=22.5°C at T=47833 ms
[VITO] Round-trip latency: 1 ms (init → queue)
[VITO] Status: vitoWritePending=true, read polling PAUSED
[VITO] Waiting for onVitoResponse() or onVitoError()...
[MQTT] State reported back to Home Assistant
════════════════════════════════════════════════════════
[VITO] ✓ WRITE CONFIRMED: setTempRaumSoll at T=48089 ms
[VITO] Device accepted and processed the write request
[VITO] Resuming read polling (vitoWritePending=false)
════════════════════════════════════════════════════════
```

---

## Datapoints Tracked for Write Confirmation

The following datapoints trigger write confirmation logging:

- `setTempRaumSoll` — Room temperature setpoint
- `setTempRaumSollRed` — Reduced room temperature setpoint
- `setTempHystWWsoll` — DHW hysteresis
- `setTempHKneigung` — Heating curve slope
- `setTempHKniveau` — Heating curve offset
- `setTempWWSoll` — DHW setpoint 1
- `setTempWWSoll2` — DHW setpoint 2

Any write to these datapoints will produce the `[VITO] ✓ WRITE CONFIRMED` or `[VITO] ✗ ERROR` output.

---

## Error Codes

| Error | Meaning | Typical Cause |
|---|---|---|
| TIMEOUT | No response from device | Device offline, serial error, or very slow response |
| LENGTH | Response length mismatch | Protocol error, corrupted packet |
| NACK | Device rejected request | Unsupported datapoint or device state prevents write |
| CRC | Checksum error | Transmission corruption |
| ERROR | Generic error | Unknown serial error |

---

## Debugging Tips

1. **Write never confirms:** Check if `vitoWritePending` is stuck true. Look for missing `vitoWritePending=false` in error handler.

2. **Write timeout errors:** Verify Optolink serial connection, baud rate (4800 baud), and signal integrity.

3. **NACK errors:** The datapoint address may not be supported by your device. Cross-reference with Viessmann documentation.

4. **No console output:** Verify WebSerial is working (`http://<ip>/webserial`) and CONSOLE_SERIAL is properly initialized.

