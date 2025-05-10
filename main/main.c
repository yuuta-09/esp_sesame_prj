#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdbool.h>

#include "blecent.h"
#include "candy.h"
#include "firebase/firebase_auth.h"
#include "firebase/firebase_config.h"
#include "firebase/firebase_internal.h"
#include "firebase_sesame/firebase_ssm_cmd.h"
#include "ssm.h"
#include "ssm_cmd.h"
#include "wifi.h"

// マクロ定義
#define TAG "main.c"

// Global変数
static firebase_auth_info_t *auth_info;
static bool ssm_connected = false;

// 現在のsesameの状態とfirebaseの状態に差がないかを確認するタスク
void task_ssm_status_monitoring(void *pvParameters) {
  firebase_ssm_status_t firebase_ssm_status;

  while (1) {
    firebase_ssm_get_current_status(auth_info, &firebase_ssm_status);
    if (firebase_ssm_status == SSM_STATUS_LOCKED) {
      ESP_LOGI(TAG, "now firebase ssm is locked");
    } else if (firebase_ssm_status == SSM_STATUS_UNLOCKED) {
      ESP_LOGI(TAG, "now firebase ssm is unlocked");
    }

    if (p_ssms_env->ssm.device_status == SSM_LOCKED) {
      ESP_LOGI(TAG, "now ssm is locked");
    } else if (p_ssms_env->ssm.device_status == SSM_UNLOCKED) {
      ESP_LOGI(TAG, "now ssm is unlocked");
    }

    if (p_ssms_env->ssm.device_status == SSM_LOCKED &&
        firebase_ssm_status == SSM_STATUS_UNLOCKED) {
      firebase_ssm_update_current_status(auth_info, SSM_STATUS_LOCKED);
    } else if (p_ssms_env->ssm.device_status == SSM_UNLOCKED &&
               firebase_ssm_status == SSM_STATUS_LOCKED) {
      firebase_ssm_update_current_status(auth_info, SSM_UNLOCKED);
    }
    vTaskDelay(pdMS_TO_TICKS(100000)); // 10秒待機
  }
}

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

  // init firebase_auth_info_t
  auth_info = firebase_setup_auth(FIREBASE_EMAIL, FIREBASE_PASSWORD,
                                  FIREBASE_API_KEY, FIREBASE_DB_URL_BASE, 5);

  if (!auth_info) {
    ESP_LOGE(TAG, "firebase auth setup failed");
  }

  xTaskCreate(task_sesame_get_command, "sesame command get task", 8192, NULL, 5,
              NULL);
  xTaskCreate(task_ssm_status_monitoring, "sesame status monitoring task", 8192,
              NULL, 10, NULL);
}
