#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// Default IP: http://192.168.4.1/

const char *AP_SSID = "ESP32-AP";
const char *AP_PASSWORD = "12345678";
IPAddress localIP;
String camStatus = "Offline";
String camIp = "";
int failedCamConnAttempts = 0;
WebServer server(80);

unsigned long lastHeartbeat = 0;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    // root page
    server.on("/", HTTP_GET, handleRoot);
    
    // connect to the given SSID and password
    server.on("/connect", HTTP_POST, handleConn);

    // check connection status
    server.on("/conn_status", HTTP_GET, handleConnStatus);

    // check ESP32-CAM status
    server.on("/cam_status", HTTP_POST, handleCamStatus);
    server.on("/cam_status", HTTP_GET, handleCamStatus);

    // check ESP32-CAM ip
    server.on("/cam_ip", HTTP_POST, handleCamIP);
    server.on("/cam_ip", HTTP_GET, handleCamIP);

    server.begin();
}

void loop() {
    server.handleClient();
    if (millis() - lastHeartbeat > 3000) {
        lastHeartbeat = millis();
        checkCamTimeout();
    }
}

void checkCamTimeout() {
    bool isCamOnline = pingCam();

    if (!isCamOnline) {
        failedCamConnAttempts++;
        if (failedCamConnAttempts >= 3) {
            Serial.println("ESP32-CAM is offline");
            camStatus = "Offline";
            camIp = "";
            failedCamConnAttempts = 0;
        }
    } else failedCamConnAttempts = 0;
}

bool pingCam() {
    HTTPClient http;
    http.begin("http://" + camIp + "/");
    int httpCode = http.GET();
    http.end();
    return httpCode == 200;
}

bool connect(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    while (millis() - startTime < 8000) {
        if (WiFi.status() == WL_CONNECTED) {
            localIP = WiFi.localIP();
            return true;
        }
        delay(500);
        Serial.print(".");
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
        <form id="connForm">
            <label for="ssid">SSID:</label>
            <input type="text" name="ssid" id="ssid" placeholder="Network name" required>
            <input type="text" name="password" id="password" placeholder="Network password (if any)">
            <button type="submit">Connect</button>
        </form>
        <p>Connection IP: <span id="connIP"></span></p>
        <p>Connection Status: <span id="connStatus"></span></p>
        <hr>
        <p>ESP32-CAM Status: <span id="camStatus"></span></p>
        <p>ESP32-CAM IP: <span id="camIp"></span></p>
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
                    document.getElementById('connStatus').textContent = data.data.connStatus;
                    document.getElementById('connIP').textContent = data.data.connIP;
                    alert(data.message);
                } catch (error) {
                    alert(error.message);
                }
            }

            async function fetchConnStatus() {
                try {
                    const data = await fetchGet('/conn_status');
                    document.getElementById('connStatus').textContent = data.data.connStatus;
                    document.getElementById('connIP').textContent = data.data.connIP;
                } catch (error) {
                    document.getElementById('connStatus').textContent = 'Error';
                    document.getElementById('connIP').textContent = '';
                }
            }

            async function fetchCamStatus() {
                try {
                    const data = await fetchGet('/cam_status');
                    document.getElementById('camStatus').textContent = data.data.camStatus;
                } catch (error) {
                    document.getElementById('camStatus').textContent = 'Error';
                }

                try {
                    const data = await fetchGet('/cam_ip');
                    const url = `http://${data.data.camIP}/`;
                    const camFeedSrc = document.getElementById('camFeed').src;
                    if (camFeedSrc !== url) {
                        document.getElementById('camIp').textContent = data.data.camIP;
                        document.getElementById('camFeed').src = url;
                    }
                } catch (error) {
                    document.getElementById('camIp').textContent = 'Error';
                    document.getElementById('camFeed').src = '';
                }
            }

            async function updateStatus() {
                try {
                    await Promise.all([fetchConnStatus(), fetchCamStatus()]);
                } catch (error) {
                    console.error('Error updating status:', error);
                }
                setTimeout(updateStatus, 1000);
            }

            function eventListeners() {
                document.getElementById('connForm').addEventListener('submit', connect);
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
    String response;
    if (server.hasArg("ssid")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");

        Serial.printf("\nConnecting to %s with password %s\n", ssid.c_str(), password.c_str());
        
        if (connect(ssid.c_str(), password.c_str())) {
            String ip = localIP.toString();
            String data = R"rawliteral(
                "connStatus": "Connected",
                "connIP": ")rawliteral" + ip + R"rawliteral("
            )rawliteral";
            response = getStatus200("Connected to network", data);
            server.send(200, "application/json", response);
        } else {
            response = getStatus500("Failed to connect to network");
            server.send(500, "application/json", response);
        }
    } else {
        response = getStatus400("Missing SSID");
        server.send(400, "application/json", response);
    }
}

void handleConnStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        String ip = localIP.toString();
        String data = R"rawliteral(
            "connStatus": "Connected",
            "connIP": ")rawliteral" + ip + R"rawliteral("
        )rawliteral";
        String response = getStatus200("Connected to network", data);
        server.send(200, "application/json", response);
    } else {
        String response = getStatus200("Not connected to network", "");
        server.send(200, "application/json", response);
    }
}

void handleCamStatus() {
    String response;
    if (server.hasArg("status")) {
        camStatus = server.arg("status");
        String data = R"rawliteral(
            "camStatus": ")rawliteral" + camStatus + R"rawliteral("
        )rawliteral";
        response = getStatus200("Camera status updated", data);
        server.send(200, "application/json", response);
    } else {
        String data = R"rawliteral(
            "camStatus": ")rawliteral" + camStatus + R"rawliteral("
        )rawliteral";
        response = getStatus200("Camera status", data);
        server.send(200, "application/json", response);
    }
}

void handleCamIP() {
    String response;
    if (server.hasArg("ip")) {
        camIp = server.arg("ip");
        String data = R"rawliteral(
            "camIP": ")rawliteral" + camIp + R"rawliteral("
        )rawliteral";
        response = getStatus200("Camera IP updated", data);
        server.send(200, "application/json", response);
    } else {
        String data = R"rawliteral(
            "camIP": ")rawliteral" + camIp + R"rawliteral("
        )rawliteral";
        response = getStatus200("Camera IP", data);
        server.send(200, "application/json", response);
    }
}