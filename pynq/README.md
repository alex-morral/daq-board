# PYNQ integration — DAQ Board → FPGA FIR → PC

Glue for running the board's signal through a FIR filter on a **PYNQ-Z2** FPGA.
This ties the three portfolio projects (PCB · FPGA · embedded) into one chain.

## Wiring

```
PC ── Ethernet ──▶ PYNQ-Z2            (Jupyter)
PYNQ USB host ── USB-A↔USB-C ──▶ DAQ Board   (powers the board + serial link)
```

The DAQ board's CH340 enumerates on the PYNQ as `/dev/ttyUSB0`. The board is
powered from the PYNQ's USB host port (it comes up ~20 s after boot, once Linux
enables the port).

## FPGA side (separate repo)

The FIR lives in [fir-filter-zynq](https://github.com/alex-morral/fir-filter-zynq).
It was originally fed by an I²S audio codec; to let the processing system inject
samples it needed one small RTL change in `src/audio_fir_top.vhd`:

- **control bit 3** = external mode (samples come from AXI instead of I²S),
- **register 0x04 (Din)** = write a 16-bit sample → drives the FIR input and
  pulses `enable` for one cycle,
- a source **mux** on the FIR input (I²S *or* AXI).

Rebuild the bitstream and copy `audio_system.bit` (+ the unchanged `.hwh`) to the
PYNQ next to `daq_fir_demo.py`.

The exact modified top-level used for this integration is kept here as a
reference: [`audio_fir_top.vhd`](audio_fir_top.vhd) (drop it into the FIR
project's `src/` and rebuild).

## Run

STM32 firmware must include the `SIG` command (streams a test signal); it's in
this repo's `firmware/`. Then, in a Jupyter notebook on the PYNQ:

```python
%run daq_fir_demo.py
```

It streams one block from the board, filters it in the FPGA, and plots input vs
output. The audio path (I²S, headphones) still works unchanged — the FIR just has
two selectable sources now.

## Notes / gotchas

- The PYNQ has no internet on a point-to-point Ethernet link, so `pip install
  pyserial` fails — the script reads the serial device directly (`open()` +
  `stty`), no dependency.
- `ser.flush()` after `write()` is required or the command isn't sent.
- The FIR has a 1-sample pipeline latency, flushed with a final dummy write.
