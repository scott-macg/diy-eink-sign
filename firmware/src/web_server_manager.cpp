#include "web_server_manager.h"

WebServerManager webServerManager;

// Embedded Maintenance Dashboard HTML/CSS/JS in PROGMEM
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>E-Ink Sign Maintenance Console</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --border-color: rgba(255, 255, 255, 0.1);
            --primary: #38bdf8;
            --primary-hover: #0284c7;
            --accent: #ef4444;
            --text: #f8fafc;
            --text-dim: #94a3b8;
            --terminal-bg: #020617;
            --font: 'Segoe UI', system-ui, -apple-system, sans-serif;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: var(--font);
            background-color: var(--bg-color);
            color: var(--text);
            padding: 1.5rem;
            min-height: 100vh;
        }
        .container { max-width: 900px; margin: 0 auto; }
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding-bottom: 1rem;
            border-bottom: 1px solid var(--border-color);
            margin-bottom: 1.5rem;
        }
        h1 { font-size: 1.5rem; font-weight: 600; color: var(--primary); }
        .badge {
            background: rgba(56, 189, 248, 0.15);
            color: var(--primary);
            padding: 0.3rem 0.75rem;
            border-radius: 9999px;
            font-size: 0.85rem;
            border: 1px solid rgba(56, 189, 248, 0.3);
        }
        .tabs { display: flex; gap: 0.5rem; margin-bottom: 1.5rem; }
        .tab-btn {
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            color: var(--text-dim);
            padding: 0.6rem 1.2rem;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 500;
            transition: all 0.2s;
        }
        .tab-btn.active, .tab-btn:hover {
            color: var(--text);
            background: var(--primary-hover);
            border-color: var(--primary);
        }
        .tab-content { display: none; background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 12px; padding: 1.5rem; backdrop-filter: blur(12px); }
        .tab-content.active { display: block; }
        
        /* Terminal Styling */
        #terminal {
            background: var(--terminal-bg);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 1rem;
            font-family: 'Courier New', Courier, monospace;
            height: 350px;
            overflow-y: auto;
            color: #4ade80;
            white-space: pre-wrap;
            margin-bottom: 1rem;
        }
        .input-group { display: flex; gap: 0.5rem; }
        input[type="text"], input[type="number"], select {
            flex: 1;
            background: #020617;
            border: 1px solid var(--border-color);
            color: var(--text);
            padding: 0.6rem 1rem;
            border-radius: 6px;
            font-size: 0.95rem;
        }
        button {
            background: var(--primary);
            color: #0f172a;
            border: none;
            padding: 0.6rem 1.2rem;
            border-radius: 6px;
            font-weight: 600;
            cursor: pointer;
            transition: opacity 0.2s;
        }
        button:hover { opacity: 0.9; }
        
        /* Form & Table Styling */
        .form-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; margin-bottom: 1rem; }
        .form-group { display: flex; flex-direction: column; gap: 0.4rem; }
        .form-group label { font-size: 0.85rem; color: var(--text-dim); }
        table { width: 100%; border-collapse: collapse; margin-top: 1rem; }
        th, td { text-align: left; padding: 0.75rem; border-bottom: 1px solid var(--border-color); }
        th { color: var(--text-dim); font-size: 0.85rem; }
        .btn-danger { background: var(--accent); color: white; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>DIY E-Ink Smart Sign</h1>
            <span class="badge" id="statusBadge">Connecting...</span>
        </header>

        <div class="tabs">
            <button class="tab-btn active" onclick="switchTab('repl')">Web REPL Console</button>
            <button class="tab-btn" onclick="switchTab('config')">Configuration</button>
            <button class="tab-btn" onclick="switchTab('files'); loadFiles();">File Manager</button>
        </div>

        <!-- REPL Tab -->
        <div id="tab-repl" class="tab-content active">
            <div id="terminal">--- Connected to Seeed Studio XIAO ESP32-C6 Web REPL ---\nType 'help' for a list of available commands.\n</div>
            <div class="input-group">
                <input type="text" id="cmdInput" placeholder="Enter command (e.g. info, ls, config, play 1000 200, refresh)..." onkeydown="if(event.key==='Enter') sendCmd()">
                <button onclick="sendCmd()">Send</button>
            </div>
        </div>

        <!-- Config Tab -->
        <div id="tab-config" class="tab-content">
            <form id="configForm" onsubmit="saveConfig(event)">
                <div class="form-grid">
                    <div class="form-group">
                        <label>Wi-Fi SSID</label>
                        <input type="text" id="cfg_wifi_ssid">
                    </div>
                    <div class="form-group">
                        <label>Wi-Fi Password</label>
                        <input type="text" id="cfg_wifi_pass" placeholder="••••••••">
                    </div>
                    <div class="form-group">
                        <label>Backend Server URL</label>
                        <input type="text" id="cfg_server_url">
                    </div>
                    <div class="form-group">
                        <label>Display Token</label>
                        <input type="text" id="cfg_display_token">
                    </div>
                    <div class="form-group">
                        <label>WiFi Timeout (ms)</label>
                        <input type="number" id="cfg_wifi_timeout_ms">
                    </div>
                    <div class="form-group">
                        <label>Default Sleep (Seconds)</label>
                        <input type="number" id="cfg_default_sleep_sec">
                    </div>
                    <div class="form-group">
                        <label>Maintenance Mode Timeout (Seconds)</label>
                        <input type="number" id="cfg_maintenance_timeout_sec">
                    </div>
                    <div class="form-group">
                        <label>E-Paper Refresh Mode</label>
                        <select id="cfg_refresh_mode">
                            <option value="3">Two-Pass: Fast B/W Cycling -> Delayed 3-Color Clean (Default)</option>
                            <option value="2">Fast B/W Partial Refresh (~1.5s)</option>
                            <option value="1">Partial Window Quote Update (~5s)</option>
                            <option value="0">Full 3-Color Refresh (~14s)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Audio Battery Alert</label>
                        <select id="cfg_audio_battery_alert">
                            <option value="false">Disabled (Default)</option>
                            <option value="true">Enabled</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Persistent Developer Mode</label>
                        <select id="cfg_developer_mode">
                            <option value="false">Disabled (Deep Sleep Active)</option>
                            <option value="true">Enabled (Disable Deep Sleep & Keep Web REPL)</option>
                        </select>
                    </div>
                </div>
                <button type="submit">Save Configuration</button>
            </form>
        </div>

        <!-- Files Tab -->
        <div id="tab-files" class="tab-content">
            <h3>Upload New File</h3>
            <div class="input-group" style="margin-top: 0.5rem; margin-bottom: 1.5rem;">
                <input type="file" id="fileUploadInput">
                <button onclick="uploadFile()">Upload to LittleFS</button>
            </div>
            <h3>Filesystem Contents</h3>
            <table>
                <thead>
                    <tr><th>Filename</th><th>Size</th><th>Actions</th></tr>
                </thead>
                <tbody id="filesTableBody">
                    <tr><td colspan="3">Loading files...</td></tr>
                </tbody>
            </table>
        </div>
    </div>

    <script>
        let ws;
        function initWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81/');
            ws.onopen = () => {
                document.getElementById('statusBadge').innerText = 'Maintenance Mode (Connected)';
                loadConfig();
            };
            ws.onmessage = (evt) => {
                const term = document.getElementById('terminal');
                term.innerText += evt.data + '\n';
                term.scrollTop = term.scrollHeight;
            };
            ws.onclose = () => {
                document.getElementById('statusBadge').innerText = 'Disconnected';
                setTimeout(initWebSocket, 3000);
            };
        }

        function switchTab(name) {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            event.target.classList.add('active');
            document.getElementById('tab-' + name).classList.add('active');
        }

        function sendCmd() {
            const input = document.getElementById('cmdInput');
            const val = input.value.trim();
            if (val && ws && ws.readyState === WebSocket.OPEN) {
                const term = document.getElementById('terminal');
                term.innerText += '> ' + val + '\n';
                ws.send(val);
                input.value = '';
            }
        }

        async function loadConfig() {
            try {
                const res = await fetch('/api/config');
                const cfg = await res.json();
                document.getElementById('cfg_wifi_ssid').value = cfg.wifi_ssid || '';
                document.getElementById('cfg_server_url').value = cfg.server_url || '';
                document.getElementById('cfg_display_token').value = cfg.display_token || '';
                document.getElementById('cfg_wifi_timeout_ms').value = cfg.wifi_timeout_ms || 15000;
                document.getElementById('cfg_default_sleep_sec').value = cfg.default_sleep_sec || 3600;
                document.getElementById('cfg_maintenance_timeout_sec').value = cfg.maintenance_timeout_sec || 300;
                document.getElementById('cfg_refresh_mode').value = (cfg.refresh_mode !== undefined) ? cfg.refresh_mode : 3;
                document.getElementById('cfg_audio_battery_alert').value = cfg.audio_battery_alert ? 'true' : 'false';
                document.getElementById('cfg_developer_mode').value = cfg.developer_mode ? 'true' : 'false';
            } catch(e) { console.error(e); }
        }

        async function saveConfig(e) {
            e.preventDefault();
            const payload = {
                wifi_ssid: document.getElementById('cfg_wifi_ssid').value,
                wifi_pass: document.getElementById('cfg_wifi_pass').value,
                server_url: document.getElementById('cfg_server_url').value,
                display_token: document.getElementById('cfg_display_token').value,
                wifi_timeout_ms: parseInt(document.getElementById('cfg_wifi_timeout_ms').value),
                default_sleep_sec: parseInt(document.getElementById('cfg_default_sleep_sec').value),
                maintenance_timeout_sec: parseInt(document.getElementById('cfg_maintenance_timeout_sec').value),
                refresh_mode: parseInt(document.getElementById('cfg_refresh_mode').value),
                audio_battery_alert: document.getElementById('cfg_audio_battery_alert').value === 'true',
                developer_mode: document.getElementById('cfg_developer_mode').value === 'true'
            };
            const res = await fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            if (res.ok) alert('Configuration saved successfully!');
            else alert('Failed to save configuration.');
        }


        async function loadFiles() {
            try {
                const res = await fetch('/api/files');
                const files = await res.json();
                const tbody = document.getElementById('filesTableBody');
                tbody.innerHTML = '';
                if (files.length === 0) {
                    tbody.innerHTML = '<tr><td colspan="3">No files found on LittleFS.</td></tr>';
                    return;
                }
                files.forEach(f => {
                    const tr = document.createElement('tr');
                    const tdName = document.createElement('td');
                    tdName.textContent = f.name; // Safe: textContent never executes HTML
                    const tdSize = document.createElement('td');
                    tdSize.textContent = f.size + ' B';
                    const tdAction = document.createElement('td');
                    const btn = document.createElement('button');
                    btn.className = 'btn-danger';
                    btn.textContent = 'Delete';
                    btn.dataset.path = f.name; // Store path in data attribute (not inline onclick)
                    btn.addEventListener('click', () => deleteFile(f.name));
                    tdAction.appendChild(btn);
                    tr.appendChild(tdName);
                    tr.appendChild(tdSize);
                    tr.appendChild(tdAction);
                    tbody.appendChild(tr);
                });
            } catch(e) { console.error(e); }
        }

        async function uploadFile() {
            const input = document.getElementById('fileUploadInput');
            if (input.files.length === 0) return alert('Select a file to upload!');
            const formData = new FormData();
            formData.append('data', input.files[0], input.files[0].name);
            const res = await fetch('/api/upload', { method: 'POST', body: formData });
            if (res.ok) { alert('File uploaded successfully!'); loadFiles(); }
            else alert('Upload failed!');
        }

        async function deleteFile(path) {
            if (!confirm('Are you sure you want to delete ' + path + '?')) return;
            const res = await fetch('/api/delete?path=' + encodeURIComponent(path), { method: 'DELETE' });
            if (res.ok) loadFiles();
            else alert('Failed to delete file.');
        }

        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

WebServerManager::WebServerManager() : server(80), webSocket(81) {}

void WebServerManager::begin(RefreshDisplayCallback refreshCb, 
                          ReadBatteryVoltageCallback vbatCb, 
                          ReadBatteryPercentCallback battPctCb,
                          PlayToneCallback playToneCb) {
    onRefreshDisplay = refreshCb;
    onReadVbat = vbatCb;
    onReadBattPct = battPctCb;
    onPlayTone = playToneCb;

    setupRoutes();
    server.begin();

    webSocket.begin();
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        this->handleWebSocketEvent(num, type, payload, length);
    });

    Serial.println("[WEB] Maintenance Web Server & WebSockets started.");
}

void WebServerManager::handleClient() {
    server.handleClient();
    webSocket.loop();
}

void WebServerManager::broadcastLog(const String& message) {
    String msg = message;
    webSocket.broadcastTXT(msg);
}

void WebServerManager::setupRoutes() {
    server.on("/", [this]() { this->handleRoot(); });
    server.on("/api/config", HTTP_GET, [this]() { this->handleGetConfig(); });
    server.on("/api/config", HTTP_POST, [this]() { this->handlePostConfig(); });
    server.on("/api/files", HTTP_GET, [this]() { this->handleListFiles(); });
    server.on("/api/delete", HTTP_DELETE, [this]() { this->handleDeleteFile(); });
    
    server.on("/api/upload", HTTP_POST, 
        [this]() { server.send(200, "text/plain", "OK"); },
        [this]() { this->handleFileUpload(); }
    );

    server.onNotFound([this]() { this->handleNotFound(); });
}

void WebServerManager::handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void WebServerManager::handleGetConfig() {
    server.send(200, "application/json", configManager.toJsonString());
}

void WebServerManager::handlePostConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Body missing");
        return;
    }
    String body = server.arg("plain");
    if (configManager.updateFromJson(body)) {
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

void WebServerManager::handleListFiles() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    File root = LittleFS.open("/", "r");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            JsonObject obj = arr.add<JsonObject>();
            obj["name"] = String(file.name());
            obj["size"] = file.size();
            file = root.openNextFile();
        }
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebServerManager::handleFileUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        // Reject path traversal attempts
        if (filename.indexOf("..") != -1) {
            Serial.println("[WEB] Upload rejected: filename contains path traversal sequence");
            return;
        }
        if (!filename.startsWith("/")) filename = "/" + filename;
        Serial.printf("[WEB] File Upload Start: %s\n", filename.c_str());
        uploadFile = LittleFS.open(filename, "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("[WEB] File Upload Finished: %u bytes\n", upload.totalSize);
        }
    }
}

void WebServerManager::handleDeleteFile() {
    if (!server.hasArg("path")) {
        server.send(400, "text/plain", "Missing path parameter");
        return;
    }
    String path = server.arg("path");
    // Reject path traversal attempts
    if (path.indexOf("..") != -1) {
        server.send(400, "text/plain", "Invalid path");
        return;
    }
    if (!path.startsWith("/")) path = "/" + path;

    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        server.send(200, "text/plain", "Deleted");
    } else {
        server.send(404, "text/plain", "File not found");
    }
}

void WebServerManager::handleNotFound() {
    server.send(404, "text/plain", "404 Not Found");
}

void WebServerManager::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        String command = String((char*)payload).substring(0, length);
        command.trim();
        executeReplCommand(num, command);
    }
}

void WebServerManager::executeReplCommand(uint8_t clientNum, const String& command) {
    String reply = "";
    if (command == "help") {
        reply = "Available Commands:\n"
                "  info             - Display system status & battery info\n"
                "  ls               - List LittleFS files\n"
                "  cat <file>       - Display contents of a file\n"
                "  rm <file>        - Delete a file\n"
                "  config           - Display current configuration JSON\n"
                "  set <key> <val>  - Update a config option (e.g., set default_sleep_sec 1800)\n"
                "  play <freq> <ms> - Play a buzzer test tone (e.g. play 1000 200)\n"
                "  refresh          - Trigger e-paper display refresh\n"
                "  reboot           - Restart ESP32-C6 microcontroller";
    } else if (command == "info" || command == "status") {
        float vbat = onReadVbat ? onReadVbat() : 0.0f;
        int pct = onReadBattPct ? onReadBattPct() : 0;
        reply = String("--- System Info ---\n") +
                "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n" +
                "Battery: " + String(vbat, 2) + "V (" + String(pct) + "%)\n" +
                "Wi-Fi IP: " + WiFi.localIP().toString() + "\n" +
                "Wi-Fi RSSI: " + String(WiFi.RSSI()) + " dBm\n" +
                "Uptime: " + String(millis() / 1000) + " seconds";
    } else if (command == "ls") {
        File root = LittleFS.open("/", "r");
        reply = "--- Filesystem List (/ ---\n";
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                reply += String("  ") + file.name() + " (" + String(file.size()) + " B)\n";
                file = root.openNextFile();
            }
        }
    } else if (command.startsWith("cat ")) {
        String path = command.substring(4);
        path.trim();
        if (!path.startsWith("/")) path = "/" + path;
        if (LittleFS.exists(path)) {
            File f = LittleFS.open(path, "r");
            reply = "--- Content of " + path + " ---\n" + f.readString();
            f.close();
        } else {
            reply = "File not found: " + path;
        }
    } else if (command.startsWith("rm ")) {
        String path = command.substring(3);
        path.trim();
        if (!path.startsWith("/")) path = "/" + path;
        if (LittleFS.exists(path)) {
            LittleFS.remove(path);
            reply = "Deleted: " + path;
        } else {
            reply = "File not found: " + path;
        }
    } else if (command == "config") {
        reply = configManager.toJsonString();
    } else if (command.startsWith("set ")) {
        int spaceIdx = command.indexOf(' ', 4);
        if (spaceIdx != -1) {
            String key = command.substring(4, spaceIdx);
            String val = command.substring(spaceIdx + 1);
            key.trim(); val.trim();
            if (configManager.updateKey(key, val)) {
                reply = "Updated key '" + key + "' = '" + val + "'";
            } else {
                reply = "Failed to update key '" + key + "'";
            }
        } else {
            reply = "Usage: set <key> <value>";
        }
    } else if (command.startsWith("play ")) {
        int spaceIdx = command.indexOf(' ', 5);
        if (spaceIdx != -1) {
            uint16_t freq = command.substring(5, spaceIdx).toInt();
            uint16_t dur = command.substring(spaceIdx + 1).toInt();
            if (onPlayTone) onPlayTone(freq, dur);
            reply = "Playing tone " + String(freq) + " Hz for " + String(dur) + " ms";
        } else {
            reply = "Usage: play <freq> <duration_ms>";
        }
    } else if (command == "refresh") {
        reply = "Triggering display refresh...";
        webSocket.sendTXT(clientNum, reply);
        if (onRefreshDisplay) onRefreshDisplay();
        return;
    } else if (command == "reboot") {
        reply = "Rebooting ESP32-C6...";
        webSocket.sendTXT(clientNum, reply);
        delay(500);
        ESP.restart();
        return;
    } else {
        reply = "Unknown command: '" + command + "'. Type 'help' for options.";
    }

    webSocket.sendTXT(clientNum, reply);
}
