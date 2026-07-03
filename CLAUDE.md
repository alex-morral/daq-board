# Proyecto PCB — Portfolio de Telecomunicaciones (DAQ Board)

> **Última actualización: 03/07/2026** — **BRING-UP EN CURSO.** Alimentación OK, USB OK (COM4), STM32 detectado por SWD. Firmware bare-metal funcionando: blink LED ✅, TMP117 ✅, ADS1115 ✅, MCP4725 ✅. Falta: W25Q32 (flash SPI). Toolchain: arm-none-eabi-gcc (C:\arm-gcc) + OpenOCD (C:\openocd) + Zadig/WinUSB.

## Sobre el estudiante
- Alex Morral, estudiante de Ingeniería Electrónica de Telecomunicaciones
- Construyendo portfolio personal con proyectos prácticos
- Enfoque: aprender de verdad haciendo proyectos, no copiar-pegar
- Objetivo profesional: **FPGA/RTL Design Engineer o Hardware Engineer (junior)**
- Sectores target: test & measurement, defensa, 5G, semiconductores

## Plan de portfolio (3 proyectos)
1. **FPGA** — COMPLETADO (https://github.com/alex-morral/fir-filter-zynq) — Filtro FIR en Zynq con audio en tiempo real (I2S + ADAU1761). Flujo completo: RTL → simulación → síntesis → implementación → validación en HW.
2. **PCB con Altium** (este proyecto) — **PEDIDO PAGADO, EN PRODUCCIÓN en JLCPCB**
3. **Embebido STM32** — Pendiente (reutilizará esta placa como base)

## Proyectos anteriores completados
- **FIR Filter on Zynq SoC**: filtro FIR configurable en VHDL sobre PYNQ-Z2. Audio en tiempo real vía I2S + codec ADAU1761. AXI-Lite, testbenches, timing closure, validación en HW real. (VHDL, Vivado, Python, Zynq)
- **Monitor Industrial**: ESP32 + sensores de temperatura + simulación módulo de potencia 48V-12V. C++ Arduino, MQTT, Node.js, Telegram Bot. (https://github.com/alex-morral/monitor-industrial)
- **Home IoT Monitor**: ESP32 + PIR + DHT11 + LDR. Dashboard web tiempo real, SQLite, alertas Telegram. (https://github.com/alex-morral/home-iot-monitor)

## Tecnologías que Alex ya domina
- VHDL, FPGA (Vivado, Quartus II), Zynq SoC, AXI, DSP
- ESP32 / Arduino IDE / C++ / C
- MQTT, Node.js, Express, WebSockets
- Dashboards web (HTML/CSS/JS, Chart.js, ApexCharts)
- EDA: LTSpice, MATLAB, Eagle, PSoC Creator, LabVIEW, Code Composer Studio, **Altium Designer**
- Instrumentación: osciloscopio, multímetro, generador de tensión, generador de funciones
- Claude Code (usuario avanzado)

---

## QUÉ ES ESTE PROYECTO

Placa de **adquisición de datos (DAQ) de propósito general**. Conectas señales del mundo real (voltajes, temperatura), el STM32 las digitaliza, procesa, guarda y/o envía al PC. También genera señales de salida. Herramienta de test & measurement. Demuestra: buses I2C/SPI/USB/UART, diseño analógico (ADC/DAC) + digital (MCU/Flash), regulación de potencia (LDO), y el flujo completo esquemático → layout → fabricación → validación.

**Componentes principales:**
| Componente | Designador | Función | Bus / Nota |
|---|---|---|---|
| STM32F103C8T6 | U3 (LQFP-48) | MCU principal | — |
| ADS1115 | U5 | ADC 16-bit, 4 canales (AIN0-3) | I2C **0x48** |
| TMP117 | U6 | Sensor temperatura precisión | I2C **0x49** |
| MCP4725 (variante **A0**) | U8 | DAC, salida voltaje (DAC_OUT) | I2C **0x60** |
| W25Q32 | U7 | Flash SPI 4MB | SPI |
| CH340G | U1 | Puente USB-UART | Alimentado a **3.3V** (V3→VCC) |
| USBLC6-2SC6 | U2 | Protección ESD USB | — |
| AMS1117-3.3 | U4 | Regulador 3.3V | — |
| USB4105-GF-A | J1 | Conector USB-C (alim. + datos) | — |
| Header 10p | J2 | Expansión (GPIOs libres + VCC/GND) | — |
| SWD 4p | J3 | SWDIO/SWCLK/VCC/GND | Debug |
| 3× LED | D1/D2/D3 | Estado (PC13/PB0/PB1) | R **1kΩ** |
| Botón | S1 | Reset | — |
| Cristales | Y1 / Y2 | 12MHz (CH340) / 8MHz (STM32) | — |

**Specs fabricación:** Altium Designer 26.5.1 (licencia estudiante **SN-09857170**) · JLCPCB assembly económico · 2 capas · 59.94×40.01mm · FR-4 1.6mm · HASL · 5 placas montadas.

---

## ESTADO ACTUAL (03/07/2026) — BRING-UP EN CURSO ✅

### Periféricos validados
- **Alimentación (AMS1117):** 3.3V OK ✅
- **USB/CH340:** COM4, driver CH341SER instalado ✅
- **STM32 (SWD):** detectado vía ST-Link V2 + OpenOCD, Cortex-M3 OK ✅
- **Blink LED (PB0):** primer firmware bare-metal funcionando ✅
- **TMP117 (I2C 0x49):** lee temperatura ~29.6°C, estable ✅
- **ADS1115 (I2C 0x48):** lee AIN0, ~0.580V flotante (correcto sin señal) ✅
- **MCP4725 (I2C 0x60):** DAC_OUT = 1.64V medido con multímetro (teórico 1.65V, DAC=2048/4096) ✅
- **W25Q32 (SPI):** ⏳ pendiente

### Toolchain instalado
- **Compilador:** arm-none-eabi-gcc 13.3.1 en `C:\arm-gcc`
- **Programador:** OpenOCD 0.12.0 en `C:\openocd`
- **Driver ST-Link:** WinUSB vía Zadig (la web de ST no dejaba descargar drivers)
- **ST-Link V2:** ICQUANZX, conectado a J3 (SWDIO/SWCLK/3.3V/GND)

### Fotos de bring-up (en `images/`)
1. **LED parpadeando** — primer firmware, valida STM32 + cristal + SWD
2. **tmp117_serial.png** — Serial Monitor ~29.60°C en tiempo real, valida I2C + TMP117
3. **ads1115_tmp117_serial.png** — TMP117 + ADS1115 leyendo simultáneamente
4. **mcp4725_dac_output.png** — multímetro midiendo 1.64V en DAC_OUT

### Siguiente paso
1. **W25Q32 (flash SPI)** — escribir/leer datos, validar bus SPI
2. Firmware final con todos los periféricos
3. Dashboard web en tiempo real
4. Montar repo GitHub con README + fotos

### Pedido JLCPCB (histórico)
- **Nº pedido:** W2026062403055595 — pagado 23/06/2026, versión Y3 (R5=10k corregido)
- **Coste:** $135.07 (5 placas montadas, DHL Express)

---

## AUDITORÍA DEL ESQUEMÁTICO — COMPLETADA 6/6 ✅ (lo mejor para el README)

Auditado pin por pin contra datasheets, bloque a bloque, **antes** de pagar. Resultado: diseño sano, **un solo error real (R5)**.

- **Bloque 1 — Alimentación (AMS1117):** VIN→+5, VOUT+tab→VCC (tab unido a VOUT, error nº1 del 1117, aquí bien), GND→GND. C2/C6 = 10µF in/out.
- **Bloque 2 — STM32:** todos los VDD/VDDA→VCC, VSS/VSSA→GND, VBAT→VCC, BOOT0(44)→GND, reloj Y2 OK. **🔴 R5 era 100Ω → corregido a 10kΩ (C17414).** Único error real. (Los zigzags de VDD eran cosméticos de Altium.)
- **Bloque 3 — USB:** cruce UART correcto (CH340 RXD→TX, TXD→RX), CH340 V3→VCC (op. 3.3V), USBLC6 sin corto D+/D− (1&6=D−, 3&4=D+), R1/R2=5.1k confirmado, DP1=DP2 / DN1=DN2.
- **Bloque 4 — I²C:** direcciones verificadas y sin colisión → ADS1115 **0x48**, TMP117 **0x49**, MCP4725 **0x60** (¡variante A0, NO 0x62!). Pull-ups R3/R4=4.7k. ALERT/RDY del ADS1115 = NC.
- **Bloque 5 — Flash SPI:** bus correcto (PA4-7), **/HOLD(7)→VCC y /WP(3)→VCC** (crítico de la W25Q32, bien). C11=100nF.
- **Bloque 6 — LEDs/SWD/header:** polaridad LED correcta (cátodo→GND), R=1kΩ. **🟡 D1 en PC13 = modo fuente** (contra spec ST, lucirá tenue). J3 SWD: SWDIO/SWCLK/VCC/GND. J2: GPIOs libres + VCC/GND.

---

## LISTA DE CORRECCIONES

### Hecha en esta sesión ✅
- **R5: 100Ω → 10kΩ** (C17414). En el BOM `DAQ_Board_BOM_R5_10k.csv`, ya en el pedido pagado.

### Para rev. B (NO bloqueaban — anotadas)
- **🔴 AIN0-3 no accesibles (hallazgo firmware, 04/07/2026):** las entradas del ADC (ADS1115 pines 4-7) **no salen a ningún header ni test point** — los nets AIN0-3 solo existen en el chip, quedan flotando. J2 saca GPIOs del STM32 (PA0/PA1/PA3/PA8/PB8/PB9/PB15) pero **ningún AIN**. Es una limitación de fondo: una placa DAQ debería exponer sus entradas de ADC para medir señales externas. El ADC funciona (verificado eléctricamente, lee ~0.58V flotante y responde al cambio de PGA), pero sin retrabajo no se le puede conectar nada. **Rev. B: sacar AIN0-3 + una referencia GND a un header de 4-5 pines.** Buen material de README (fallo cazado por criterio, igual que R5). En el dashboard se muestran los 4 canales leyendo el mismo ruido flotante — es honesto, no un bug.
- **D1 (PC13):** cablear en modo sumidero (LED desde VCC → R → PC13). La lógica de firmware de LED1 se invertiría.
- **LEDs más tenues de lo planeado:** el plan original eran 330Ω; se fabricaron a **1kΩ** → los tres lucen más flojos, y **D1 (PC13) será el más tenue** por el modo fuente. En el bring-up, si se ven apagados es esperado, no un fallo.
- **VDDA sin filtrar:** añadir ferrita + 100nF/1µF (impacto mínimo: se usa el ADS1115 externo, no el ADC interno).
- **NRST sin cap:** añadir 100nF a GND.
- **Desacoplo STM32:** hay 3×100nF; lo de libro son 4 (uno por VDD).
- **Caps de cristal a 22pF:** revisar contra el CL del cristal (Y2 12pF→~15pF; Y1 18pF→~27pF). Arrancan igual.
- **NRST en header SWD:** añadir para "connect under reset".
- **Sincronizar Altium:** R5 se cambió solo en el BOM de JLCPCB, no en el esquemático fuente. Al renovar la licencia, corregir R5 a 10k en Altium y regenerar el BOM para que la fuente coincida con la placa real.

---

## EXTENSIONES FUTURAS (comprometidas — hacer al terminar el proyecto base)

Mínimo estas cinco, en orden de retorno para el portfolio:

1. **Generador de funciones con el DAC** — con un timer HW, sacar seno/triangular/cuadrada por DAC_OUT. Convierte la placa en fuente de señal. Muy vistoso.
2. **Datalogger real a la W25Q32** — grabar muestras del ADC en la flash de la placa y volcarlas por USB. Le da sentido al chip que ahora solo saludamos en el bring-up.
3. **Muestreo con timer + DMA + UART por interrupción** — sube el sample rate de ~pocos Hz a cientos/miles. Es el salto de "demo" a "firmware serio" (el más importante técnicamente).
4. **Integración con la PYNQ (filtro FIR)** — la joya: PCB (adquisición) → FPGA (DSP en tiempo real) → PC. Une los 3 proyectos del portfolio. Vía UART/SPI por J2 a un PMOD de la PYNQ-Z2.
5. **Rutina de calibración guardada en flash** — medir un voltaje conocido, corregir offset/ganancia del ADC y persistir la calibración en la W25Q32.

---

## PINOUT Y CONEXIONES (referencia)

| Net | Pin STM32 | Conectado a |
|---|---|---|
| SCL / SDA | PB6 / PB7 | ADS1115, MCP4725, TMP117 + R3/R4 (4.7k pull-up) |
| SCK / MISO / MOSI | PA5 / PA6 / PA7 | W25Q32 CLK / DO / DI |
| FLASH_CS | PA4 | W25Q32 /CS |
| TX / RX | PA9 / PA10 | CH340G RXD / TXD |
| SWDIO / SWCLK | PA13 / PA14 | J3 pin 1 / 2 |
| LED1 / LED2 / LED3 | PC13 / PB0 / PB1 | R (1k) → D → GND *(D1 tenue, modo fuente)* |
| BOOT0 | pin 44 | GND |
| NRST | pin 7 | R5 (10k) → VCC + S1 → GND |
| OSC_IN / OSC_OUT | PD0 / PD1 (5/6) | Y2 (8MHz) + C16/C14 (22pF) → GND |

### Direcciones I2C (verificadas)
- ADS1115: **0x48** (ADDR→GND) · TMP117: **0x49** (ADD0→VCC) · MCP4725: **0x60** (A0→GND, variante A0 — **no usar 0x62**)

### Notas de diseño (lo realmente fabricado)
- Desacoplo: 3×100nF (C4/C7/C9) + 1µF (C12) en STM32; 100nF por chip I2C/flash.
- Pull-ups I2C 4.7kΩ. LEDs con **1kΩ** (no 330Ω). VDDA directo a VCC (sin filtrar).
- AMS1117: 10µF in (C2) + 10µF out (C6). Cristales con caps de **22pF**.

---

## QUÉ HARÁ ALEX CUANDO LLEGUE LA PLACA (fase firmware, Claude Code)
1. Inspección física + soldar J2/J3 a mano si vienen sin montar
2. **Test alimentación con multímetro (5V entrada, 3.3V salida) ANTES de enchufar USB**
3. Detección USB (CH340G → puerto COM)
4. Programar por SWD (J3) con ST-Link
5. Parpadeo LED (valida STM32 + cristal) — recordar que **los LEDs lucen tenues y D1 el más flojo**
6. Leer TMP117 por I2C (0x49)
7. Leer ADS1115 (0x48, medir voltaje conocido)
8. Sacar voltaje por MCP4725 (**0x60**, no 0x62), medir con multímetro
9. Guardar/leer flash por SPI (W25Q32)
10. Enviar datos al PC por USB + dashboard web tiempo real

---

## PLAN GITHUB (al final del proyecto)
```
daq-board/
├── README.md          ← historia + la AUDITORÍA del esquemático
├── hardware/          ← esquemático PDF, gerbers, BOM de diseño (cantidad por placa, sin precios)
├── docs/              ← capturas, DRC 23-cortos vs limpio, auditoría
├── firmware/          ← código (Claude Code)
└── images/            ← fotos placa, bring-up
```
**Material estrella para el README:**
- Routing: el auto-router (Situs) dio "135/135 100%" pero el DRC reveló **23 cortos + 30 clearance** → Unroute All + ruteo a mano. Capturas DRC-23-cortos vs DRC-limpio.
- **La auditoría pin a pin antes de fabricar:** cazó R5=100Ω→10k, verificó 3 direcciones I²C, /WP-/HOLD de la flash, el tab del AMS1117, descartó un falso corto en el USBLC6, detectó lo de PC13/D1. Narrativa de criterio de ingeniería real.
- **NO subir al repo público:** dirección, factura, ni VAT/datos de la empresa del padre.

---

## INSTRUCCIONES PARA EL NUEVO CHAT / CLAUDE CODE

### Contexto a recordar
- Esquemático auditado 6/6 (sano, solo R5 era error → corregido a 10k). Placa **rehecha (Y3), pedida y PAGADA** (W2026062403055595).
- "Confirmar colocación" ACTIVADA → revisar el render antes de que monten (sobre todo LEDs). NO aprobar en automático.
- **Siguiente:** revisar DFM + confirmación de colocación. Luego, al llegar la placa → **firmware en Claude Code** (bring-up por pasos, empezando por test de alimentación con multímetro ANTES de USB).

### Estilo de trabajo (IMPORTANTE)
- NO decir "perfecto" sin verificar bien. Consultar datasheets antes de confirmar conexiones.
- Si no estás seguro, dilo y busca la info. No verificar visualmente algo y afirmar que está bien = no hacerlo. Si Alex confirma una lectura que tú no puedes ver, queda de su lado.
- Alex paga de su bolsillo (o de la empresa): los errores cuestan dinero y tiempo.
- En temas fiscales/legales: explicar el mecanismo, remitir al gestor para la decisión.

### Flujo de trabajo
- Diseño Altium (esquemático + layout + Gerbers) → Claude online (HECHO).
- Firmware STM32, documentación y GitHub → Claude Code (VSCode, PC principal).
