#include "status_led.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "sdkconfig.h"

static bool s_enabled = false;

void status_led_init(void)
{
    s_enabled = CONFIG_EPP_STATUS_LED_GPIO >= 0;
    if (!s_enabled) {
        return;
    }
    // El operador ternario evita un shift por valor negativo cuando el pin
    // está deshabilitado (-1) para que el compilador no lo trate como UB
    // en tiempo de compilación; en ese caso ya se retornó arriba.
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << (CONFIG_EPP_STATUS_LED_GPIO >= 0 ? CONFIG_EPP_STATUS_LED_GPIO : 0),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(CONFIG_EPP_STATUS_LED_GPIO, 0);
}

void status_led_set(status_led_state_t state)
{
    if (!s_enabled) {
        return;
    }
    // Patrón mínimo: encendido fijo mientras arranca o hay error, apagado en
    // operación normal. Sin parpadeo (evita depender de un timer adicional).
    int level = (state == STATUS_LED_BOOTING || state == STATUS_LED_ERROR) ? 1 : 0;
    gpio_set_level(CONFIG_EPP_STATUS_LED_GPIO, level);
}
