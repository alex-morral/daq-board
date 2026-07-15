"""
daq_fir_demo.py — DAQ Board -> FPGA FIR -> PC

Runs on the PYNQ-Z2 (Jupyter). Reads a test signal streamed by the DAQ board
over USB serial, pushes it sample-by-sample through the FIR filter in the FPGA
fabric (over AXI-Lite), and plots input vs filtered output.

Setup:
  - DAQ board USB -> PYNQ USB host port (powers the board + serial link).
  - PYNQ reached over Ethernet (Jupyter).
  - FIR overlay (audio_system.bit/.hwh) with the AXI "Din" register — see the
    fir-filter-zynq project (src/audio_fir_top.vhd): ctrl bit 3 = external mode,
    reg 0x04 = write-a-sample-and-pulse-enable.

No pyserial needed (the PYNQ has no internet to pip-install it): the serial port
is a plain file, configured with `stty` and read/written with open().
"""

import os, time
import matplotlib.pyplot as plt
from pynq import Overlay

# --- FPGA overlay + FIR handle ---
ol  = Overlay("audio_system.bit")
fir = ol.audio_fir_0

# AXI-Lite register map (base of audio_fir_0):
#   0x00 control (b0 enable, b1 reset, b2 passthrough, b3 external source)
#   0x04 Din  (write a sample -> feeds the FIR, pulses enable)   [external mode]
#   0x08 Dout (last filtered sample, read-only)
#   0x10-0x1C coefficients 0..3 (Q1.15)

def set_coeffs(c0, c1, c2, c3):
    for i, c in enumerate([c0, c1, c2, c3]):
        v = max(-32768, min(32767, int(c * 32768)))
        fir.write(0x10 + i * 4, v & 0xFFFF)

def fir_block(samples):
    """Push a list of samples through the FPGA FIR and return the filtered output.
    The filter has a 1-sample pipeline latency, flushed with a final dummy write."""
    out = []
    for x in samples:
        fir.write(0x04, int(x) & 0xFFFF)
        y = fir.read(0x08) & 0xFFFF
        if y >= 0x8000:
            y -= 0x10000
        out.append(y)
    fir.write(0x04, 0)
    y = fir.read(0x08) & 0xFFFF
    if y >= 0x8000:
        y -= 0x10000
    out.append(y)
    return out[1:]

def read_signal(port="/dev/ttyUSB0", timeout=10):
    """Ask the DAQ board for one test-signal block ("SIG:1" -> "FX:v0,v1,...")."""
    os.system(f"stty -F {port} 115200 raw -echo min 0 time 10")
    ser = open(port, "r+b", buffering=0)
    ser.write(b"SIG:1\n"); ser.flush()          # flush is required
    buf, samples, deadline = b"", None, time.time() + timeout
    while time.time() < deadline and samples is None:
        chunk = ser.read(512)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            s = line.strip().decode(errors="ignore")
            if s.startswith("FX:"):
                samples = [int(v) for v in s[3:].split(",") if v.lstrip("-").isdigit()]
                break
    ser.write(b"SIG:0\n"); ser.flush()
    ser.close()
    return samples

# --- Run the chain ---
set_coeffs(0.25, 0.25, 0.25, 0.25)   # 4-tap moving average: low-pass, nulls f_s/4
fir.write(0x00, 0x08 | 0x02)          # external mode + reset
fir.write(0x00, 0x08)                 # external mode

samples  = read_signal()
print("samples received:", len(samples) if samples else 0)
filtered = fir_block(samples)

plt.figure(figsize=(11, 4))
plt.plot(samples,  label="Input (STM32): sine + noise at f_s/4", alpha=0.7)
plt.plot(filtered, label="Output (FPGA FIR): filtered", linewidth=2)
plt.title("DAQ Board -> FPGA FIR -> PC")
plt.xlabel("sample"); plt.ylabel("amplitude"); plt.legend(); plt.grid(True)
plt.show()
