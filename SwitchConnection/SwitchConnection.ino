#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

#define SERVO_PIN_1 26
#define SERVO_PIN_2 27

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

WebServer server(80);

// gate servo motor
Servo servoMotor1;
// valve servo motor
Servo servoMotor2;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    servoMotor1.attach(SERVO_PIN_1);
    servoMotor2.attach(SERVO_PIN_2);

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
        servoMotor1.write(0);
        isGateOpen = false;
    }
}

void checkValve() {
    if (isValveOpen && millis() >= valveCloseTimer) {
        servoMotor2.write(0);
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

void handleOpenGate() {
    if (isGateOpen) {
        server.send(403, "application/json", getStatus400("Gate is already open"));
        return;
    }

    isGateOpen = true;
    servoMotor1.write(90);
    
    delay(100);
    servoMotor2.write(90);
    gateCloseTimer = millis() + 2000;

    isValveOpen = true;
    valveCloseTimer = max(millis(), valveCloseTimer) + 10000;

    server.send(200, "application/json", getStatus200("Gate opened", ""));
}