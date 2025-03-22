#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// Default IP: http://192.168.4.1/

const char* AP_SSID = "ESP32-AP";
const char* AP_PASSWORD = "12345678";

String staSSID;
String staPASSWORD;

String connStatus = "Not connected";
IPAddress connIP = IPAddress();

String camStatus = "Offline";
IPAddress camIP = IPAddress();

unsigned long lastHeartbeat = 0;
int failedCamConnAttempts = 0;

bool isGateOpen = false;
bool isValveOpen = false;

unsigned long gateCloseTimer = 0;
unsigned long valveCloseTimer = 0;

const int gatePin;
const int valvePin;

WebServer server(80);

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    pinMode(gatePin, OUTPUT);
    pinMode(valvePin, OUTPUT);

    digitalWrite(gatePin, LOW);
    digitalWrite(valvePin, LOW);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/connect", HTTP_POST, handleConn);
    server.on("/disconnect", HTTP_GET, handleDisconnect);
    server.on("/ssid", HTTP_GET, handleSSID);
    server.on("/password", HTTP_GET, handlePassword);
    server.on("/ip", HTTP_GET, handleIP);
    server.on("/conn/status", HTTP_GET, handleConnStatus);
    server.on("/cam/status", HTTP_POST, handleCamStatus);
    server.on("/cam/status", HTTP_GET, handleCamStatus);
    server.on("/cam/ip", HTTP_POST, handleCamIP);
    server.on("/cam/ip", HTTP_GET, handleCamIP);

    // pin route controls
    server.on("/opengate", HTTP_POST, handleOpenGate);

    server.begin();
}

void loop() {
    server.handleClient();
    if (millis() - lastHeartbeat > 3000 && camStatus == "Online") {
        lastHeartbeat = millis();
        checkCamTimeout();
    }

    if (WiFi.status() != WL_CONNECTED && connStatus == "Connected") {
        Serial.printf("\nDisconnected from %s\n", staSSID.c_str());
        staSSID = "";
        staPASSWORD = "";
        connStatus = "Not connected";
        connIP = IPAddress();
    }

    checkGate();
    checkValve();
}

void checkGate() {
    if (isGateOpen && millis() >= gateCloseTimer) {
        digitalWrite(gatePin, LOW);
        isGateOpen = false;
    }
}

void checkValve() {
    if (isValveOpen && millis() >= valveCloseTimer) {
        digitalWrite(valvePin, LOW);
        isValveOpen = false;
    }
}

void checkCamTimeout() {
    bool isCamOnline = pingCam();

    if (!isCamOnline) {
        failedCamConnAttempts++;
        if (failedCamConnAttempts >= 3) {
            camStatus = "Offline";
            camIP = IPAddress();
            failedCamConnAttempts = 0;
        }
    } else failedCamConnAttempts = 0;
}

bool pingCam() {
    if (camIP == IPAddress()) return false;
    HTTPClient http;
    http.begin("http://" + camIP.toString() + "/");
    int httpCode = http.GET();
    http.end();
    return httpCode == 200;
}

bool connect(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    unsigned long startTime = millis();
    while (millis() - startTime < 8000) {
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(500);
    }

    WiFi.disconnect(true);
    return false;
}

void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang='en'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>ESP32</title>
</head>
<body>
    <h2>ESP32</h2>
    <form id="connForm">
        <label for="ssid">SSID:</label>
        <input type="text" name="ssid" id="ssid" placeholder="Network name" required>
        <input type="text" name="password" id="password" placeholder="Network password (if any)">
        <button type="submit">Connect</button>
        <button type="button" id="disconnect">Disconnect</button>
    </form>
    <p>Connection SSID/Password: <strong id="connSSID"></strong> / <strong id="connPassword"></strong></p>
    <p>Connection Status: <strong id="connStatus"></strong></p>
    <p>Connection IP: <strong id="connIP"></strong></p>
    <hr>
    <h2>ESP32-CAM</h2>
    <p>ESP32-CAM SSID/Password: <strong id="camSSID"></strong> / <strong id="camPassword"></strong></p>
    <p>ESP32-CAM Status: <strong id="camStatus"></strong></p>
    <p>ESP32-CAM IP: <strong id="camIP"></strong></p>
    <iframe id="camFeed" width="640" height="480" src=""></iframe>

    <script>
        function formatLogMessage(level, processTime, message) {
            const timestamp = new Date().toISOString();
            return `${timestamp} ${level} ${processTime} - ${message}`;
        }

        function formatProcessTime(startTime, endTime) {
            const time = endTime - startTime;
            const hours = Math.floor(time / 3600000)
            const minutes = Math.floor((time % 3600000) / 60000);
            const seconds = Math.floor((time % 60000) / 1000);
            const milliseconds = Math.floor(time % 1000);
            const processTime = `${hours ? hours + 'h ' : ''}${minutes ? minutes + 'm ' : ''}${seconds ? seconds + 's ' : ''}${milliseconds}ms`;
            return processTime;
        }

        async function fetchGet(url) {
            const startTime = performance.now();
            let endTime;
            try {
                const response = await fetch(url);
                const responseText = await response.text();
                let data;
                
                try {
                    data = JSON.parse(responseText);
                } catch (error) {
                    data = responseText;
                }
                endTime = performance.now();

                if (response.ok) return data;
                throw data;
            } catch (error) {
                endTime = performance.now();

                if (error instanceof Error) console.error(`${formatLogMessage('ERROR', formatProcessTime(startTime, endTime), error.message)}`);
                else console.error(`${formatLogMessage(error.level, formatProcessTime(startTime, endTime), error.message)}`);
                throw error;
            }
        }

        async function fetchPost(url, formData) {
            const startTime = performance.now();
            let endTime;
            try {
                const response = await fetch(url, {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();
                endTime = performance.now();

                if (response.ok) return data;
                throw data;
            } catch (error) {
                endTime = performance.now();

                if (error instanceof Error) console.error(`${formatLogMessage('ERROR', formatProcessTime(startTime, endTime), error.message)}`);
                else console.error(`${formatLogMessage(error.level, formatProcessTime(startTime, endTime), error.message)}`);
                throw error;
            }
        }

        async function connect(event) {
            event.preventDefault();
            const formData = new FormData(document.getElementById('connForm'));
            const ssid = formData.get('ssid');
            let password = formData.get('password');
            if (!ssid) return alert('SSID is required');
            if (!password) password = '';

            try {
                const data = await fetchPost('/connect', formData);
                document.getElementById('connSSID').textContent = data.data.connSSID;
                document.getElementById('connPassword').textContent = data.data.connPassword;
                document.getElementById('connStatus').textContent = data.data.connStatus;
                document.getElementById('connIP').textContent = data.data.connIP;
                alert(data.message);
            } catch (error) {
                alert(error.message);
            }
        }

        async function disconnect() {
            try {
                const data = await fetchGet('/disconnect');
                document.getElementById('connSSID').textContent = data.data.connSSID;
                document.getElementById('connPassword').textContent = data.data.connPassword;
                document.getElementById('connStatus').textContent = data.data.connStatus;
                document.getElementById('connIP').textContent = data.data.connIP;
                alert(data.message);
            } catch (error) {
                alert(error.message);
            }
        }

        async function fetchConnStatus() {
            try {
                const data = await fetchGet('/conn/status');
                document.getElementById('connSSID').textContent = data.data.connSSID;
                document.getElementById('connPassword').textContent = data.data.connPassword;
                document.getElementById('connStatus').textContent = data.data.connStatus;
                document.getElementById('connIP').textContent = data.data.connIP;
            } catch (error) {
                document.getElementById('connSSID').textContent = '';
                document.getElementById('connPassword').textContent = '';
                document.getElementById('connStatus').textContent = 'Not connected';
                document.getElementById('connIP').textContent = '0.0.0.0';
            }
        }

        async function fetchCamStatus() {
            try {
                const data = await fetchGet('/cam/status');
                document.getElementById('camStatus').textContent = data.data.camStatus;
            } catch (error) {
                document.getElementById('camStatus').textContent = 'Offline';
            }

            try {
                const data = await fetchGet('/cam/ip');
                const url = `http://${data.data.camIP}/`;
                const camFeedSrc = document.getElementById('camFeed').src;
                if (camFeedSrc !== url) {
                    document.getElementById('camIP').textContent = data.data.camIP;
                    document.getElementById('camFeed').src = url;
                }
            } catch (error) {
                document.getElementById('camIP').textContent = '0.0.0.0';
                document.getElementById('camFeed').src = '';
            }
        }

        async function fetchCamSSIDPassword() {
            try {
                const data = await fetchGet('/ssid');
                document.getElementById('camSSID').textContent = data;
            } catch (error) {
                document.getElementById('camSSID').textContent = '';
            }

            try {
                const data = await fetchGet('/password');
                document.getElementById('camPassword').textContent = data;
            } catch (error) {
                document.getElementById('camPassword').textContent = '';
            }
        }

        async function updateStatus() {
            try {
                await fetchConnStatus();
                await fetchCamStatus();
                await fetchCamSSIDPassword();
            } catch (error) {
                console.error('Error updating status:', error);
            }
            setTimeout(updateStatus, 2000);
        }

        function eventListeners() {
            document.getElementById('connForm').addEventListener('submit', connect);
            document.getElementById('disconnect').addEventListener('click', disconnect);
        }

        document.addEventListener('DOMContentLoaded', () => {
            eventListeners();
            updateStatus();
        });
    </script>
</body>
</html>
    )rawliteral";

    server.send(200, "text/html", html);
}

String getStatus200(String message, String data) {
    String response = R"rawliteral({
        "status": 200,
        "message": ")rawliteral" + message + R"rawliteral(",
        "level": "INFO",
        "data": {)rawliteral" + data + R"rawliteral(}
    })rawliteral";
    return response;
}

String getStatus400(String message) {
    String response = R"rawliteral({
        "status": 400,
        "error": "Bad Request",
        "message": ")rawliteral" + message + R"rawliteral(",
        "level": "WARNING"
    })rawliteral";
    return response;
}

String getStatus500(String message) {
    String response = R"rawliteral({
        "status": 500,
        "error": "Internal Server Error",
        "message": ")rawliteral" + message + R"rawliteral(",
        "level": "ERROR"
    })rawliteral";
    return response;
}

void handleConn() {
    if (!server.hasArg("ssid")) {
        server.send(400, "application/json", getStatus400("SSID is required"));
        return;
    }

    staSSID = server.arg("ssid");
    staPASSWORD = server.arg("password");

    Serial.printf("\nConnecting to %s with password %s\n", staSSID.c_str(), staPASSWORD.c_str());

    String response;
    if (connect(staSSID.c_str(), staPASSWORD.c_str())) {
        connStatus = "Connected";
        connIP = WiFi.localIP();
        Serial.printf("Connected to %s with IP %s\n", staSSID.c_str(), connIP.toString().c_str());

        String data = R"rawliteral(
            "connStatus": ")rawliteral" + connStatus + R"rawliteral(",
            "connIP": ")rawliteral" + connIP.toString() + R"rawliteral(",
            "connSSID": ")rawliteral" + WiFi.SSID() + R"rawliteral(",
            "connPassword": ")rawliteral" + WiFi.psk() + R"rawliteral("
        )rawliteral";

        response = getStatus200("Connected to network", data);
        server.send(200, "application/json", response);
    } else {
        response = getStatus500("Failed to connect to network");
        server.send(500, "application/json", response);
    }
}

void handleDisconnect() {
    WiFi.disconnect(true);
    Serial.printf("\nDisconnected from %s\n", staSSID.c_str());

    staSSID = WiFi.SSID();
    staPASSWORD = WiFi.psk();
    connStatus = "Not connected";
    connIP = IPAddress();

    String data = R"rawliteral(
        "connStatus": ")rawliteral" + connStatus + R"rawliteral(",
        "connIP": "",
        "connSSID": ")rawliteral" + staSSID + R"rawliteral(",
        "connPassword": ")rawliteral" + staPASSWORD + R"rawliteral("
    )rawliteral";

    String response = getStatus200("Disconnected from network", data);
    server.send(200, "application/json", response);
}

void handleSSID() {
    server.send(200, "text/plain", staSSID);
}

void handlePassword() {
    server.send(200, "text/plain", staPASSWORD);
}

void handleIP() {
    server.send(200, "text/plain", connIP.toString());
}

void handleConnStatus() {
    String data = R"rawliteral(
        "connStatus": ")rawliteral" + connStatus + R"rawliteral(",
        "connIP": ")rawliteral" + connIP.toString() + R"rawliteral(",
        "connSSID": ")rawliteral" + staSSID + R"rawliteral(",
        "connPassword": ")rawliteral" + staPASSWORD + R"rawliteral("
    )rawliteral";

    String response = getStatus200("Connection status", data);
    server.send(200, "application/json", response);
}

void handleCamStatus() {
    if (server.hasArg("status")) camStatus = server.arg("status");

    String data = R"rawliteral(
        "camStatus": ")rawliteral" + camStatus + R"rawliteral("
    )rawliteral";

    String response = getStatus200("Camera status", data);
    server.send(200, "application/json", response);
}

void handleCamIP() {
    if (server.hasArg("ip")) camIP.fromString(server.arg("ip"));

    String data = R"rawliteral(
        "camIP": ")rawliteral" + camIP.toString() + R"rawliteral("
    )rawliteral";

    String response = getStatus200("Camera IP", data);
    server.send(200, "application/json", response);
}

// the esp32 board will receive the request from the client and open the gate,
// once the gate is open, it has 2 seconds to close it. Now, this is not stackable
// i.e its not allowed to open the gate again while it is still open (this is handled
// by a boolean variable that doesn't allow the gate to be opened while it is open).

// Same goes for the valve, boolean variable, a 3 seconds delay to close and is stackable
// i.e it only closes when the valve timer is up (adds 3 seconds to the current time for
// each bottle sent to the client).

// now the order of operation here is that;
// 1. check if theres bottle in the gate (client side dont do anything here)
// 2. client sends a request to open the gate (POST /opengate=true)
// 3. esp32 receives the request and either
//    a. open the servo motor if it is closed and close it after 2 seconds
//    b. ignore the request if the gate is open
// 4. after receiving the request, in open scenario, a function adds 3 seconds to the
//    current time of the valve.


void handleOpenGate() {
    if (isGateOpen) {
        server.send(403, "application/json", getStatus400("Gate is already open"));
        return;
    }

    isGateOpen = true;
    digitalWrite(gatePin, HIGH);
    gateCloseTimer = millis() + 2000;

    isValveOpen = true;
    valveCloseTimer = max(millis(), valveCloseTimer) + 3000;

    server.send(200, "application/json", getStatus200("Gate opened", ""));
}