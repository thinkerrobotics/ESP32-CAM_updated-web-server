/*
* Capture ESP32 Cam JPEG images into a AVI file and store on SD
* AVI files stored on the SD card can also be selected and streamed to a browser as MJPEG.
*
* s60sc 2020 - 2023
*/

#include "appGlobals.h"

// camera board selected
#if defined(CAMERA_MODEL_AI_THINKER)
#define CAM_BOARD "CAMERA_MODEL_AI_THINKER"
#elif defined(CAMERA_MODEL_ESP32S3_EYE)
#define CAM_BOARD "CAMERA_MODEL_ESP32S3_EYE"
#elif defined(CAMERA_MODEL_XIAO_ESP32S3)
#define CAM_BOARD "CAMERA_MODEL_XIAO_ESP32S3"
#else
#define CAM_BOARD "OTHER"
#endif

char camModel[10];

static void prepCam() {
  // initialise camera depending on model and board
  // configure camera
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
  config.xclk_freq_hz = xclkMhz * 1000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  // init with high specs to pre-allocate larger buffers
  config.fb_location = CAMERA_FB_IN_PSRAM;
#if CONFIG_IDF_TARGET_ESP32S3
  config.frame_size = FRAMESIZE_QSXGA; // 8M
#else
  config.frame_size = FRAMESIZE_UXGA;  // 4M
#endif  
  config.jpeg_quality = 10;
  config.fb_count = 4;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  if (psramFound()) {
    esp_err_t err = ESP_FAIL;
    uint8_t retries = 2;
    while (retries && err != ESP_OK) {
      err = esp_camera_init(&config);
      if (err != ESP_OK) {
        // power cycle the camera, provided pin is connected
        digitalWrite(PWDN_GPIO_NUM, 1);
        delay(100);
        digitalWrite(PWDN_GPIO_NUM, 0); 
        delay(100);
        retries--;
      }
    } 
    if (err != ESP_OK) sprintf(startupFailure, "Startup Failure: Camera init error 0x%x", err);
    else {
      sensor_t * s = esp_camera_sensor_get();
      switch (s->id.PID) {
        case (OV2640_PID):
          strcpy(camModel, "OV2640");
        break;
        case (OV3660_PID):
          strcpy(camModel, "OV3660");
        break;
        case (OV5640_PID):
          strcpy(camModel, "OV5640");
        break;
        default:
          strcpy(camModel, "Other");
        break;
      }
      LOG_INF("Camera init OK for model %s on board %s", camModel, CAM_BOARD);

      // model specific corrections
      if (s->id.PID == OV3660_PID) {
        // initial sensors are flipped vertically and colors are a bit saturated
        s->set_vflip(s, 1);//flip it back
        s->set_brightness(s, 1);//up the brightness just a bit
        s->set_saturation(s, -2);//lower the saturation
      }
      //drop down frame size for higher initial frame rate
      s->set_framesize(s, FRAMESIZE_SVGA);
  
#if defined(CAMERA_MODEL_M5STACK_WIDE)
      s->set_vflip(s, 1);
      s->set_hmirror(s, 1);
#endif
  
#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif
  
#if defined(CAMERA_MODEL_ESP32S3_EYE)
    s->set_vflip(s, 1);
#endif
    }
  }
  debugMemory("prepCam");
}

void setup() {   
  logSetup();
  LOG_INF("=============== Starting ===============");
  if (!psramFound()) sprintf(startupFailure, "Startup Failure: Need PSRAM to be enabled");
  
  // prep SD card storage
  startStorage();

  // initialise camera
  prepCam();
  
  // Load saved user configuration
  loadConfig();

#ifdef DEV_ONLY
  devSetup();
#endif

  // connect wifi or start config AP if router details not available
  startWifi();
  
  startWebServer();
  if (strlen(startupFailure)) LOG_ERR("%s", startupFailure);
  else {
    // start rest of services
    startStreamServer();
    prepSMTP(); 
    prepPeripherals();
    prepMic(); 
    prepRecording();
    LOG_INF("Camera model %s on board %s ready @ %uMHz, version %s", camModel, CAM_BOARD, xclkMhz, APP_VER); 
    checkMemory();
  } 
}

void loop() {
  vTaskDelete(NULL); // free 8k ram
}
