#pragma once

typedef enum {
    STATUS_LED_OFF,
    STATUS_LED_BOOTING,
    STATUS_LED_OK,
    STATUS_LED_ERROR,
} status_led_state_t;

/** No-op si EPP_STATUS_LED_GPIO es -1 (deshabilitado). */
void status_led_init(void);
void status_led_set(status_led_state_t state);
