#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "blecent.h"
#include "firebase/firebase_auth.h"
#include "firebase/firebase_config.h"
#include "firebase/firebase_internal.h"
#include "firebase_sesame/firebase_ssm_cmd.h"
#include "ssm_cmd.h"
#include "wifi.h"
#include <stdbool.h>

// マクロ定義
#define TAG "main.c"

// Global変数
firebase_auth_info_t *auth_info;

// 現在のコマンドを1秒間隔で取得するタスク
void task_sesame_get_command(void *pvParameters) {
  esp_err_t status;

  while (1) {
    firebase_ssm_cmd_t cmd = {0};
    status = firebase_ssm_get_commands(auth_info, &cmd);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "firebase_ssm_command_get_current failed: %s",
               esp_err_to_name(status));
      continue;
    }
    ESP_LOGI(TAG, "firebase_ssm_command_get_current_succeeded.");
    ESP_LOGI(TAG,
             "Command: type=%d, user_name=%s, is_finished=%d, "
             "is_success=%d",
             cmd.cmd_type, cmd.user_name, cmd.is_finished, cmd.is_success);

    if (cmd.is_finished == 0 && cmd.cmd_type != SSM_CMD_NONE) {
      // ここで実際のsesameを操作
      if (cmd.cmd_type == SSM_CMD_LOCK) {
        ssm_lock(NULL, 0);
      } else if (cmd.cmd_type == SSM_CMD_UNLOCK) {
        ssm_unlock(NULL, 0);
      }

      // 成功後にはfirebaseをupdateする。
      cmd.is_finished = true;
      cmd.is_success = true;
      firebase_ssm_update_status(auth_info, &cmd);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒待機
  }
}

static void ssm_action_handle(sesame *ssm) {
  ESP_LOGI(TAG, "[ssm_action_handle][ssm status: %s]",
           SSM_STATUS_STR(ssm->device_status));

  if (ssm->device_status == SSM_UNLOCKED) {
    firebase_ssm_update_current_status(auth_info, SSM_STATUS_UNLOCKED);
  } else if (ssm->device_status == SSM_LOCKED) {
    firebase_ssm_update_current_status(auth_info, SSM_STATUS_LOCKED);
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

  vTaskDelay(pdMS_TO_TICKS(10000));

  // init firebase_auth_info_t
  auth_info = firebase_setup_auth(FIREBASE_EMAIL, FIREBASE_PASSWORD,
                                  FIREBASE_API_KEY, FIREBASE_DB_URL_BASE, 5);

  if (!auth_info) {
    ESP_LOGE(TAG, "firebase auth setup failed");
  }

  xTaskCreate(task_sesame_get_command, "sesame command get task", 8192, NULL, 5,
              NULL);
}
