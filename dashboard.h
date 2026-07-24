#ifndef DASHBOARD_H
#define DASHBOARD_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>International 4300 Tactical</title>
    <style>
        body { font-family: sans-serif; background: #000; color: #e0e0e0; text-align: center; margin: 0; padding: 15px; padding-bottom: 140px; }
        h1 { font-size: 1.4rem; color: #fff; margin-bottom: 5px; }
        .links-bar { margin-bottom: 15px; font-size: 0.85rem; }
        .links-bar a { color: #00e6b8; text-decoration: none; margin: 0 10px; border: 1px solid #1c2130; padding: 4px 8px; border-radius: 4px; background: #11141d; }
        .lamp-container { display: flex; justify-content: center; flex-wrap: wrap; gap: 8px; margin-bottom: 15px; }
        .lamp { padding: 6px 10px; border-radius: 4px; font-weight: bold; background: #121212; color: #2a2a2a; font-size: 0.8rem; border: 1px solid #1a1a1a; }
        .lamp.mil.on { background: #cc7a00; color: #000; box-shadow: 0 0 10px #cc7a00; }
        .lamp.red.on { background: #cc0000; color: #fff; box-shadow: 0 0 10px #cc0000; }
        .lamp.amber.on { background: #b38f00; color: #000; box-shadow: 0 0 10px #b38f00; }
        .lamp.protect.on { background: #0052cc; color: #fff; box-shadow: 0 0 10px #0052cc; }
        .lamp.wts.on { background: #e65c00; color: #fff; box-shadow: 0 0 12px #e65c00; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 12px; max-width: 650px; margin: auto; }
        .card { background: #11141d; padding: 12px; border-radius: 10px; border: 1px solid #1c2130; }
        .card.wide { grid-column: span 2; background: #151a26; border-color: #263047; }
        .card.alert { border-color: #990000; background: #1a0808; }
        h2 { font-size: 0.8rem; color: #646c7c; margin: 0 0 6px 0; text-transform: uppercase; }
        .val { font-size: 2rem; font-weight: bold; color: #00e6b8; font-variant-numeric: tabular-nums; }
        .val.odo { color: #fff; font-size: 2.2rem; }
        .val.gear { color: #ff3388; font-size: 2.4rem; }
        .unit { font-size: 0.7rem; color: #444b59; margin-top: 2px; font-weight: bold; }
        .status { font-size: 0.75rem; color: #333; margin-top: 10px; }
        .status.connected { color: #00b33c; }
        .dtc-footer { position: fixed; bottom: 0; left: 0; right: 0; background: #0e0404; border-top: 2px solid #3d0a0a; padding: 12px; height: 85px; box-sizing: border-box; visibility: hidden; }
        .dtc-footer.active { visibility: visible; }
        .dtc-title { color: #ff3333; font-weight: bold; font-size: 0.85rem; margin-bottom: 2px; text-transform: uppercase; }
        .dtc-desc { font-size: 0.9rem; font-family: monospace; color: #ff8888; }
    </style>
</head>
<body>
    <h1>International 4300 Tactical</h1>
    <div class="links-bar">
        <a href="/faults" target="_blank">[LOGS] View Flash Logs</a>
        <a href="/clearfaults" onclick="return confirm('Wipe logs?');">[WARN] Wipe Memory</a>
    </div>
    <div id="ws-status" class="status">Connecting...</div>
    <div class="lamp-container">
        <div id="lamp-wts" class="lamp wts">WAIT TO START</div>
        <div id="lamp-mil" class="lamp mil">MIL</div>
        <div id="lamp-red" class="lamp red">STOP</div>
        <div id="lamp-amber" class="lamp amber">WARN</div>
        <div id="lamp-protect" class="lamp protect">PROT</div>
    </div>
    <div class="grid">
        <div class="card wide"><h2>Odometer</h2><div class="val odo" id="odo">0</div><div class="unit">TOTAL MILES</div></div>
        <div class="card"><h2>Gear</h2><div class="val gear" id="gear">--</div><div class="unit">SELECTED</div></div>
        <div class="card"><h2>Engine RPM</h2><div class="val" id="rpm">0</div><div class="unit">RPM</div></div>
        <div class="card"><h2>Speed</h2><div class="val" id="speed">0</div><div class="unit">MPH</div></div>
        <div class="card"><h2>Engine Load</h2><div class="val" id="load" style="color: #00fffa;">0</div><div class="unit">% LOAD</div></div>
        <div class="card"><h2>Turbo Boost</h2><div class="val" id="boost" style="color: #ff9900;">0.0</div><div class="unit">PSI</div></div>
        <div class="card"><h2>Fuel Rate</h2><div class="val" id="gph" style="color: #ff9900;">0.0</div><div class="unit">GPH</div></div>
        <div class="card"><h2>Oil Press</h2><div class="val" id="oil">0</div><div class="unit">PSI</div></div>
        <div id="c-coolant" class="card"><h2>Coolant</h2><div class="val" id="coolant">0</div><div class="unit">&deg;F</div></div>
        <div class="card"><h2>Trans Temp</h2><div class="val" id="trans">0</div><div class="unit">&deg;F</div></div>
        <div id="c-air1" class="card"><h2>Primary Air</h2><div class="val" id="air1">0</div><div class="unit">PSI</div></div>
        <div id="c-air2" class="card"><h2>Secondary Air</h2><div class="val" id="air2">0</div><div class="unit">PSI</div></div>
        <div id="c-volt" class="card"><h2>Battery</h2><div class="val" id="volt">0.0</div><div class="unit">Volts</div></div>
        <div class="card"><h2>Fuel Level</h2><div class="val" id="fuel">0</div><div class="unit">%</div></div>
    </div>
    <div id="dtc-anchor" class="dtc-footer">
        <div class="dtc-title">[WARN] Active System Fault Registered</div>
        <div id="dtc-text" class="dtc-desc">SPN: 0 | FMI: 0</div>
    </div>
    <script>
        let gateway = `ws://${window.location.hostname}/ws`, websocket;
        let lastActiveSPN = 0, lastActiveFMI = 0, dtcClearTimeout = null;

        const spnDictionary = {
            100: "Engine Oil Pressure Low",
            102: "Intake Manifold Boost Sensor Error",
            105: "Intake Manifold Air Temp Problem",
            108: "Barometric Atmospheric Pressure Failed",
            110: "Engine Coolant Overheating / Sensor Short",
            111: "Coolant Level Circuit Low Error",
            158: "ECU Keyswitch Battery Voltage Dropped",
            168: "Main Battery Power Voltage Low",
            190: "Engine Overspeed Condition Hit",
            513: "Actual Engine Percent Torque Error",
            629: "Engine Control Module (ECU) Failure",
            91:  "Accelerator Pedal Position Circuit Fault"
        };

        function resolveDTC(spn, fmi) {
            let baseDesc = spnDictionary[spn] || "Unknown Fault Component";
            let fmiDesc = "Data Erratic";
            if (fmi == 1) fmiDesc = "Valid But Below Range";
            else if (fmi == 3) fmiDesc = "Voltage High / Shorted";
            else if (fmi == 4) fmiDesc = "Voltage Low / Open Circuit";
            else if (fmi == 0) fmiDesc = "Valid But Above Range";
            return `SPN: ${spn} | FMI: ${fmi} - ${baseDesc} (${fmiDesc})`;
        }

        function initWebSocket() {
            websocket = new WebSocket(gateway);
            websocket.onopen = () => {
                document.getElementById('ws-status').innerText = "Streaming Cleaned Telemetry (High-Capacity Logger Active)";
                document.getElementById('ws-status').className = "status connected";
            };
            websocket.onclose = () => {
                document.getElementById('ws-status').innerText = "Reconnecting to Truck Network...";
                document.getElementById('ws-status').className = "status";
                setTimeout(initWebSocket, 2000);
            };
            websocket.onmessage = (e) => {
                let d = JSON.parse(e.data);
                let cleanVolt = (d.volt > 32) ? 0.0 : d.volt;

                let gearText = "--";
                if (d.gear == 125) gearText = "N";
                else if (d.gear == 126) gearText = "R";
                else if (d.gear == 251) gearText = "P";
                else if (d.gear >= 127 && d.gear <= 136) gearText = (d.gear - 126).toString();

                document.getElementById('odo').innerText = d.odo.toLocaleString();
                document.getElementById('gear').innerText = gearText;
                document.getElementById('rpm').innerText = Math.round(d.rpm);
                document.getElementById('speed').innerText = Math.round(d.speed);
                document.getElementById('load').innerText = Math.round(d.load);
                document.getElementById('boost').innerText = d.boost.toFixed(1);
                document.getElementById('gph').innerText = d.gph.toFixed(1);
                document.getElementById('oil').innerText = Math.round(d.oil);
                document.getElementById('coolant').innerText = Math.round(d.coolant);
                document.getElementById('trans').innerText = Math.round(d.trans);
                document.getElementById('air1').innerText = Math.round(d.air1);
                document.getElementById('air2').innerText = Math.round(d.air2);
                document.getElementById('volt').innerText = cleanVolt.toFixed(1);
                document.getElementById('fuel').innerText = Math.round(d.fuel);
                
                toggleLamp('lamp-mil', d.lMIL);
                toggleLamp('lamp-red', d.lRED);
                toggleLamp('lamp-amber', d.lAMB);
                toggleLamp('lamp-protect', d.lPRT);
                toggleLamp('lamp-wts', d.lWTS);

                toggleAlert('c-coolant', d.coolant > 220);
                toggleAlert('c-air1', d.air1 < 90);
                toggleAlert('c-air2', d.air2 < 90);
                toggleAlert('c-volt', cleanVolt < 11.8 && cleanVolt > 5);

                let footer = document.getElementById('dtc-anchor');
                if (d.spn > 0 && d.spn < 524287) {
                    lastActiveSPN = d.spn;
                    lastActiveFMI = d.fmi;
                    document.getElementById('dtc-text').innerText = resolveDTC(d.spn, d.fmi);
                    footer.classList.add('active');
                    if (dtcClearTimeout) { clearTimeout(dtcClearTimeout); dtcClearTimeout = null; }
                } else {
                    if (footer.classList.contains('active') && !dtcClearTimeout) {
                        dtcClearTimeout = setTimeout(() => {
                            footer.classList.remove('active');
                            dtcClearTimeout = null;
                        }, 5000); 
                    }
                }
            };
        }
        function toggleLamp(id, s) {
            let el = document.getElementById(id);
            if(s == 1) el.classList.add('on'); else el.classList.remove('on');
        }
        function toggleAlert(id, c) {
            let el = document.getElementById(id);
            if(c) el.classList.add('alert'); else el.classList.remove('alert');
        }
        window.addEventListener('load', initWebSocket);
    </script>
</body>
</html>
)rawliteral";

#endif
