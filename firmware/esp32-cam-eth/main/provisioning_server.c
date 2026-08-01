#include "provisioning_server.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "device_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mdns.h"
#include "sdkconfig.h"

static const char *TAG = "provisioning";

static const char *PROVISION_FORM_HTML =
    "<html><head><meta charset='utf-8'><title>EPP Sentinel - Alta de camara</title></head>"
    "<body style='font-family:sans-serif;max-width:480px;margin:2rem auto'>"
    "<h2>Alta de camara EPP Sentinel</h2>"
    "<p>Cree el dispositivo en el panel (Configuracion &gt; Nodos) y copie aqui el "
    "<code>device_id</code> y el <code>api_token</code> generados.</p>"
    "<form method='POST' action='/save'>"
    "<label>Backend URL<br><input name='backend_url' placeholder='http://192.168.1.50:8000' "
    "style='width:100%' required></label><br><br>"
    "<label>Device ID<br><input name='device_id' style='width:100%' required></label><br><br>"
    "<label>Device Token<br><input name='device_token' style='width:100%' required></label>"
    "<br><br>"
    "<button type='submit'>Guardar y reiniciar</button>"
    "</form></body></html>";

static const char *LANDING_HTML_FMT =
    "<html><head><meta charset='utf-8'><title>EPP Sentinel Cam</title></head>"
    "<body style='font-family:sans-serif;max-width:480px;margin:2rem auto'>"
    "<h2>EPP Sentinel Cam</h2>"
    "<p>Dispositivo provisionado.</p>"
    "<ul><li>device_id: %s</li><li>backend_url: %s</li></ul>"
    "<p><a href='/status'>Ver estado</a></p>"
    "</body></html>";

typedef struct {
    bool last_capture_ok;
    bool has_capture;
    char last_capture_note[96];
    time_t last_capture_time;
    char note[128];
} provisioning_status_t;

static provisioning_status_t s_status;
static SemaphoreHandle_t s_status_mutex;

static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 1 < dst_size; si++) {
        char c = src[si];
        if (c == '+') {
            dst[di++] = ' ';
        } else if (c == '%' && src[si + 1] != '\0' && src[si + 2] != '\0') {
            char hex[3] = {src[si + 1], src[si + 2], '\0'};
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else {
            dst[di++] = c;
        }
    }
    dst[di] = '\0';
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (!device_config_is_provisioned()) {
        return httpd_resp_send(req, PROVISION_FORM_HTML, HTTPD_RESP_USE_STRLEN);
    }
    const device_config_t *cfg = device_config_get();
    char page[768];
    snprintf(page, sizeof(page), LANDING_HTML_FMT, cfg->device_id, cfg->backend_url);
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len >= 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload invalido");
        return ESP_FAIL;
    }
    char buf[512];
    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error de lectura");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';

    char raw_backend[128] = {0};
    char raw_id[64] = {0};
    char raw_token[96] = {0};
    httpd_query_key_value(buf, "backend_url", raw_backend, sizeof(raw_backend));
    httpd_query_key_value(buf, "device_id", raw_id, sizeof(raw_id));
    httpd_query_key_value(buf, "device_token", raw_token, sizeof(raw_token));

    char backend_url[128];
    char device_id[64];
    char device_token[96];
    url_decode(backend_url, raw_backend, sizeof(backend_url));
    url_decode(device_id, raw_id, sizeof(device_id));
    url_decode(device_token, raw_token, sizeof(device_token));

    if (!backend_url[0] || !device_id[0] || !device_token[0]) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Todos los campos son obligatorios");
        return ESP_FAIL;
    }

    if (device_config_save(backend_url, device_id, device_token) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo guardar en NVS");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, "<html><body><h3>Configuracion guardada. Reiniciando...</h3></body></html>",
                     HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    provisioning_status_t snapshot;
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    snapshot = s_status;
    xSemaphoreGive(s_status_mutex);

    char capture_line[160];
    if (snapshot.has_capture) {
        char time_buf[32] = "-";
        struct tm tm_info;
        if (localtime_r(&snapshot.last_capture_time, &tm_info)) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
        }
        snprintf(capture_line, sizeof(capture_line), "%s a las %s (%s)",
                  snapshot.last_capture_ok ? "OK" : "FALLO", time_buf, snapshot.last_capture_note);
    } else {
        snprintf(capture_line, sizeof(capture_line), "(sin capturas todavia)");
    }

    const device_config_t *cfg = device_config_get();
    char page[900];
    snprintf(page, sizeof(page),
              "<html><head><meta charset='utf-8'><title>Estado</title></head>"
              "<body style='font-family:sans-serif;max-width:480px;margin:2rem auto'>"
              "<h2>Estado EPP Sentinel Cam</h2>"
              "<ul>"
              "<li>device_id: %s</li>"
              "<li>backend_url: %s</li>"
              "<li>Ultima captura: %s</li>"
              "<li>Nota: %s</li>"
              "</ul>"
              "<p><a href='/'>Volver</a></p>"
              "</body></html>",
              cfg->provisioned ? cfg->device_id : "(sin provisionar)",
              cfg->provisioned ? cfg->backend_url : "-", capture_line,
              snapshot.note[0] ? snapshot.note : "-");

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

esp_err_t provisioning_server_start(void)
{
    s_status_mutex = xSemaphoreCreateMutex();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    char hostname[48];
    snprintf(hostname, sizeof(hostname), "%s-%02x%02x%02x", CONFIG_EPP_MDNS_HOSTNAME_PREFIX,
              mac[3], mac[4], mac[5]);

    esp_err_t err = mdns_init();
    if (err == ESP_OK) {
        mdns_hostname_set(hostname);
        mdns_instance_name_set("EPP Sentinel Cam");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        ESP_LOGI(TAG, "mDNS activo: http://%s.local/", hostname);
    } else {
        ESP_LOGW(TAG, "mDNS no disponible: %s", esp_err_to_name(err));
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;
    esp_err_t start_err = httpd_start(&server, &config);
    if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar httpd: %s", esp_err_to_name(start_err));
        return start_err;
    }

    static const httpd_uri_t root_uri = {
        .uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    static const httpd_uri_t save_uri = {
        .uri = "/save", .method = HTTP_POST, .handler = save_post_handler};
    static const httpd_uri_t status_uri = {
        .uri = "/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &save_uri);
    httpd_register_uri_handler(server, &status_uri);

    ESP_LOGI(TAG, "Portal de provisioning en http://%s.local/ (provisioned=%d)", hostname,
              device_config_is_provisioned());
    return ESP_OK;
}

void provisioning_status_set_capture(bool success, const char *detail)
{
    if (!s_status_mutex) {
        return;
    }
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    s_status.has_capture = true;
    s_status.last_capture_ok = success;
    s_status.last_capture_time = time(NULL);
    strlcpy(s_status.last_capture_note, detail ? detail : "", sizeof(s_status.last_capture_note));
    xSemaphoreGive(s_status_mutex);
}

void provisioning_status_set_note(const char *note)
{
    if (!s_status_mutex) {
        return;
    }
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    strlcpy(s_status.note, note ? note : "", sizeof(s_status.note));
    xSemaphoreGive(s_status_mutex);
}
