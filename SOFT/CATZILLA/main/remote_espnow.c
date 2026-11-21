/*
 * ============================================================================
 * ЗАКОМЕНТОВАНИЙ МОДУЛЬ ПРИЙМАЧА ПУЛЬТА ESP-NOW (main/remote_espnow.c)
 *
 * Зараз увесь файл закоментовано, щоб НЕ вмикати Wi-Fi радіомодуль і не
 * навантажувати живлення / ефір під час налаштування аудіотракту.
 *
 * Щоб активувати прийом команд з пульта ESP-NOW:
 *   1. Розкоментуйте main/remote_espnow.h і цей файл main/remote_espnow.c.
 *   2. Додайте "remote_espnow.c" у main/CMakeLists.txt (SRCS) та
 *      esp_wifi nvs_flash esp_netif esp_event (REQUIRES).
 *   3. У main/main.c розкоментуйте #include "remote_espnow.h" та remote_espnow_init();
 *   4. Перед входом у Light Sleep (за бажанням) можна викликати esp_wifi_stop(),
 *      а після пробудження — esp_wifi_start().
 * ============================================================================
 */

/*
#include "remote_espnow.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "app_state.h"
#include "eeprom_24lc128.h"
#include "audio_manager.h"
#include "autosearch.h"
#include "menu.h"

static const char *TAG = "REMOTE_ESPNOW";

// Зовнішні змінні та функції з основної системи
extern volatile uint8_t is_input_sig_flag;
extern void set_mute_state(bool enable_mute);
extern void toggle_mute(void);
extern void check_system_idle(void);

typedef struct {
    uint8_t src_mac[6];
    remote_packet_t pkt;
} remote_rx_item_t;

static QueueHandle_t s_remote_queue = NULL;
static TimerHandle_t s_eeprom_commit_timer = NULL;
static uint32_t      s_last_seq_id = 0xFFFFFFFFUL;
static bool          s_has_seq = false;
static volatile bool s_wake_requested = false; // Прапорець пробудження підсилювача зі Standby по ESP-NOW

// ============================================================================
// 1. КРИПТО-ФІЛЬТРАЦІЯ ТА ШИФР ЕФІРУ (відсіює сторонній ESP-NOW шум)
// ============================================================================

uint16_t remote_calc_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Keyed 32-bit FNV-1a / Murmur-міксинг поверх 128-бітного ключа CATZILLA_REMOTE_SECRET_KEY.
// Точно такий самий алгоритм має обчислювати пульт перед відправкою пакета!
uint32_t remote_calc_auth_token(uint32_t seq_id, uint8_t cmd, uint8_t param, const uint8_t *src_mac) {
    uint32_t h = 0x811C9DC5UL;

    // 1. Підмішуємо 128-бітний секретний ключ
    for (int i = 0; i < 16; i++) {
        h ^= CATZILLA_REMOTE_SECRET_KEY[i];
        h *= 0x01000193UL;
    }

    // 2. Підмішуємо seq_id (лічильник натискань — кожен пакет має унікальний підпис!)
    for (int i = 0; i < 4; i++) {
        h ^= (uint8_t)((seq_id >> (i * 8)) & 0xFF);
        h *= 0x01000193UL;
    }

    // 3. Підмішуємо команду та параметр
    h ^= cmd;
    h *= 0x01000193UL;
    h ^= param;
    h *= 0x01000193UL;

#if CATZILLA_USE_MAC_BINDING
    // 4. За потреби підмішуємо MAC-адресу пульта-відправника
    if (src_mac != NULL) {
        for (int i = 0; i < 6; i++) {
            h ^= src_mac[i];
            h *= 0x01000193UL;
        }
    }
#else
    (void)src_mac;
#endif

    // Фінальне перемішування (avalanche)
    h ^= (h >> 16);
    h *= 0x85EBCA6BUL;
    h ^= (h >> 13);
    h *= 0xC2B2AE35UL;
    h ^= (h >> 16);
    return h;
}

// ============================================================================
// 2. ОБРОБКА КОМАНД У РОБОЧОМУ ПОТОЦІ (гучність, пресети, режими)
// ============================================================================

static void remote_eeprom_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    // Відкладений запис в 24LC128 через 5 сек після останнього натискання на пульті,
    // щоб не зношувати EEPROM при швидкому крутінні гучності
    settings_commit_check();
}

static void schedule_eeprom_commit(void) {
    if (s_eeprom_commit_timer != NULL) {
        xTimerReset(s_eeprom_commit_timer, 0);
    }
}

static void apply_remote_volume(int target_vol) {
    if (target_vol < 0)   target_vol = 0;
    if (target_vol > 100) target_vol = 100;

    if (is_muted) {
        set_mute_state(false);
    }

    if ((uint8_t)target_vol != master_volume) {
        master_volume = (uint8_t)target_vol;
        g_settings.adau_main_volume = master_volume;
        audio_manager_set_master_volume(master_volume);
        schedule_eeprom_commit();
    }

    if (current_state != STATE_SETUP_MENU && current_state != STATE_SLEEP_SHUTDOWN) {
        current_state = STATE_VOLUME_POPUP;
        last_vol_activity_tick = xTaskGetTickCount();
    }
}

static void execute_remote_command(const remote_packet_t *pkt) {
    remote_cmd_id_t cmd = (remote_cmd_id_t)pkt->cmd;

    // Якщо підсилювач у стані Standby (знеструмлена периферія) — приймаємо ЛИШЕ команди ввімкнення!
    if (current_state == STATE_SLEEP_SHUTDOWN) {
        if (cmd == RCMD_POWER_ON || cmd == RCMD_POWER_TOGGLE) {
            ESP_LOGI(TAG, "Remote WAKE UP command received!");
            s_wake_requested = true;
        }
        return;
    }

    // Оновлюємо таймер автосну (система активна)
    check_system_idle();

    switch (cmd) {
        case RCMD_VOL_UP: {
            int step = (pkt->param > 0 && pkt->param <= 10) ? pkt->param : 1;
            apply_remote_volume((int)master_volume + step);
            break;
        }

        case RCMD_VOL_DOWN: {
            int step = (pkt->param > 0 && pkt->param <= 10) ? pkt->param : 1;
            apply_remote_volume((int)master_volume - step);
            break;
        }

        case RCMD_VOL_SET:
            apply_remote_volume((int)pkt->param);
            break;

        case RCMD_MUTE_TOGGLE:
            toggle_mute();
            break;

        case RCMD_MUTE_SET:
            set_mute_state(pkt->param != 0);
            break;

        case RCMD_EQ_NEXT: {
            uint8_t next_eq = (uint8_t)((g_settings.adau_eq_preset + 1) % EQ_PRESET_MAX);
            g_settings.adau_eq_preset = next_eq;
            eq_menu_pointer = next_eq;
            audio_manager_set_eq_preset((eq_preset_id_t)next_eq);
            schedule_eeprom_commit();
            ESP_LOGI(TAG, "Remote EQ -> %u (%s)", next_eq, eq_names[next_eq]);
            break;
        }

        case RCMD_EQ_SET:
            if (pkt->param < EQ_PRESET_MAX) {
                g_settings.adau_eq_preset = pkt->param;
                eq_menu_pointer = pkt->param;
                audio_manager_set_eq_preset((eq_preset_id_t)pkt->param);
                schedule_eeprom_commit();
                ESP_LOGI(TAG, "Remote EQ -> %u (%s)", pkt->param, eq_names[pkt->param]);
            }
            break;

        case RCMD_FILTER_NEXT: {
            uint8_t next_flt = (uint8_t)((g_settings.audio_preset + 1) % 6);
            g_settings.audio_preset = next_flt;
            filters_menu_pointer = next_flt;
            audio_update_filters(next_flt);
            schedule_eeprom_commit();
            ESP_LOGI(TAG, "Remote Filter -> %u (%s)", next_flt, filter_names[next_flt]);
            break;
        }

        case RCMD_FILTER_SET:
            if (pkt->param < 6) {
                g_settings.audio_preset = pkt->param;
                filters_menu_pointer = pkt->param;
                audio_update_filters(pkt->param);
                schedule_eeprom_commit();
                ESP_LOGI(TAG, "Remote Filter -> %u (%s)", pkt->param, filter_names[pkt->param]);
            }
            break;

        case RCMD_SPATIAL_TOGGLE:
            if (g_settings.sys_mode == 0) {
                // У режимі 2.1 (обхід ADIsurround1) завжди залишається MUSIC
                g_settings.adau_surround_mode = 1;
            } else {
                g_settings.adau_surround_mode = !g_settings.adau_surround_mode;
                audio_manager_set_surround_mode(
                    g_settings.adau_surround_mode ? SURROUND_MODE_MUSIC : SURROUND_MODE_SURROUND
                );
                schedule_eeprom_commit();
            }
            old_menu_pointer = 255;
            break;

        case RCMD_SPATIAL_SET:
            if (g_settings.sys_mode == 0) {
                g_settings.adau_surround_mode = 1;
            } else {
                uint8_t val = (pkt->param != 0) ? 1 : 0; // 0=CINEMA, 1=MUSIC
                g_settings.adau_surround_mode = val;
                audio_manager_set_surround_mode(val ? SURROUND_MODE_MUSIC : SURROUND_MODE_SURROUND);
                schedule_eeprom_commit();
            }
            old_menu_pointer = 255;
            break;

        case RCMD_SYS_MODE_TOGGLE:
            g_settings.sys_mode = !g_settings.sys_mode;
            if (g_settings.sys_mode == 0) {
                g_settings.adau_surround_mode = 1; // 2.1 -> завжди MUSIC
            }
            audio_manager_set_speaker_mode(g_settings.sys_mode ? SPEAKER_MODE_5_1 : SPEAKER_MODE_2_1);
            schedule_eeprom_commit();
            old_menu_pointer = 255;
            break;

        case RCMD_SYS_MODE_SET: {
            uint8_t mode = (pkt->param != 0) ? 1 : 0; // 0=2.1, 1=5.1
            g_settings.sys_mode = mode;
            if (mode == 0) {
                g_settings.adau_surround_mode = 1;
            }
            audio_manager_set_speaker_mode(mode ? SPEAKER_MODE_5_1 : SPEAKER_MODE_2_1);
            schedule_eeprom_commit();
            old_menu_pointer = 255;
            break;
        }

        case RCMD_FORCE_SCAN:
            autosearch_force_scan_from_button();
            break;

        case RCMD_POWER_OFF:
        case RCMD_POWER_TOGGLE:
            ESP_LOGI(TAG, "Remote POWER OFF -> Standby");
            s_wake_requested = false;
            settings_commit_check();
            current_state = STATE_SLEEP_SHUTDOWN;
            break;

        case RCMD_POWER_ON:
            // Підсилювач уже увімкнений — нічого не робимо
            break;

        default:
            ESP_LOGW(TAG, "Unknown remote cmd: %u", (unsigned)pkt->cmd);
            break;
    }
}

bool remote_espnow_consume_wake_request(void) {
    if (s_wake_requested) {
        s_wake_requested = false;
        return true;
    }
    return false;
}

static void remote_process_task(void *pvParameters) {
    (void)pvParameters;
    remote_rx_item_t item;

    while (1) {
        if (xQueueReceive(s_remote_queue, &item, portMAX_DELAY) == pdTRUE) {
            execute_remote_command(&item.pkt);
        }
    }
}

// ============================================================================
// 3. ESP-NOW ПРИЙОМ З ЕФІРУ (Wi-Fi Task Callback)
// ============================================================================

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (!recv_info || !data || len != sizeof(remote_packet_t)) {
        return; // Чужий або битий пакет — мовчки відкидаємо
    }

    remote_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));

    // 1. Перевірка магічного числа
    if (pkt.magic != CATZILLA_REMOTE_MAGIC) {
        return;
    }

    // 2. Перевірка цілісності CRC16
    uint16_t expected_crc = remote_calc_crc16((const uint8_t *)&pkt, offsetof(remote_packet_t, crc16));
    if (pkt.crc16 != expected_crc) {
        ESP_LOGW(TAG, "CRC mismatch from " MACSTR, MAC2STR(recv_info->src_addr));
        return;
    }

#if CATZILLA_USE_MAC_BINDING
    // 3. Жорстка перевірка MAC-адреси нашого пульта
    if (memcmp(recv_info->src_addr, CATZILLA_REMOTE_MAC, 6) != 0) {
        ESP_LOGW(TAG, "Unauthorized MAC: " MACSTR, MAC2STR(recv_info->src_addr));
        return;
    }
#endif

    // 4. Крипто-перевірка auth_token (секретний шифр)
    uint32_t expected_token = remote_calc_auth_token(pkt.seq_id, pkt.cmd, pkt.param, recv_info->src_addr);
    if (pkt.auth_token != expected_token) {
        ESP_LOGW(TAG, "Invalid auth_token from " MACSTR, MAC2STR(recv_info->src_addr));
        return;
    }

    // 5. Анти-дубль (якщо пульт дублює пакет 2-3 рази для надійності)
    if (s_has_seq && pkt.seq_id == s_last_seq_id) {
        return;
    }
    s_last_seq_id = pkt.seq_id;
    s_has_seq = true;

    remote_rx_item_t item;
    memcpy(item.src_mac, recv_info->src_addr, 6);
    item.pkt = pkt;
    if (s_remote_queue != NULL) {
        xQueueSend(s_remote_queue, &item, 0);
    }
}

// ============================================================================
// 4. ІНІЦІАЛІЗАЦІЯ РАДІО ESP-NOW (STA без роутера)
// ============================================================================

esp_err_t remote_espnow_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Працюємо на фіксованому каналі без сканування мереж та без підключення до AP
    ESP_ERROR_CHECK(esp_wifi_set_channel(CATZILLA_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // Зменшуємо потужність передавача (приймачу не потрібні 20 dBm -> менше завад аудіотракту)
    esp_wifi_set_max_tx_power(34);

    ESP_ERROR_CHECK(esp_now_init());
    // Задаємо первинний ключ шифрування ESP-NOW (LMK/PMK) на базі нашого ключа
    ESP_ERROR_CHECK(esp_now_set_pmk(CATZILLA_REMOTE_SECRET_KEY));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    s_eeprom_commit_timer = xTimerCreate("RemoteSave", pdMS_TO_TICKS(5000), pdFALSE, NULL, remote_eeprom_timer_cb);
    s_remote_queue = xQueueCreate(10, sizeof(remote_rx_item_t));
    xTaskCreate(remote_process_task, "remote_espnow", 4096, NULL, 4, NULL);

    uint8_t my_mac[6] = {0};
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "ESP-NOW приймач готовий. Канал=%d, MAC підсилювача=" MACSTR,
             CATZILLA_ESPNOW_CHANNEL, MAC2STR(my_mac));

    return ESP_OK;
}
*/
