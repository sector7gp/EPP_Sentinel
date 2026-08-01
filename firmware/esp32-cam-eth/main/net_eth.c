#include "net_eth.h"

#include <string.h>

#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "esp_eth_mac.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

static const char *TAG = "net_eth";

static EventGroupHandle_t s_eth_event_group;
static const int GOT_IP_BIT = BIT0;
static volatile bool s_connected = false;

static void eth_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == ETH_EVENT) {
        switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Link up");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Link down");
            s_connected = false;
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Driver iniciado");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGW(TAG, "Driver detenido");
            s_connected = false;
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        xEventGroupSetBits(s_eth_event_group, GOT_IP_BIT);
    }
}

bool net_eth_is_connected(void)
{
    return s_connected;
}

esp_err_t net_eth_start(uint32_t timeout_ms)
{
    if (CONFIG_EPP_ETH_GPIO_MISO < 0 || CONFIG_EPP_ETH_GPIO_MOSI < 0 ||
        CONFIG_EPP_ETH_GPIO_SCLK < 0 || CONFIG_EPP_ETH_GPIO_CS < 0 ||
        CONFIG_EPP_ETH_GPIO_INT < 0) {
        ESP_LOGE(TAG, "Pines del W5500 sin configurar: ejecute 'idf.py menuconfig' "
                       "y complete 'EPP Sentinel Camera > Ethernet W5500 (SPI)' "
                       "con el pinout real de la placa");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // El driver MAC del W5500 usa gpio_isr_handler_add() en el pin INT
    // internamente; sin este servicio instalado antes, falla en silencio
    // y el link/RX no funciona aunque esp_eth_start() no devuelva error.
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service falló: %s", esp_err_to_name(isr_err));
        return isr_err;
    }

    s_eth_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                          &eth_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                          &eth_event_handler, NULL, NULL));

    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EPP_ETH_GPIO_MISO,
        .mosi_io_num = CONFIG_EPP_ETH_GPIO_MOSI,
        .sclk_io_num = CONFIG_EPP_ETH_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(CONFIG_EPP_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                         TAG, "spi_bus_initialize");

    spi_device_interface_config_t devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = CONFIG_EPP_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EPP_ETH_GPIO_CS,
        .queue_size = 20,
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_EPP_ETH_SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = CONFIG_EPP_ETH_GPIO_INT;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = CONFIG_EPP_ETH_GPIO_RST;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!mac || !phy) {
        ESP_LOGE(TAG, "No se pudo crear el driver W5500 (revisar pinout SPI)");
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&eth_config, &eth_handle), TAG,
                         "esp_eth_driver_install");

    // El W5500 no trae MAC de fábrica: se deriva una localmente administrada
    // a partir de la MAC base del chip (mismo enfoque que los ejemplos de
    // Ethernet SPI de ESP-IDF).
    uint8_t mac_addr[6];
    ESP_ERROR_CHECK(esp_read_mac(mac_addr, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    ESP_RETURN_ON_ERROR(
        esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)), TAG,
        "esp_netif_attach");

    ESP_RETURN_ON_ERROR(esp_eth_start(eth_handle), TAG, "esp_eth_start");

    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group, GOT_IP_BIT, pdFALSE, pdTRUE,
                                            pdMS_TO_TICKS(timeout_ms));
    if (!(bits & GOT_IP_BIT)) {
        ESP_LOGW(TAG, "Timeout esperando IP por DHCP; se seguirá reintentando en segundo plano");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
