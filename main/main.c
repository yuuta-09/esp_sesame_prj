#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"

#include "blecent.h"
#include "firebase/firebase_auth.h"
#include "firebase/firebase_config.h"
#include "firebase/firebase_database.h"
#include "firebase_sesame/firebase_ssm_cmd.h"
#include "sesame/ssm_tasks.h"
#include "wifi.h"

// マクロ定義
#define TAG "main.c"

// Global変数
static firebase_auth_info_t *auth_info;
static bool ssm_connected = false;

static void ssm_action_handle(sesame *ssm) {
  ESP_LOGI(TAG, "[ssm_action_handle][ssm status: %s]",
           SSM_STATUS_STR(ssm->device_status));

  if (ssm->device_status == SSM_UNLOCKED) {
    firebase_ssm_update_current_status(auth_info, SSM_STATUS_UNLOCKED);
  } else if (ssm->device_status == SSM_LOCKED) {
    firebase_ssm_update_current_status(auth_info, SSM_STATUS_LOCKED);
  } else if (ssm->device_status == SSM_LOGGIN) {
    ssm_connected = true;
  }
}

void app_main(void) {
  // NVSの初期化
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // wifiの初期化
  wifi_init();

  // sesame, espの初期化
  ssm_init(ssm_action_handle);
  esp_ble_init();

  // ssmが接続するまで待機
  while (!ssm_connected) {
    ESP_LOGI(TAG, "Waiting for ssm connecting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // mutexの初期化
  firebase_https_mutex = xSemaphoreCreateMutex();

  // init firebase_auth_info_t
  auth_info = firebase_setup_auth(FIREBASE_EMAIL, FIREBASE_PASSWORD,
                                  FIREBASE_API_KEY, FIREBASE_DB_URL_BASE, 5);

  if (!auth_info) {
    ESP_LOGE(TAG, "firebase auth setup failed");
  }

  start_sesame_tasks(auth_info);
}
