// DAQ Board dashboard server
// Bridges the STM32's USB-serial stream to the browser over WebSocket, and
// forwards DAC/PGA commands back to the board. Auto-detects the CH340 port
// and survives unplug/replug without crashing.

const express = require('express');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const { WebSocketServer } = require('ws');
const path = require('path');

const BAUD = 115200;
const HTTP_PORT = 3000;
const CH340_VENDOR = '1a86';          // WCH CH340 USB vendor ID
const cliPort = process.argv[2];       // optional explicit COM port

const app = express();
app.use(express.static(path.join(__dirname, 'public')));
const server = app.listen(HTTP_PORT, () =>
    console.log(`Dashboard:  http://localhost:${HTTP_PORT}`));
const wss = new WebSocketServer({ server });

let serial = null;
let connected = false;

function broadcast(obj) {
    const msg = JSON.stringify(obj);
    wss.clients.forEach(c => { if (c.readyState === 1) c.send(msg); });
}

// Find the CH340 serial port (or use the one given on the command line).
async function findPort() {
    if (cliPort) return cliPort;
    const ports = await SerialPort.list();
    const match = ports.find(p =>
        (p.vendorId || '').toLowerCase() === CH340_VENDOR);
    return match ? match.path : null;
}

async function connectSerial() {
    let portPath;
    try {
        portPath = await findPort();
    } catch (e) {
        portPath = null;
    }

    if (!portPath) {
        broadcast({ type: 'status', connected: false, port: null });
        console.log('No CH340 found — retrying in 2s…');
        setTimeout(connectSerial, 2000);
        return;
    }

    serial = new SerialPort({ path: portPath, baudRate: BAUD }, (err) => {
        if (err) {
            console.log(`Open ${portPath} failed: ${err.message} — retry in 2s`);
            setTimeout(connectSerial, 2000);
        }
    });

    const parser = serial.pipe(new ReadlineParser({ delimiter: '\r\n' }));

    serial.on('open', () => {
        connected = true;
        console.log(`Serial:     ${portPath} @ ${BAUD}`);
        broadcast({ type: 'status', connected: true, port: portPath });
    });

    parser.on('data', (line) => {
        const data = parseLine(line);
        if (data) broadcast(data);
    });

    serial.on('error', (err) => console.log('Serial error:', err.message));

    serial.on('close', () => {
        if (connected) console.log('Serial closed — reconnecting…');
        connected = false;
        broadcast({ type: 'status', connected: false, port: null });
        setTimeout(connectSerial, 2000);
    });
}

wss.on('connection', (ws) => {
    // Tell the freshly-connected client the current link state.
    ws.send(JSON.stringify({ type: 'status', connected,
        port: connected && serial ? serial.path : null }));

    ws.on('message', (msg) => {
        const cmd = msg.toString();
        const ok = cmd.startsWith('DAC:') || cmd.startsWith('PGA:') ||
                   cmd.startsWith('GEN:') || cmd.startsWith('LOG:') ||
                   cmd.startsWith('CAP:');
        if (ok && connected && serial) {
            serial.write(cmd + '\n', (e) => { if (e) console.log('write err', e.message); });
        }
    });
});

// Parse the board's serial lines into tagged objects for the browser.
// "G:<mode>:<freq>" is the function-generator status; "T:..." is a sample.
function parseLine(line) {
    if (line.startsWith('G:')) {
        const [, mode, freq] = line.split(':');
        return { type: 'gen', mode: parseInt(mode), freq: parseInt(freq) };
    }
    // Datalogger: status, dump header, one record, dump end.
    if (line.startsWith('LG:')) {
        const [, st, count] = line.split(':');
        return { type: 'log', logging: st === '1', count: parseInt(count) };
    }
    if (line.startsWith('LD:')) {
        return { type: 'logdump', count: parseInt(line.slice(3)) };
    }
    if (line.startsWith('LR:')) {
        const f = line.slice(3).split(',');   // idx,temp,a0,a1,a2,a3
        return { type: 'logrec', index: f[0], temp: f[1], ch: f.slice(2) };
    }
    if (line.startsWith('LE')) {
        return { type: 'logend' };
    }
    // High-speed capture block: "CB:<rate>:<n>:v0,v1,..."
    if (line.startsWith('CB:')) {
        const p = line.split(':');
        const samples = p[3] ? p[3].split(',').map(Number) : [];
        return { type: 'capture', rate: parseInt(p[1]), n: parseInt(p[2]), samples };
    }
    // "T:29.60|A0:0.580|A1:err|A2:0.456|A3:0.789|D:2048|P:1"
    // A field of "err" becomes null so the UI can show a fault instead of a number.
    if (!line.startsWith('T:')) return null;
    const p = {};
    line.split('|').forEach(kv => { const [k, v] = kv.split(':'); p[k] = v; });
    if (p.T === undefined || p.A0 === undefined || p.D === undefined) return null;

    const num = (v) => (v === 'err' || v === undefined) ? null : parseFloat(v);
    return {
        type: 'sample',
        timestamp: Date.now(),
        temperature: num(p.T),
        channels: [num(p.A0), num(p.A1), num(p.A2), num(p.A3)],
        dac: parseInt(p.D),
        pga: parseInt(p.P)
    };
}

connectSerial();
