/* Firmware entry (M1: skeleton only, no device code yet; SPEC 12 M3/M4). */
#include "esp_log.h"
#include "hal_sequent.h"
#include "safety.h"

static const char *TAG = "main";

void app_main(void)
{
    /* S1: first action after every start, before UI and network. The SM-H-001 keeps its
     * relays over a P4 reset, so they are switched off here (SPEC 4.2, 6). */
    hal_io_t *io = hal_sequent_io();
    int failed = safety_boot_all_off(io);
    if (failed) {
        ESP_LOGE(TAG, "S1: %d output writes failed (hal_sequent not implemented before M4)", failed);
    }

    /* TODO(M3): display_viewe68 + touch, LVGL, ui_init.
     * TODO(M4): hal_sequent (SM-H-001, SM-I-009 on I2C 0), hal_ladder, profile from LittleFS,
     *           io task (20 ms) and logic task (50 ms) with task watchdog (S10), cerbo client. */
    ESP_LOGI(TAG, "camping unit skeleton started");
}
