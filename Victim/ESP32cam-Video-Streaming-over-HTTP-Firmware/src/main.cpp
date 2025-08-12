#include <Arduino.h>

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// Wi-Fi credentials
const char *ssid = "SATL";
const char *password = "SATL4321";

// Select camera model
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

int res_num = 0;
framesize_t frameSizes[] = {
    FRAMESIZE_96X96,   // 96x96
    FRAMESIZE_QQVGA,   // 160x120
    FRAMESIZE_QCIF,    // 176x144
    FRAMESIZE_HQVGA,   // 240x176
    FRAMESIZE_240X240, // 240x240
    FRAMESIZE_QVGA,    // 320x240
    FRAMESIZE_CIF,     // 400x296
    FRAMESIZE_HVGA,    // 480x320
    FRAMESIZE_VGA,     // 640x480
    FRAMESIZE_SVGA,    // 800x600
    FRAMESIZE_XGA,     // 1024x768
    FRAMESIZE_HD,      // 1280x720
    FRAMESIZE_SXGA,    // 1280x1024
    FRAMESIZE_UXGA     // 1600x1200
};

// Setup the camera
void startCamera()
{
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
  config.pixel_format = PIXFORMAT_RGB565; // RAW565
  config.frame_size = FRAMESIZE_QQVGA;    // Small size for quick streaming
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12; // not used in RGB
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // Init camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed: 0x%x\n", err);
    while (true)
      ; // Halt
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_hmirror(s, 1);
  s->set_vflip(s, 0);
}

esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_RGB565) {
      httpd_resp_send_500(req);
      if (fb) esp_camera_fb_return(fb);
      return ESP_FAIL;
  }

  // Set response headers
  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=frame.raw");
  
  // Enable chunked encoding for large buffers
  if (fb->len > 100 * 1024) {  // >100KB
      httpd_resp_set_hdr(req, "Transfer-Encoding", "chunked");
  }

  esp_err_t result = ESP_OK;
  const size_t CHUNK_SIZE = 100*1024;  // 100KB chunks
  size_t remaining = fb->len;
  size_t offset = 0;

  // Send data in chunks
  while (remaining > 0 && result == ESP_OK) {
      size_t chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
      result = httpd_resp_send_chunk(req, (const char *)fb->buf + offset, chunk_size);
      
      remaining -= chunk_size;
      offset += chunk_size;
      
      // Optional: Add small delay to prevent watchdog triggers
      vTaskDelay(1);
  }

  // Send final chunk to indicate end of response
  if (result == ESP_OK) {
      result = httpd_resp_send_chunk(req, NULL, 0);
  }

  esp_camera_fb_return(fb);
  return result;
}

esp_err_t size_handler(httpd_req_t *req)
{
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // Create response string with image dimensions
  char resp[64];
  snprintf(resp, sizeof(resp), "Width: %d, Height: %d, Size: %d bytes",
           fb->width, fb->height, fb->len);

  esp_camera_fb_return(fb);

  // Send response
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, resp, strlen(resp));
}

// Start the HTTP server
void startServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &stream_uri);

    // New size endpoint
    httpd_uri_t size_uri = {
        .uri = "/size",
        .method = HTTP_GET,
        .handler = size_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &size_uri);

    Serial.println("✅ HTTP server started at /stream and /size");
  }
  else
  {
    Serial.println("❌ Failed to start HTTP server");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Booting...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  startCamera();
  startServer();
}

void loop()
{

  if (Serial.available() > 0)
  {
    String input = Serial.readStringUntil('\n');
    int frameIndex = input.toInt();
    esp_camera_deinit();

    if (frameIndex >= 0 && frameIndex < 13)
    {
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
      config.pixel_format = PIXFORMAT_RGB565;     // RAW565
      config.frame_size = frameSizes[frameIndex]; // Small size for quick streaming
      config.fb_location = CAMERA_FB_IN_PSRAM;
      config.jpeg_quality = 12; // not used in RGB
      config.fb_count = 1;
      config.grab_mode = CAMERA_GRAB_LATEST;
      sensor_t *s = esp_camera_sensor_get();

      esp_err_t err = esp_camera_init(&config);
      if (err != ESP_OK)
      {
        // Serial.printf("Camera init failed with error 0x%x", err);
        return;
      }

      // Stop current frame buffer
      esp_camera_return_all();

      // Force sensor to apply changes
      vTaskDelay(pdMS_TO_TICKS(100));

      // Get new frame to verify size
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb)
      {
        Serial.printf("New resolution set - Width: %d, Height: %d, Buffer Size: %d bytes\n",
                      fb->width, fb->height, fb->len);
        esp_camera_fb_return(fb);
      }
      else
      {
        Serial.println("Failed to get frame after resolution change");
      }
    }
    else
    {
      Serial.println("Invalid frame size index! Use 0-13");
    }

    // Clear serial buffer
    while (Serial.available() > 0)
    {
      Serial.read();
    }
  }
  delay(100);
}
