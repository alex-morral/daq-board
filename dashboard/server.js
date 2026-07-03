const express = require('express');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const { WebSocketServer } = require('ws');
const path = require('path');

const PORT_NAME = process.argv[2] || 'COM4';
const HTTP_PORT = 3000;

const app = express();
app.use(express.static(path.join(__dirname, 'public')));

const server = app.listen(HTTP_PORT, () => {
    console.log(`Dashboard: http://localhost:${HTTP_PORT}`);
});

const wss = new WebSocketServer({ server });

const serial = new SerialPort({ path: PORT_NAME, baudRate: 9600 });
const parser = serial.pipe(new ReadlineParser({ delimiter: '\r\n' }));

serial.on('open', () => console.log(`Serial: ${PORT_NAME} open`));
serial.on('error', (err) => console.error('Serial error:', err.message));

parser.on('data', (line) => {
    const data = parseLine(line);
    if (!data) return;

    const msg = JSON.stringify(data);
    wss.clients.forEach((client) => {
        if (client.readyState === 1) client.send(msg);
    });
});

wss.on('connection', (ws) => {
    ws.on('message', (msg) => {
        const cmd = msg.toString();
        if (cmd.startsWith('DAC:') || cmd.startsWith('PGA:')) {
            serial.write(cmd + '\n');
        }
    });
});

function parseLine(line) {
    // Format: T:29.60|A0:0.580|A1:0.123|A2:0.456|A3:0.789|D:2048|P:1
    if (!line.startsWith('T:')) return null;

    const parts = {};
    line.split('|').forEach(p => {
        const [key, val] = p.split(':');
        parts[key] = val;
    });

    if (!parts.T || !parts.A0 || !parts.D) return null;

    return {
        timestamp: Date.now(),
        temperature: parseFloat(parts.T),
        channels: [
            parseFloat(parts.A0),
            parseFloat(parts.A1),
            parseFloat(parts.A2),
            parseFloat(parts.A3)
        ],
        dac: parseInt(parts.D),
        pga: parseInt(parts.P)
    };
}
