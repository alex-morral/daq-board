# Dashboard walkthrough — how the live monitor works

The dashboard is three pieces connected in a line:

```
STM32 ──USB serial──▶ Node server (server.js) ──WebSocket──▶ Browser (index.html)
      ◀──DAC/PGA commands───────────────────────────────────
```

## 1. The board (firmware)

Once per loop the STM32 prints one text line over USB serial at 115200 baud:

```
T:29.60|A0:0.580|A1:0.581|A2:0.579|A3:0.580|D:2048|P:1
```

`T`=temperature, `A0..A3`=ADC channels (volts), `D`=DAC code, `P`=PGA index.
A failed reading is sent as the literal `err`.

## 2. The server (`dashboard/server.js`, Node.js)

Its job is to bridge serial ↔ browser, because a web page can't open a COM port
directly. It:

- **Auto-detects the CH340** by USB vendor ID `1a86` (`SerialPort.list()`), so
  you don't hard-code COM3/COM4.
- Reads lines, **parses** them into a JSON object (turning `err` into `null`).
- **Broadcasts** each sample to all connected browsers over WebSocket.
- Forwards `DAC:`/`PGA:` commands from the browser back to the serial port.
- **Reconnects** automatically if the board is unplugged (it no longer crashes).

Messages sent to the browser are tagged: `{type:'status', connected, port}` for
link state and `{type:'sample', temperature, channels[], dac, pga}` for data.

## 3. The browser (`dashboard/public/index.html`)

A single self-contained page (HTML + CSS + vanilla JS, no framework):

- Opens a **WebSocket** to the server and listens for messages.
- `status` messages drive the connection indicator.
- `sample` messages update the six **measurement tiles**, the **4-channel scope**
  (drawn on a `<canvas>`), the **temperature chart**, running **min/max/avg**
  statistics, and the **data-stream log**.
- Moving the **DAC slider** or changing the **PGA** sends a command back through
  the WebSocket → server → board.
- **REC** buffers samples; **CSV** exports them as a file.
- A `null` (error) reading shows as **ERR** in red instead of a fake number.

## Run it

```
cd dashboard
npm install
npm start          # auto-detects the port → http://localhost:3000
```

If auto-detect ever fails, pass the port explicitly: `node server.js COM4`.

## Why this architecture?

It's the same shape as professional test software: the instrument speaks a
simple line protocol, a host process bridges the transport, and the UI is
decoupled from the hardware. You could swap the browser for a Python client, or
the serial link for TCP, without touching the firmware.
