#include "esp_log.h"

#include "candy.h"
#include "firebase_sesame/firebase_ssm_cmd.h"
#include "sesame/ssm.h"
#include "sesame/ssm_cmd.h"

static char *TAG = "ssm_task";

// 現在のsesameの状態とfirebaseの状態に差がないかを確認するタスク
static void task_ssm_status_monitoring(void *pvParameters) {
  firebase_ssm_status_t firebase_ssm_status;
  firebase_auth_info_t *auth_info = (firebase_auth_info_t *)pvParameters;

  while (1) {
    firebase_ssm_get_current_status(auth_info, &firebase_ssm_status);

    if (p_ssms_env->ssm.device_status == SSM_LOCKED &&
        firebase_ssm_status == SSM_STATUS_UNLOCKED) {
      firebase_ssm_update_current_status(auth_info, SSM_STATUS_LOCKED);
    } else if (p_ssms_env->ssm.device_status == SSM_UNLOCKED &&
               firebase_ssm_status == SSM_STATUS_LOCKED) {
      firebase_ssm_update_current_status(auth_info, SSM_UNLOCKED);
    }
    vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒待機
  }
}

// 現在のコマンドを1秒間隔で取得するタスク
static void task_sesame_get_command(void *pvParameters) {
  esp_err_t status;
  firebase_auth_info_t *auth_info = (firebase_auth_info_t *)pvParameters;

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

void start_sesame_tasks(void *auth_info) {
  xTaskCreate(task_sesame_get_command, "sesame command get task", 8192,
              auth_info, 5, NULL);
  xTaskCreate(task_ssm_status_monitoring, "sesame status monitoring task", 8192,
              auth_info, 10, NULL);
}
