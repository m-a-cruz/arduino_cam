#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

const char* ssid = "Infinix SMART 9";
const char* password = "12345678";
const char* serverUrl = "http://192.168.124.80:5000/api/camera/upload";  // Use your actual server IP and HTTP (not https!)

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#define FLASH_LED_PIN 4  // Built-in flash

const char* camera_name = "CAM-001";
const char* location = "CSS Office Entrance";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");

  // Setup flash
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true); // halt
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(200); // flash on briefly

    camera_fb_t *fb = esp_camera_fb_get();
    digitalWrite(FLASH_LED_PIN, LOW); // turn flash off

    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverUrl);

    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    String contentType = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentType);

    String bodyStart =
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"camera_name\"\r\n\r\n" + String(camera_name) + "\r\n" +
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"location\"\r\n\r\n" + String(location) + "\r\n" +
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"image\"; filename=\"image.jpg\"\r\n"
      "Content-Type: image/jpeg\r\n\r\n";

    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    int totalLength = bodyStart.length() + fb->len + bodyEnd.length();

    // Allocate buffer
    uint8_t* fullData = (uint8_t*)malloc(totalLength);
    if (!fullData) {
      Serial.println("Failed to allocate memory for HTTP body");
      esp_camera_fb_return(fb);
      return;
    }

    memcpy(fullData, bodyStart.c_str(), bodyStart.length());
    memcpy(fullData + bodyStart.length(), fb->buf, fb->len);
    memcpy(fullData + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());

    http.addHeader("Content-Length", String(totalLength));

    int httpCode = http.POST(fullData, totalLength);
    if (httpCode > 0) {
      Serial.printf("Upload done. HTTP Code: %d\n", httpCode);
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.printf("HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
    }

    free(fullData);
    http.end();
    esp_camera_fb_return(fb);
  } else {
    Serial.println("WiFi not connected");
  }

  delay(60000); // 1 minute interval
}
