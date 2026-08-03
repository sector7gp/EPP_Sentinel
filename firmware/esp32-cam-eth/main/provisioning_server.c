#include "provisioning_server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "backend_client.h"
#include "camera.h"
#include "camera_settings.h"
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
    "h3{font-size:.85rem;text-transform:uppercase;letter-spacing:.03em;"
    "color:#64748b;margin:1.5rem 0 .25rem;border-top:1px solid #e2e8f0;padding-top:1rem}"
    "form>h3:first-of-type{border-top:none;padding-top:0}"
    "label.checkbox{display:flex;align-items:center;gap:.5rem;margin:.6rem 0}"
    "label.checkbox input{width:auto}"
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
    "h3{color:#94a3b8;border-color:#334155}"
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
    char head[3200];
    int n = snprintf(head, sizeof(head),
                       "<!doctype html><html><head><meta charset='utf-8'>"
                       "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                       "<title>%s - EPP Sentinel Cam</title><style>%s</style></head>"
                       "<body><div class='wrap'><header><h1>EPP Sentinel Cam</h1>"
                       "<nav><a href='/'>Inicio</a><a href='/status'>Estado</a>"
                       "<a href='/monitor'>Monitor</a>"
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
              "<li><span>backend_url</span><span>%s</span></li></ul>",
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
// Helpers para armar el formulario de ajustes de imagen sin buffers grandes
// en la pila de la tarea httpd (cada uno arma un fragmento chico y lo manda
// como chunk aparte).
static void send_num_field(httpd_req_t *req, const char *label, const char *name, int min_v,
                             int max_v, int value)
{
    char buf[192];
    snprintf(buf, sizeof(buf),
              "<label>%s<input type='number' name='%s' min='%d' max='%d' value='%d'></label>",
              label, name, min_v, max_v, value);
    send_chunk(req, buf);
}

static void send_checkbox_field(httpd_req_t *req, const char *label, const char *name, bool checked)
{
    char buf[192];
    snprintf(buf, sizeof(buf),
              "<label class='checkbox'><input type='checkbox' name='%s' value='1'%s> %s</label>",
              name, checked ? " checked" : "", label);
    send_chunk(req, buf);
}

static void send_select_open(httpd_req_t *req, const char *label, const char *name)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "<label>%s<select name='%s'>", label, name);
    send_chunk(req, buf);
}

static void send_option(httpd_req_t *req, int value, const char *text, bool selected)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "<option value='%d'%s>%s</option>", value,
              selected ? " selected" : "", text);
    send_chunk(req, buf);
}

static void send_select_close(httpd_req_t *req)
{
    send_chunk(req, "</select></label>");
}

// Monitor + ajustes en una sola página: el formulario de ajustes de imagen
// se manda por fetch() (ver JS al final) en vez de POST normal, así no
// interrumpe el <img> que se refresca solo mientras se están probando
// valores.
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
    char body[512];
    snprintf(body, sizeof(body),
              "<h2>Monitor en vivo</h2>"
              "<p class='muted'>%dx%d &middot; calidad %d &middot; max %d KB</p>"
              "<img id='mon' class='monitor-img' src='%s' alt='Vista de la camara'>",
              width, height, quality, max_kb, capture_src);
    send_chunk(req, body);

    // Formulario de preset de resolucion (no persiste, ver SETTINGS_FORM_BODY
    // mas arriba): string estatico, se manda tal cual sin pasar por snprintf.
    send_chunk(req, SETTINGS_FORM_BODY);

    char script[800];
    snprintf(script, sizeof(script),
              "<button type='button' id='force-btn'>Forzar envio al backend</button>"
              "<p id='force-status' class='muted'></p>"
              "<script>"
              "var src='%s',img=document.getElementById('mon');"
              "setInterval(function(){img.src=src+'&t='+Date.now();},500);"
              "document.getElementById('force-btn').addEventListener('click',function(){"
              "var st=document.getElementById('force-status');st.textContent='Enviando...';"
              "fetch('/capture-now',{method:'POST'}).then(function(r){return r.text().then("
              "function(t){st.textContent=t;});}).catch(function(e){st.textContent="
              "'Error de red: '+e;});"
              "});"
              "</script>",
              capture_src);
    send_chunk(req, script);

    const camera_settings_t *cs = camera_settings_get();

    send_chunk(req, "<h2>Ajustes de imagen</h2>"
                      "<p class='muted'>Estos se guardan en el dispositivo y se aplican a "
                      "toda captura (no solo a este monitor). Se guardan sin recargar la "
                      "pagina, para poder ver el efecto en vivo.</p>"
                      "<form id='camset-form'>"
                      "<h3>Imagen</h3>");
    send_num_field(req, "Brillo (-3 a 3)", "brightness", -3, 3, cs->brightness);
    send_num_field(req, "Contraste (-3 a 3)", "contrast", -3, 3, cs->contrast);
    send_num_field(req, "Saturacion (-4 a 4)", "saturation", -4, 4, cs->saturation);
    send_num_field(req, "Nitidez (-3 a 3)", "sharpness", -3, 3, cs->sharpness);
    send_num_field(req, "Reduccion de ruido (0 a 8)", "denoise", 0, 8, cs->denoise);

    send_chunk(req, "<h3>Balance de blancos</h3>");
    send_checkbox_field(req, "Automatico (AWB)", "whitebal", cs->whitebal);
    send_select_open(req, "Modo fijo (si AWB esta apagado)", "wb_mode");
    send_option(req, 0, "Auto", cs->wb_mode == 0);
    send_option(req, 1, "Soleado", cs->wb_mode == 1);
    send_option(req, 2, "Nublado", cs->wb_mode == 2);
    send_option(req, 3, "Oficina", cs->wb_mode == 3);
    send_option(req, 4, "Interior/hogar", cs->wb_mode == 4);
    send_select_close(req);

    send_chunk(req, "<h3>Exposicion</h3>");
    send_checkbox_field(req, "Automatica (AEC)", "exposure_ctrl", cs->exposure_ctrl);
    send_checkbox_field(req, "Algoritmo AEC mejorado (AEC2)", "aec2", cs->aec2);
    send_num_field(req, "Compensacion de exposicion (-5 a 5)", "ae_level", -5, 5, cs->ae_level);

    send_chunk(req, "<h3>Ganancia</h3>");
    send_checkbox_field(req, "Automatica (AGC)", "gain_ctrl", cs->gain_ctrl);
    send_num_field(req, "Ganancia manual (0 a 64, si AGC esta apagado)", "agc_gain", 0, 64,
                     cs->agc_gain);
    send_select_open(req, "Techo de ganancia (AGC)", "gainceiling");
    send_option(req, 0, "2x", cs->gainceiling == 0);
    send_option(req, 1, "4x", cs->gainceiling == 1);
    send_option(req, 2, "8x", cs->gainceiling == 2);
    send_option(req, 3, "16x", cs->gainceiling == 3);
    send_option(req, 4, "32x", cs->gainceiling == 4);
    send_option(req, 5, "64x", cs->gainceiling == 5);
    send_option(req, 6, "128x", cs->gainceiling == 6);
    send_select_close(req);

    send_chunk(req, "<h3>Orientacion</h3>");
    send_checkbox_field(req, "Espejo horizontal", "hmirror", cs->hmirror);
    send_checkbox_field(req, "Voltear vertical", "vflip", cs->vflip);

    send_chunk(req, "<h3>Correccion de imagen</h3>");
    send_checkbox_field(req, "Correccion de viñeteado (lente)", "lenc", cs->lenc);
    send_checkbox_field(req, "Correccion de pixeles negros", "bpc", cs->bpc);
    send_checkbox_field(req, "Correccion de pixeles blancos", "wpc", cs->wpc);

    send_chunk(req, "<h3>Efecto especial</h3>");
    send_select_open(req, "Efecto", "special_effect");
    send_option(req, 0, "Ninguno", cs->special_effect == 0);
    send_option(req, 1, "Negativo", cs->special_effect == 1);
    send_option(req, 2, "Escala de grises", cs->special_effect == 2);
    send_option(req, 3, "Tinte rojo", cs->special_effect == 3);
    send_option(req, 4, "Tinte verde", cs->special_effect == 4);
    send_option(req, 5, "Tinte azul", cs->special_effect == 5);
    send_option(req, 6, "Sepia", cs->special_effect == 6);
    send_select_close(req);

    send_chunk(req, "<button type='submit'>Guardar ajustes de imagen</button>"
                      "</form><p id='camset-status' class='muted'></p>"
                      "<script>"
                      "document.getElementById('camset-form').addEventListener('submit',"
                      "function(ev){"
                      "ev.preventDefault();"
                      "var st=document.getElementById('camset-status');st.textContent='Guardando...';"
                      "fetch('/settings',{method:'POST',"
                      "body:new URLSearchParams(new FormData(ev.target))})"
                      ".then(function(r){return r.text().then(function(t){st.textContent=t;});})"
                      ".catch(function(e){st.textContent='Error de red: '+e;});"
                      "});"
                      "</script>");

    return send_page_close(req);
}

static int form_int(const char *body, const char *key, int def_val)
{
    char val[16];
    if (httpd_query_key_value(body, key, val, sizeof(val)) == ESP_OK) {
        return atoi(val);
    }
    return def_val;
}

static bool form_bool(const char *body, const char *key)
{
    char val[8];
    return httpd_query_key_value(body, key, val, sizeof(val)) == ESP_OK;
}

// Guarda los ajustes de imagen (persistentes en NVS) y los re-aplica al
// sensor. No requiere reinicio, a diferencia de /save (backend/token).
static esp_err_t settings_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len >= 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload invalido");
        return ESP_FAIL;
    }
    char buf[1024];
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

    camera_settings_t s = *camera_settings_get();
    s.brightness = clampi(form_int(buf, "brightness", s.brightness), -3, 3);
    s.contrast = clampi(form_int(buf, "contrast", s.contrast), -3, 3);
    s.saturation = clampi(form_int(buf, "saturation", s.saturation), -4, 4);
    s.sharpness = clampi(form_int(buf, "sharpness", s.sharpness), -3, 3);
    s.denoise = clampi(form_int(buf, "denoise", s.denoise), 0, 8);
    s.whitebal = form_bool(buf, "whitebal");
    s.wb_mode = clampi(form_int(buf, "wb_mode", s.wb_mode), 0, 4);
    s.exposure_ctrl = form_bool(buf, "exposure_ctrl");
    s.aec2 = form_bool(buf, "aec2");
    s.ae_level = clampi(form_int(buf, "ae_level", s.ae_level), -5, 5);
    s.gain_ctrl = form_bool(buf, "gain_ctrl");
    s.agc_gain = clampi(form_int(buf, "agc_gain", s.agc_gain), 0, 64);
    s.gainceiling = clampi(form_int(buf, "gainceiling", s.gainceiling), 0, 6);
    s.hmirror = form_bool(buf, "hmirror");
    s.vflip = form_bool(buf, "vflip");
    s.lenc = form_bool(buf, "lenc");
    s.bpc = form_bool(buf, "bpc");
    s.wpc = form_bool(buf, "wpc");
    s.special_effect = clampi(form_int(buf, "special_effect", s.special_effect), 0, 6);

    if (camera_settings_save(&s) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo guardar en NVS");
        return ESP_FAIL;
    }
    camera_apply_settings();

    // Texto plano: el llamador normal es el fetch() del formulario en
    // /monitor, que solo muestra esto como mensaje de estado (ver JS ahi).
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_send(req, "Guardado y aplicado.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// /settings quedó fusionada dentro de /monitor; se deja como redirect para
// no romper enlaces o bookmarks viejos.
static esp_err_t settings_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/monitor");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Dispara una captura y subida real al backend usando la config remota
// actual (la misma que usa el loop programado en app_main.c), para
// verificar en el momento que el backend recibe/procesa la imagen sin
// esperar al próximo intervalo.
static esp_err_t capture_now_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain; charset=utf-8");

    remote_config_t cfg;
    if (backend_client_fetch_config(&cfg) != ESP_OK || !cfg.valid) {
        httpd_resp_send(req, "Error: no se pudo obtener la configuracion del backend",
                          HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    const uint8_t *buf = NULL;
    size_t len = 0;
    if (camera_capture_jpeg(cfg.image_width, cfg.image_height, cfg.image_jpeg_quality,
                              cfg.image_max_kb, &buf, &len) != ESP_OK) {
        httpd_resp_send(req, "Error: fallo la captura de camara", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Copiar y soltar la camara antes de subir (misma razón que en
    // app_main.c: la subida por red no debe dejar tomado el mutex de
    // captura).
    uint8_t *copy = malloc(len);
    if (copy) {
        memcpy(copy, buf, len);
    }
    camera_release();

    char msg[160];
    if (!copy) {
        snprintf(msg, sizeof(msg), "Error: sin memoria para copiar la captura");
    } else if (backend_client_upload(copy, len, cfg.stream_id) == ESP_OK) {
        snprintf(msg, sizeof(msg), "OK: %u bytes subidos, el backend deberia procesarla en breve",
                  (unsigned)len);
        provisioning_status_set_capture(true, "forzada manualmente desde /monitor");
    } else {
        snprintf(msg, sizeof(msg), "Error: la captura se tomo pero fallo la subida al backend");
        provisioning_status_set_capture(false, "forzada manualmente, fallo la subida");
    }
    free(copy);

    httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
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
    config.max_uri_handlers = 12;
    // El default (4096) no alcanza: send_page_open() usa un buffer de 3200
    // bytes en la pila de la tarea del httpd (la CSS compartida), sumado al
    // buffer propio de cada handler (hasta ~1KB) y al overhead de snprintf.
    // Sin este aumento se pisa la pila y crashea con errores random de
    // lwIP/FreeRTOS en el primer request tras el boot.
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
    static const httpd_uri_t settings_post_uri = {
        .uri = "/settings", .method = HTTP_POST, .handler = settings_post_handler};
    static const httpd_uri_t capture_now_uri = {
        .uri = "/capture-now", .method = HTTP_POST, .handler = capture_now_post_handler};
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
    httpd_register_uri_handler(server, &settings_post_uri);
    httpd_register_uri_handler(server, &capture_now_uri);
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
