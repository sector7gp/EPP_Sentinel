#include "provisioning_server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "camera.h"
#include "device_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "mdns.h"
#include "ota_update.h"
#include "sdkconfig.h"

static const char *TAG = "provisioning";

// Paleta compartida con el panel web (frontend/src/styles.css: slate + sky)
// para que el portal del ESP32 se sienta parte del mismo producto.
static const char *PAGE_CSS =
    ":root{color-scheme:light dark}"
    "*{box-sizing:border-box}"
    "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
    "background:#f1f5f9;color:#0f172a}"
    ".wrap{max-width:480px;margin:0 auto;padding:1.5rem 1.25rem 3rem}"
    "header{margin-bottom:1.25rem}"
    "header h1{font-size:1.1rem;margin:0 0 .5rem;color:#0369a1}"
    "nav{display:flex;gap:.75rem;flex-wrap:wrap;font-size:.85rem}"
    "nav a{color:#64748b;text-decoration:none}"
    "nav a:hover{color:#0369a1;text-decoration:underline}"
    ".card{background:#fff;border:1px solid #e2e8f0;border-radius:12px;padding:1.25rem;"
    "box-shadow:0 1px 2px rgba(15,23,42,.06)}"
    "h2{margin-top:0;font-size:1.05rem}"
    "label{display:block;font-size:.85rem;color:#475569;margin:.85rem 0 .3rem}"
    "input,select{width:100%;padding:.55rem .6rem;border:1px solid #cbd5e1;border-radius:8px;"
    "font-size:.95rem;font-family:inherit}"
    "input:focus,select:focus{outline:none;border-color:#0369a1;box-shadow:0 0 0 1px #0369a1}"
    "button{margin-top:1.25rem;width:100%;padding:.65rem;border:none;border-radius:8px;"
    "background:#0369a1;color:#fff;font-size:.95rem;font-weight:600;cursor:pointer}"
    "button:hover{background:#075985}"
    ".muted{color:#64748b;font-size:.85rem}"
    "code{background:#f1f5f9;padding:.1rem .3rem;border-radius:4px;font-size:.85em}"
    "ul.kv{list-style:none;padding:0;margin:0}"
    "ul.kv li{display:flex;justify-content:space-between;gap:1rem;padding:.4rem 0;"
    "border-bottom:1px solid #e2e8f0;font-size:.9rem}"
    "ul.kv li:last-child{border-bottom:none}"
    ".badge{display:inline-block;padding:.15rem .55rem;border-radius:999px;font-size:.78rem;"
    "font-weight:600}"
    ".badge-ok{background:#dcfce7;color:#166534}"
    ".badge-err{background:#fee2e2;color:#991b1b}"
    ".monitor-img{width:100%;border-radius:8px;border:1px solid #e2e8f0;display:block;"
    "background:#0f172a;min-height:180px}"
    "@media (prefers-color-scheme:dark){"
    "body{background:#0f172a;color:#e2e8f0}"
    ".card{background:#1e293b;border-color:#334155}"
    "header h1{color:#38bdf8}"
    "nav a{color:#94a3b8}nav a:hover{color:#38bdf8}"
    "input,select{background:#0f172a;border-color:#334155;color:#e2e8f0}"
    "code{background:#0f172a}"
    "ul.kv li{border-color:#334155}"
    ".muted{color:#94a3b8}"
    "}";

static const char *PROVISION_FORM_BODY =
    "<h2>Alta de camara</h2>"
    "<p class='muted'>Cree el dispositivo en el panel (Configuracion &gt; Nodos) y copie aqui "
    "el <code>device_id</code> y el <code>api_token</code> generados.</p>"
    "<form method='POST' action='/save'>"
    "<label>Backend URL<input name='backend_url' placeholder='http://192.168.1.50:8000' "
    "required></label>"
    "<label>Device ID<input name='device_id' required></label>"
    "<label>Device Token<input name='device_token' required></label>"
    "<button type='submit'>Guardar y reiniciar</button>"
    "</form>";

static const char *SETTINGS_FORM_BODY =
    "<h2>Ajustes de monitor</h2>"
    "<p class='muted'>Solo cambia lo que ves en <a href='/monitor'>/monitor</a>. La "
    "configuracion real de captura sigue viniendo del panel web.</p>"
    "<form method='GET' action='/monitor'>"
    "<label>Resolucion<select name='resolution'>"
    "<option value='320x240'>QVGA 320x240</option>"
    "<option value='640x480'>VGA 640x480</option>"
    "<option value='800x600'>SVGA 800x600</option>"
    "<option value='1024x768'>XGA 1024x768</option>"
    "<option value='1280x720' selected>HD 1280x720</option>"
    "<option value='1600x1200'>UXGA 1600x1200</option>"
    "<option value='1920x1080'>Full HD 1920x1080</option>"
    "<option value='2048x1536'>QXGA 2048x1536</option>"
    "<option value='2560x1920'>QSXGA 2560x1920 (maximo)</option>"
    "</select></label>"
    "<label>Calidad JPEG (30-100)<input type='number' name='quality' min='30' max='100' "
    "value='75'></label>"
    "<label>Maximo KB (50-5000)<input type='number' name='max_kb' min='50' max='5000' "
    "value='500'></label>"
    "<button type='submit'>Ver monitor</button>"
    "</form>";

static const char *OTA_FORM_BODY =
    "<h2>Actualizar firmware (OTA)</h2>"
    "<p class='muted'>Sube el .bin compilado "
    "(<code>build/epp_sentinel_cam.bin</code>). La contrasena es el "
    "<code>device_token</code> de este dispositivo (el usuario no importa).</p>"
    "<form id='ota-form'>"
    "<label>Token del dispositivo<input type='password' id='ota-token' required></label>"
    "<label>Archivo .bin<input type='file' id='ota-file' accept='.bin' required></label>"
    "<button type='submit'>Actualizar y reiniciar</button>"
    "</form>"
    "<p id='ota-status' class='muted'></p>"
    "<script>"
    "document.getElementById('ota-form').addEventListener('submit',function(ev){"
    "ev.preventDefault();"
    "var token=document.getElementById('ota-token').value;"
    "var file=document.getElementById('ota-file').files[0];"
    "var status=document.getElementById('ota-status');"
    "if(!file){return;}"
    "status.textContent='Subiendo '+file.name+' ('+file.size+' bytes)...';"
    "fetch('/ota',{method:'POST',headers:{"
    "'Authorization':'Basic '+btoa(':'+token),"
    "'Content-Type':'application/octet-stream'"
    "},body:file}).then(function(res){"
    "if(res.ok){status.textContent='OK, reiniciando con el firmware nuevo...';}"
    "else{res.text().then(function(t){status.textContent='Error '+res.status+': '+t;});}"
    "}).catch(function(err){status.textContent='Error de red: '+err;});"
    "});"
    "</script>";

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

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// query debe venir null-terminado (ver *_get_handler, que la copian con
// httpd_req_get_url_query_str a un buffer local ya cero-inicializado).
static int query_int(const char *query, const char *key, int def_val)
{
    char val[16];
    if (query && httpd_query_key_value(query, key, val, sizeof(val)) == ESP_OK) {
        char *end = NULL;
        long v = strtol(val, &end, 10);
        if (end != val) {
            return (int)v;
        }
    }
    return def_val;
}

static void parse_resolution(const char *query, int *width, int *height)
{
    *width = 1280;
    *height = 720;
    char val[24];
    if (!query || httpd_query_key_value(query, "resolution", val, sizeof(val)) != ESP_OK) {
        return;
    }
    char *x = strchr(val, 'x');
    if (!x) {
        return;
    }
    *x = '\0';
    int w = atoi(val);
    int h = atoi(x + 1);
    if (w > 0 && h > 0) {
        *width = w;
        *height = h;
    }
}

static esp_err_t send_chunk(httpd_req_t *req, const char *s)
{
    return httpd_resp_send_chunk(req, s, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_page_open(httpd_req_t *req, const char *title)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    char head[2560];
    int n = snprintf(head, sizeof(head),
                       "<!doctype html><html><head><meta charset='utf-8'>"
                       "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                       "<title>%s - EPP Sentinel Cam</title><style>%s</style></head>"
                       "<body><div class='wrap'><header><h1>EPP Sentinel Cam</h1>"
                       "<nav><a href='/'>Inicio</a><a href='/status'>Estado</a>"
                       "<a href='/monitor'>Monitor</a><a href='/settings'>Ajustes</a>"
                       "<a href='/ota'>OTA</a></nav>"
                       "</header><main class='card'>",
                       title, PAGE_CSS);
    if (n < 0 || (size_t)n >= sizeof(head)) {
        ESP_LOGE(TAG, "Buffer de cabecera de pagina insuficiente (%d)", n);
        return ESP_FAIL;
    }
    return send_chunk(req, head);
}

static esp_err_t send_page_close(httpd_req_t *req)
{
    send_chunk(req, "</main></div></body></html>");
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    if (!device_config_is_provisioned()) {
        if (send_page_open(req, "Alta de camara") != ESP_OK) {
            return ESP_FAIL;
        }
        send_chunk(req, PROVISION_FORM_BODY);
        return send_page_close(req);
    }

    const device_config_t *cfg = device_config_get();
    if (send_page_open(req, "Inicio") != ESP_OK) {
        return ESP_FAIL;
    }
    char body[512];
    snprintf(body, sizeof(body),
              "<h2>Dispositivo provisionado</h2>"
              "<ul class='kv'><li><span>device_id</span><span>%s</span></li>"
              "<li><span>backend_url</span><span>%s</span></li></ul>"
              "<p class='muted'>Ver <a href='/status'>estado</a> o "
              "<a href='/monitor'>monitor en vivo</a>.</p>",
              cfg->device_id, cfg->backend_url);
    send_chunk(req, body);
    return send_page_close(req);
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

    if (send_page_open(req, "Guardado") == ESP_OK) {
        send_chunk(req, "<h2>Configuracion guardada</h2>"
                          "<p class='muted'>Reiniciando el dispositivo...</p>");
        send_page_close(req);
    }
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

    char capture_line[200];
    if (snapshot.has_capture) {
        char time_buf[32] = "-";
        struct tm tm_info;
        if (localtime_r(&snapshot.last_capture_time, &tm_info)) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
        }
        snprintf(capture_line, sizeof(capture_line),
                  "<span class='badge %s'>%s</span> %s &middot; %s",
                  snapshot.last_capture_ok ? "badge-ok" : "badge-err",
                  snapshot.last_capture_ok ? "OK" : "FALLO", time_buf, snapshot.last_capture_note);
    } else {
        snprintf(capture_line, sizeof(capture_line), "<span class='muted'>(sin capturas todavia)</span>");
    }

    const device_config_t *cfg = device_config_get();
    if (send_page_open(req, "Estado") != ESP_OK) {
        return ESP_FAIL;
    }
    char body[900];
    snprintf(body, sizeof(body),
              "<h2>Estado</h2><ul class='kv'>"
              "<li><span>device_id</span><span>%s</span></li>"
              "<li><span>backend_url</span><span>%s</span></li>"
              "<li><span>Ultima captura</span><span>%s</span></li>"
              "<li><span>Nota</span><span>%s</span></li>"
              "</ul><p class='muted'><a href='/monitor'>Ver monitor en vivo</a></p>",
              cfg->provisioned ? cfg->device_id : "(sin provisionar)",
              cfg->provisioned ? cfg->backend_url : "-", capture_line,
              snapshot.note[0] ? snapshot.note : "-");
    send_chunk(req, body);
    return send_page_close(req);
}

// Snapshot JPEG unico: ?resolution=WxH&quality=30-100&max_kb=50-5000.
// Comparte camera_capture_jpeg/camera_release (y su mutex) con el loop
// principal, asi que puede demorar si coincide con una captura programada.
static esp_err_t capture_get_handler(httpd_req_t *req)
{
    char query[128] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));

    int width, height;
    parse_resolution(query, &width, &height);
    width = clampi(width, 320, 2560);
    height = clampi(height, 240, 1920);
    int quality = clampi(query_int(query, "quality", 75), 30, 100);
    int max_kb = clampi(query_int(query, "max_kb", 500), 50, 5000);

    const uint8_t *buf = NULL;
    size_t len = 0;
    if (camera_capture_jpeg(width, height, quality, max_kb, &buf, &len) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Fallo de captura");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, (const char *)buf, len);
    camera_release();
    return err;
}

// Pagina con <img> apuntando a /capture que se refresca sola por JS.
static esp_err_t monitor_get_handler(httpd_req_t *req)
{
    char query[128] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));

    int width, height;
    parse_resolution(query, &width, &height);
    width = clampi(width, 320, 2560);
    height = clampi(height, 240, 1920);
    int quality = clampi(query_int(query, "quality", 75), 30, 100);
    int max_kb = clampi(query_int(query, "max_kb", 500), 50, 5000);

    char capture_src[96];
    snprintf(capture_src, sizeof(capture_src), "/capture?resolution=%dx%d&quality=%d&max_kb=%d",
              width, height, quality, max_kb);

    if (send_page_open(req, "Monitor") != ESP_OK) {
        return ESP_FAIL;
    }
    char body[768];
    snprintf(body, sizeof(body),
              "<h2>Monitor en vivo</h2>"
              "<p class='muted'>%dx%d &middot; calidad %d &middot; max %d KB &middot; "
              "<a href='/settings'>cambiar</a></p>"
              "<img id='mon' class='monitor-img' src='%s' alt='Vista de la camara'>"
              "<script>"
              "var src='%s',img=document.getElementById('mon');"
              "setInterval(function(){img.src=src+'&t='+Date.now();},1500);"
              "</script>",
              width, height, quality, max_kb, capture_src, capture_src);
    send_chunk(req, body);
    return send_page_close(req);
}

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    if (send_page_open(req, "Ajustes de monitor") != ESP_OK) {
        return ESP_FAIL;
    }
    send_chunk(req, SETTINGS_FORM_BODY);
    return send_page_close(req);
}

// Compara la contraseña de un header "Authorization: Basic base64(:token)"
// contra el device_token provisionado. El usuario se ignora.
static bool ota_check_auth(httpd_req_t *req)
{
    char auth_header[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK) {
        return false;
    }
    const char *prefix = "Basic ";
    size_t prefix_len = strlen(prefix);
    if (strncmp(auth_header, prefix, prefix_len) != 0) {
        return false;
    }

    unsigned char decoded[96] = {0};
    size_t decoded_len = 0;
    const char *b64 = auth_header + prefix_len;
    if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &decoded_len,
                                (const unsigned char *)b64, strlen(b64)) != 0) {
        return false;
    }
    decoded[decoded_len] = '\0';

    char *colon = strchr((char *)decoded, ':');
    const char *password = colon ? colon + 1 : (const char *)decoded;

    const device_config_t *cfg = device_config_get();
    return cfg->provisioned && password[0] != '\0' && strcmp(password, cfg->device_token) == 0;
}

static esp_err_t ota_require_auth(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"EPP Sentinel Cam\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Token invalido");
    return ESP_FAIL;
}

static esp_err_t ota_get_handler(httpd_req_t *req)
{
    if (send_page_open(req, "Actualizar firmware") != ESP_OK) {
        return ESP_FAIL;
    }
    send_chunk(req, OTA_FORM_BODY);
    return send_page_close(req);
}

#define OTA_CHUNK_SIZE 1024

// Recibe el .bin crudo (application/octet-stream, no multipart) en el body
// y lo escribe a la particion OTA "next" en bloques chicos: nada de
// buffers grandes en la pila de la tarea httpd (ver el comentario de
// stack_size en provisioning_server_start).
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    if (!ota_check_auth(req)) {
        return ota_require_auth(req);
    }
    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Falta el archivo .bin");
        return ESP_FAIL;
    }

    if (ota_update_begin() != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo iniciar OTA "
                              "(revisar particiones ota_0/ota_1)");
        return ESP_FAIL;
    }

    char buf[OTA_CHUNK_SIZE];
    int remaining = req->content_len;
    while (remaining > 0) {
        int to_read = remaining < OTA_CHUNK_SIZE ? remaining : OTA_CHUNK_SIZE;
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            ota_update_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error leyendo la subida");
            return ESP_FAIL;
        }
        if (ota_update_write((const uint8_t *)buf, received) != ESP_OK) {
            ota_update_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error escribiendo OTA");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    if (ota_update_finish() != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Imagen invalida o incompleta");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_send(req, "OK: actualizando y reiniciando...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
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
    config.max_uri_handlers = 10;
    // El default (4096) no alcanza: send_page_open() usa un buffer de 2560
    // bytes en la pila de la tarea del httpd, sumado al buffer propio de
    // cada handler (hasta ~900 bytes) y al overhead de snprintf. Sin este
    // aumento se pisa la pila y crashea con errores random de lwIP/FreeRTOS
    // en el primer request tras el boot.
    config.stack_size = 8192;
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
    static const httpd_uri_t capture_uri = {
        .uri = "/capture", .method = HTTP_GET, .handler = capture_get_handler};
    static const httpd_uri_t monitor_uri = {
        .uri = "/monitor", .method = HTTP_GET, .handler = monitor_get_handler};
    static const httpd_uri_t settings_uri = {
        .uri = "/settings", .method = HTTP_GET, .handler = settings_get_handler};
    static const httpd_uri_t ota_get_uri = {
        .uri = "/ota", .method = HTTP_GET, .handler = ota_get_handler};
    static const httpd_uri_t ota_post_uri = {
        .uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler};
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &save_uri);
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &capture_uri);
    httpd_register_uri_handler(server, &monitor_uri);
    httpd_register_uri_handler(server, &settings_uri);
    httpd_register_uri_handler(server, &ota_get_uri);
    httpd_register_uri_handler(server, &ota_post_uri);

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
