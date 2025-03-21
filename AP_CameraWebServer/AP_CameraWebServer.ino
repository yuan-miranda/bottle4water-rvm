#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char* AP_SSID = "ESP32-AP";
const char* AP_PASSWORD = "12345678";
const char* AP_LOCAL_IP = "http://192.168.4.1";

String staSSID;
String staPassword;
IPAddress staIP = IPAddress();

void startCameraServer();
void setupLedFlash(int pin);

void setup() {
    Serial.begin(115200);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (config.pixel_format == PIXFORMAT_JPEG) {
        if (psramFound()) {
            config.jpeg_quality = 10;
            config.fb_count = 2;
            config.grab_mode = CAMERA_GRAB_LATEST;
        } else {
            config.frame_size = FRAMESIZE_SVGA;
            config.fb_location = CAMERA_FB_IN_DRAM;
        }
    } else {
        config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
        config.fb_count = 2;
#endif
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }

    if (config.pixel_format == PIXFORMAT_JPEG) s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(LED_GPIO_NUM)
    setupLedFlash(LED_GPIO_NUM);
#endif

    if (connect(AP_SSID, AP_PASSWORD)) Serial.printf("Connected to %s with IP %s\n", AP_SSID, WiFi.localIP().toString().c_str());
    else {
        Serial.printf("Failed to connect to %s\n", AP_SSID);
        return;
    }

    startCameraServer();
    Serial.printf("AP_CameraWebServer started on http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
    if (staIP != IPAddress() && !pingSTA()) WiFi.disconnect(true);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Disconnected from the network\n");
        staSSID = "";
        staPassword = "";
        staIP = IPAddress();

        if (connect(AP_SSID, AP_PASSWORD)) Serial.printf("Reconnected to %s with IP %s\n", AP_SSID, WiFi.localIP().toString().c_str());
        else Serial.printf("Failed to reconnect to %s\n", AP_SSID);
    }
    if (WiFi.status() == WL_CONNECTED) connectSTA();

    sendStatus();
    sendIp();
}

bool pingSTA() {
    HTTPClient http;
    http.begin("http://" + staIP.toString() + "/");
    int httpCode = http.GET();
    http.end();
    return httpCode == 200;
}

bool connect(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    unsigned long startTime = millis();
    while (millis() - startTime < 8000) {
        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }
        delay(500);
    }

    WiFi.disconnect(true);
    return false;
}

void connectSTA() {
    getSTAIP();
    getSTASSID();
    if (staSSID.length() == 0 || staSSID == WiFi.SSID()) return;

    getSTAPassword();

    WiFi.disconnect();
    if (connect(staSSID.c_str(), staPassword.c_str())) Serial.printf("Connected to %s with IP %s\n", staSSID.c_str(), WiFi.localIP().toString().c_str());
    else Serial.printf("Failed to connect to %s\n", staSSID.c_str());
}

void getSTASSID() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;

    if (WiFi.SSID() == AP_SSID) http.begin(String(AP_LOCAL_IP) + "/ssid");
    else http.begin("http://" + staIP.toString() + "/ssid");

    http.addHeader("Content-Type", "text/plain");

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) staSSID = http.getString();
    http.end();
}

void getSTAPassword() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;

    if (WiFi.SSID() == AP_SSID) http.begin(String(AP_LOCAL_IP) + "/password");
    else http.begin("http://" + staIP.toString() + "/password");

    http.addHeader("Content-Type", "text/plain");

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) staPassword = http.getString();
    http.end();
}

void getSTAIP() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(String(AP_LOCAL_IP) + "/ip");
    http.addHeader("Content-Type", "text/plain");

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) staIP.fromString(http.getString());
    http.end();
}

void sendStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        if (WiFi.SSID() == AP_SSID) http.begin(String(AP_LOCAL_IP) + "/cam_status");
        else http.begin("http://" + staIP.toString() + "/cam_status");

        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        int httpResponseCode = http.POST("status=Online");        
        http.end();
    }
}

void sendIp() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        if (WiFi.SSID() == AP_SSID) http.begin(String(AP_LOCAL_IP) + "/cam_ip");
        else http.begin("http://" + staIP.toString() + "/cam_ip");
        
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        int httpResponseCode = http.POST("ip=" + WiFi.localIP().toString());        
        http.end();
    }
}
