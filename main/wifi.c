// this is a file that contains the wifi functions
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;
static const char *TAG = "wifi";

// wifiイベントハンドラ
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init(void) {
    ESP_LOGI(TAG, "Start WiFi initialization...");

    // ネットワークインターフェースの初期化
    ESP_ERROR_CHECK(esp_netif_init());

    // デフォルトのイベントループを作成
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // デフォルトのWiFi stationインターフェースを作成
    esp_netif_create_default_wifi_sta();

    // WiFiイベントグループの作成
    wifi_event_group = xEventGroupCreate();

    // WiFiの初期化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // WiFi接続を待機
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected");
}