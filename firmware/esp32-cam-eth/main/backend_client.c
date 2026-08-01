#include "backend_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "device_config.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "backend_client";
static const char *BOUNDARY = "----EPPCamBoundary7d81b1c4";

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} http_response_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buffer_t *resp = (http_response_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && resp != NULL &&
        !esp_http_client_is_chunked_response(evt->client)) {
        size_t needed = resp->len + evt->data_len + 1;
        if (needed > resp->cap) {
            size_t new_cap = needed + 256;
            char *grown = realloc(resp->buf, new_cap);
            if (!grown) {
                return ESP_FAIL;
            }
            resp->buf = grown;
            resp->cap = new_cap;
        }
        memcpy(resp->buf + resp->len, evt->data, evt->data_len);
        resp->len += evt->data_len;
        resp->buf[resp->len] = '\0';
    }
    return ESP_OK;
}

static const char *json_string_or(const cJSON *obj, const char *key, const char *fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    return fallback;
}

static int json_int_or(const cJSON *obj, const char *key, int fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static void parse_remote_config(const char *json_text, remote_config_t *out)
{
    memset(out, 0, sizeof(*out));

    cJSON *root = cJSON_Parse(json_text);
    if (!root) {
        ESP_LOGW(TAG, "Config remota: JSON inválido");
        return;
    }

    strlcpy(out->config_version, json_string_or(root, "config_version", ""),
             sizeof(out->config_version));

    cJSON *schedule = cJSON_GetObjectItemCaseSensitive(root, "schedule");
    if (schedule) {
        strlcpy(out->schedule.start_time, json_string_or(schedule, "start_time", "07:00:00"),
                 sizeof(out->schedule.start_time));
        strlcpy(out->schedule.end_time, json_string_or(schedule, "end_time", "18:00:00"),
                 sizeof(out->schedule.end_time));
        strlcpy(out->schedule.interval_unit, json_string_or(schedule, "interval_unit", "minutes"),
                 sizeof(out->schedule.interval_unit));
        strlcpy(out->schedule.enabled_days,
                 json_string_or(schedule, "enabled_days", "0,1,2,3,4,5,6"),
                 sizeof(out->schedule.enabled_days));
        out->schedule.interval_value = json_int_or(schedule, "interval_value", 5);
    }

    cJSON *image = cJSON_GetObjectItemCaseSensitive(root, "image_settings");
    out->image_width = json_int_or(image, "width", 1280);
    out->image_height = json_int_or(image, "height", 720);
    out->image_jpeg_quality = json_int_or(image, "jpeg_quality", 75);
    out->image_max_kb = json_int_or(image, "max_kb", 500);

    cJSON *streams = cJSON_GetObjectItemCaseSensitive(root, "streams");
    cJSON *stream;
    cJSON_ArrayForEach(stream, streams) {
        const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(stream, "enabled");
        if (cJSON_IsTrue(enabled) || enabled == NULL) {
            strlcpy(out->stream_id, json_string_or(stream, "id", ""), sizeof(out->stream_id));
            break;
        }
    }

    cJSON_Delete(root);
    out->valid = true;
}

esp_err_t backend_client_fetch_config(remote_config_t *out)
{
    const device_config_t *cfg = device_config_get();
    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/devices/%s/config", cfg->backend_url, cfg->device_id);

    http_response_buffer_t resp = {0};
    esp_http_client_config_t http_cfg = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    esp_http_client_set_header(client, "X-Device-Token", cfg->device_token);

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GET /config falló: %s", esp_err_to_name(err));
        free(resp.buf);
        return err;
    }
    if (status != 200 || !resp.buf) {
        ESP_LOGW(TAG, "GET /config respondió HTTP %d", status);
        free(resp.buf);
        return ESP_FAIL;
    }

    parse_remote_config(resp.buf, out);
    free(resp.buf);
    return out->valid ? ESP_OK : ESP_FAIL;
}

esp_err_t backend_client_upload(const uint8_t *jpeg_data, size_t jpeg_len, const char *stream_id)
{
    const device_config_t *cfg = device_config_get();
    char url[192];
    snprintf(url, sizeof(url), "%s/api/v1/captures", cfg->backend_url);

    char part_stream[160] = "";
    size_t part_stream_len = 0;
    if (stream_id && stream_id[0]) {
        part_stream_len = snprintf(part_stream, sizeof(part_stream),
                                     "--%s\r\nContent-Disposition: form-data; name=\"stream_id\"\r\n\r\n%s\r\n",
                                     BOUNDARY, stream_id);
    }

    char part_file_header[192];
    size_t part_file_header_len = snprintf(
        part_file_header, sizeof(part_file_header),
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n",
        BOUNDARY);

    char part_footer[64];
    size_t part_footer_len = snprintf(part_footer, sizeof(part_footer), "\r\n--%s--\r\n", BOUNDARY);

    size_t content_length = part_stream_len + part_file_header_len + jpeg_len + part_footer_len;

    char content_type_header[96];
    snprintf(content_type_header, sizeof(content_type_header),
              "multipart/form-data; boundary=%s", BOUNDARY);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    esp_http_client_set_header(client, "X-Device-Token", cfg->device_token);
    esp_http_client_set_header(client, "Content-Type", content_type_header);

    esp_err_t err = esp_http_client_open(client, (int)content_length);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo abrir la conexión de subida: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    bool write_ok = true;
    if (part_stream_len && esp_http_client_write(client, part_stream, part_stream_len) < 0) {
        write_ok = false;
    }
    if (write_ok &&
        esp_http_client_write(client, part_file_header, part_file_header_len) < 0) {
        write_ok = false;
    }
    if (write_ok) {
        const size_t CHUNK = 2048;
        size_t offset = 0;
        while (offset < jpeg_len) {
            size_t n = (jpeg_len - offset) < CHUNK ? (jpeg_len - offset) : CHUNK;
            int written = esp_http_client_write(client, (const char *)(jpeg_data + offset), n);
            if (written < 0) {
                write_ok = false;
                break;
            }
            offset += (size_t)written;
        }
    }
    if (write_ok && esp_http_client_write(client, part_footer, part_footer_len) < 0) {
        write_ok = false;
    }

    if (!write_ok) {
        ESP_LOGW(TAG, "Error escribiendo el cuerpo multipart de la captura");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int resp_content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    (void)resp_content_len;

    char discard[256];
    while (esp_http_client_read(client, discard, sizeof(discard)) > 0) {
        // drenar el cuerpo de la respuesta para poder reciclar la conexión
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGW(TAG, "POST /captures respondió HTTP %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

int backend_client_retry_delay_seconds(int retries)
{
    int capped = retries < 0 ? 0 : (retries > 10 ? 10 : retries);
    long value = 5L * (1L << capped);
    return value > 300 ? 300 : (int)value;
}
