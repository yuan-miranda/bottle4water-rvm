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
        <script>
            async function connect(event) {
                event.preventDefault();
                let ssid = document.querySelector("input[name='ssid']").value;
                let password = document.querySelector("input[name='password']").value;
                if (!ssid || !password) return alert('SSID and Password are required');

                let formData = new FormData(document.getElementById('networkForm'));

                try {
                    const response = await fetch('/connect', {
                        method: 'POST',
                        body: formData
                    });

                    if (response.ok) {
                        const data = await response.text();

                        if (data === 'Failed to connect to network') {
                            document.getElementById('connIP').textContent = 'Connection IP: ';
                            document.getElementById('connStatus').textContent = `Connection Status: ${data}`;
                            alert(data);
                            return;
                        }

                        document.getElementById('connIP').textContent = 'Connection IP: ' + data;
                        document.getElementById('connStatus').textContent = 'Connection Status: Connected';
                        alert(data);
                    } else {
                        alert(response.statusText);
                    }
                } catch (error) {
                    console.error('Error connecting to network:', error);
                    alert('Request failed');
                }
            }

            async function checkStatus() {
                try {
                    const response = await fetch('/conn_status');
                    if (response.ok) {
                        const data = await response.text();
                        document.getElementById('connStatus').textContent = 'Connection Status: ' + data;
                        if (data === 'Not connected') document.getElementById('connIP').textContent = 'Connection IP: ';
                    } else {
                        document.getElementById('connStatus').textContent = 'Connection Status: Error';
                    }
                } catch (error) {
                    console.error('Error fetching connection status:', error);
                    document.getElementById('connStatus').textContent = 'Connection Status: Error';
                    document.getElementById('connIP').textContent = 'Connection IP: ';
                }
            }

            async function checkCamStatus() {
                try {
                    const response = await fetch('/cam_status');
                    if (response.ok) {
                        const data = await response.text();
                        document.getElementById('camStatus').textContent = 'ESP32-CAM Status: ' + data;
                    } else {
                        document.getElementById('camStatus').textContent = 'ESP32-CAM Status: Error';
                    }
                } catch (error) {
                    console.error('Error fetching cam status:', error);
                    document.getElementById('camStatus').textContent = 'ESP32-CAM Status: Error';
                }

                try {
                    const response = await fetch('/cam_ip');
                    if (response.ok) {
                        const data = await response.text();
                        const url = `http://${data}/`;
                        const camFeedSrc = document.getElementById('camFeed').src;
                        if (camFeedSrc !== url) {
                            document.getElementById('camFeed').src = `http://${data}`;
                            document.getElementById('camIp').textContent = 'ESP32-CAM IP: ' + data;
                        }
                    } else {
                        document.getElementById('camIp').textContent = 'ESP32-CAM IP: Error';
                        document.getElementById('camFeed').src = '';
                    }
                } catch (error) {
                    console.error('Error fetching cam IP:', error);
                    document.getElementById('camIp').textContent = 'ESP32-CAM IP: Error';
                    document.getElementById('camFeed').src = '';
                }
            }

            async function intervalCheck() {
                try {
                    await Promise.all([checkStatus(), checkCamStatus()]);
                } catch (error) {
                    console.error("Error checking status:", error);
                }
                setTimeout(intervalCheck, 1000);
            }

            document.addEventListener('DOMContentLoaded', () => {
                intervalCheck();
            });
        </script>
    </head>
    <body>
        <form id='networkForm' onsubmit='connect(event)'>
            SSID: <input type='text' name='ssid'><br>
            Password: <input type='text' name='password'><br>
            <input type='submit' value='Connect'>
        </form>
        <p id='connIP'>Connection IP: )rawliteral" + (localIP.toString() == "0.0.0.0" ? "" : localIP.toString()) + R"rawliteral(</p>
        <p id='connStatus'>Connection Status: </p>
        <hr>
        <p id='camStatus'>ESP32-CAM Status: )rawliteral" + camStatus + R"rawliteral(</p>
        <p id='camIp'>ESP32-CAM IP: )rawliteral" + camIp + R"rawliteral(</p>
        <iframe id='camFeed' width='640' height='480' src='http://)rawliteral" + camIp + R"rawliteral('></iframe>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", html);
}

void handleConn() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");

        Serial.printf("\nConnecting to %s with password %s\n", ssid.c_str(), password.c_str());
        
        if (connect(ssid.c_str(), password.c_str())) {
            localIP = WiFi.localIP();
            server.send(200, "text/plain", localIP.toString());
        } else {
            server.send(200, "text/plain", "Failed to connect to network");
        }
    } else {
        server.send(400, "text/plain", "Missing SSID or password");
    }
}

void handleConnStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        server.send(200, "text/plain", "Connected");
    } else {
        server.send(200, "text/plain", "Not connected");
    }
}

void handleCamStatus() {
    if (server.hasArg("status")) {
        camStatus = server.arg("status");
        server.send(200, "text/plain", "Camera status updated");
    } else {
        server.send(200, "text/plain", camStatus);
    }
}

void handleCamIP() {
    if (server.hasArg("ip")) {
        camIp = server.arg("ip");
        server.send(200, "text/plain", "Camera IP updated");
    } else server.send(200, "text/plain", camIp);
}