#include "lean_espnow.h"
#include "lean_crypto.h"
#include "lean_ota.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <string.h>

static const uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// send_cb / recv_cb ile setup() akisi arasinda paylasilan durum
static volatile bool     s_l2_done   = false;   // donanim ACK (send_cb) geldi
static volatile bool     s_l2_ok     = false;
static volatile bool     s_app_ack   = false;   // uygulama ACK (recv_cb) geldi
static volatile bool     s_paired    = false;   // PAIR_RESP alindi
static AckResult         s_ack;                  // recv_cb doldurur
static uint8_t           s_gw_mac[6];            // ACK gelen kaynak MAC
static bool              s_gw_known  = false;

// ---- Callbacks -------------------------------------------------------------
// Core 3.x / IDF5.5: send_cb imzasi (const wifi_tx_info_t*, status)
static void onSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  (void)info;
  s_l2_ok   = (status == ESP_NOW_SEND_SUCCESS);
  s_l2_done = true;
}

static void parse_ack_json(const char* json, int len) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return;
  const char* typ = doc["type"] | "";
  if (typ[0] == 0) typ = doc["typ"] | "";
  if (strcmp(typ, "PAIR_RESP") == 0) { s_paired = true; return; }  // eslesme onayi
  if (strcmp(typ, "ACK") != 0) return;

  uint32_t unix_v = doc["unix"] | 0UL;
  if (unix_v == 0) unix_v = doc["ts"] | 0UL;

  s_ack.got_ack     = true;
  s_ack.sleep_sec   = doc["sleep_time_sec"] | 0UL;
  s_ack.unix_time   = unix_v;
  s_ack.tz_off      = doc["off"] | 0;
  s_ack.special_cmd = doc["special_cmd"] | 0;
  s_ack.op_mode     = doc["op_mode"] | 255;
  s_app_ack = true;
}

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  // Kaynak MAC'i gateway olarak kaydet
  memcpy(s_gw_mac, info->src_addr, 6);
  s_gw_known = true;

#if ENABLE_ENCRYPTION
  // Sifreli cerceve -> plaintext. IV icin gonderen (gateway) MAC'i kullanilir.
  static uint8_t plain[512];
  uint32_t rn; uint16_t rbc;
  size_t pn = crypto_decrypt(data, len, info->src_addr, plain, sizeof(plain) - 1, &rn, &rbc);
  if (pn > 0) {
#if ENABLE_OTA
    if (ota_is_packet(plain, pn)) { ota_handle(plain, pn); return; }   // OTA binary
#endif
    plain[pn] = 0; parse_ack_json((const char*)plain, pn); return;     // ACK JSON
  }
  // decrypt basarisiz -> belki duz JSON (lab) gelmistir, dene
#endif
#if ENABLE_OTA
  if (ota_is_packet(data, len)) { ota_handle(data, len); return; }
#endif
  if (len > 0 && (data[0] == '{')) parse_ack_json((const char*)data, len);
}

// ---- Radyo -----------------------------------------------------------------
bool espnow_begin() {
  crypto_init();
  WiFi.mode(WIFI_STA);                 // <-- brownout tanisi "step1c" noktasi
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) { DEBUG_PRINTLN("[ESPNOW] init FAIL"); return false; }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);

  // Hedef peer: sabit GATEWAY_MAC verildiyse unicast, aksi halde broadcast (discovery).
  uint8_t gw[6] = GATEWAY_MAC;
  bool have_gw = false;
  for (int i = 0; i < 6; i++) if (gw[i] != 0) have_gw = true;

  esp_now_peer_info_t peer = {};
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;                // HW sifreleme kapali; AES-GCM uygulama katmaninda
  memcpy(peer.peer_addr, have_gw ? gw : BCAST, 6);
  if (esp_now_add_peer(&peer) != ESP_OK) { DEBUG_PRINTLN("[ESPNOW] add_peer FAIL"); return false; }
  memcpy(s_gw_mac, have_gw ? gw : BCAST, 6);
  s_gw_known = have_gw;

  DEBUG_PRINT("[ESPNOW] up ch="); DEBUG_PRINT(ESPNOW_CHANNEL);
  DEBUG_PRINTLN(have_gw ? " (unicast)" : " (broadcast discovery)");
  return true;
}

// Bir cerceve gonder (varsa unicast gateway'e, yoksa broadcast). L2 ACK bekle.
static bool send_frame(const uint8_t* frame, size_t n) {
  s_l2_done = false; s_l2_ok = false;
  const uint8_t* dest = s_gw_known ? s_gw_mac : BCAST;
  if (esp_now_send(dest, frame, n) != ESP_OK) return false;
  uint32_t t0 = millis();
  while (!s_l2_done && (millis() - t0) < ESPNOW_L2_ACK_TIMEOUT_MS) delay(2);
  return s_l2_done && s_l2_ok;
}

static size_t build_and_encrypt(const SensorDataMessage& msg,
                                uint16_t boot_cnt, uint32_t nonce,
                                uint8_t* out, size_t cap) {
#if ENABLE_ENCRYPTION
  return crypto_encrypt((const uint8_t*)&msg, sizeof(msg), boot_cnt, nonce, out, cap);
#else
  if (cap < sizeof(msg)) return 0;
  memcpy(out, &msg, sizeof(msg));
  return sizeof(msg);
#endif
}

bool espnow_send_record(const SensorRecord& rec, uint16_t boot_cnt,
                        uint32_t pkt, AckResult* ack_out) {
  SensorDataMessage msg = {};
  msg.msg_type         = MSG_SENSOR_DATA;
  msg.protocol_version = PROTOCOL_VERSION;
  msg.packet_counter   = pkt;
  msg.project_id_hash  = PROJECT_ID_HASH;
  msg.timestamp        = rec.timestamp;
  msg.count            = 4;
  msg.param_ids[0] = PARAM_TEMP;   msg.values[0] = rec.temp;
  msg.param_ids[1] = PARAM_HUM;    msg.values[1] = rec.hum;
  msg.param_ids[2] = PARAM_BATT;   msg.values[2] = (float)rec.batt_mv;
  msg.param_ids[3] = PARAM_STATUS; msg.values[3] = (float)rec.status;
  msg.rssi = 0; msg.channel = ESPNOW_CHANNEL;
  strncpy(msg.version, FW_VERSION, sizeof(msg.version) - 1);

  uint8_t frame[GCM_HDR_LEN + GCM_TAG_LEN + sizeof(SensorDataMessage) + 4];
  size_t n = build_and_encrypt(msg, boot_cnt, pkt, frame, sizeof(frame));
  if (n == 0) { DEBUG_PRINTLN("[ESPNOW] encrypt FAIL"); return false; }

  // uygulama ACK durumunu sifirla
  s_app_ack = false; memset(&s_ack, 0, sizeof(s_ack));

  bool l2 = false;
  for (int r = 0; r <= ESPNOW_SEND_RETRIES && !l2; r++) {
    l2 = send_frame(frame, n);
    if (!l2) { DEBUG_PRINT("[ESPNOW] L2 retry "); DEBUG_PRINTLN(r); delay(30); }
  }
  if (!l2) { DEBUG_PRINTLN("[ESPNOW] L2 ACK YOK"); return false; }

  // Uygulama ACK'i (JSON) icin kisa pencere bekle
  uint32_t t0 = millis();
  while (!s_app_ack && (millis() - t0) < ESPNOW_APP_ACK_TIMEOUT_MS) delay(2);

  if (ack_out) *ack_out = s_ack;
  DEBUG_PRINT("[ESPNOW] gonderildi (L2 ok, appACK=");
  DEBUG_PRINT(s_app_ack ? "1" : "0"); DEBUG_PRINTLN(")");
  return true;   // L2 ACK = gateway aldi; uygulama ACK opsiyonel
}

bool espnow_send_bc(const char* uid, uint16_t boot_cnt, uint32_t nonce) {
  StaticJsonDocument<384> doc;
  doc["typ"] = "BC";
  doc["uid"] = uid;
  doc["fv"]  = FW_VERSION;
  doc["hr"]  = HARDWARE_MODEL;
  JsonObject tr = doc.createNestedObject("trace");
  tr["cal_cert"] = "REMOTE_SYNC";
  tr["t90_ref"]  = 150;
  tr["acc"]      = "0.3C-A";

  char json[384];
  size_t jn = serializeJson(doc, json, sizeof(json));

  uint8_t frame[GCM_HDR_LEN + GCM_TAG_LEN + 384];
  size_t n;
#if ENABLE_ENCRYPTION
  n = crypto_encrypt((const uint8_t*)json, jn, boot_cnt, nonce, frame, sizeof(frame));
#else
  memcpy(frame, json, jn); n = jn;
#endif
  if (n == 0 || n > 250) { DEBUG_PRINTLN("[ESPNOW] BC cerceve>250B (chunk gerekli, LEAN atliyor)"); return false; }
  bool ok = send_frame(frame, n);
  DEBUG_PRINT("[ESPNOW] BC gonderim="); DEBUG_PRINTLN(ok ? "OK" : "FAIL");
  return ok;
}

bool espnow_is_paired() { return s_paired; }

// PAIR_REQ gonderir (gateway allowlist'e eklesin diye). Gateway pmk_fpr+pid_h+pmk_ver
// dogrulayinca PAIR_RESP yollar ve MAC'i allowlist'e ekler. PAIR_RESP gelmese bile
// PAIR_REQ ulastiysa gateway allowlist'ler; yine de kisa sure bekleyip teyit arariz.
bool espnow_pair(const char* uid, uint16_t boot_cnt, uint32_t nonce) {
  StaticJsonDocument<256> doc;
  doc["type"]    = "PAIR_REQ";
  doc["pid"]     = PROJECT_ID;
  doc["pid_h"]   = (uint32_t)PROJECT_ID_HASH;
  doc["pmk_ver"] = ESPNOW_PMK_DERIVATION_VERSION;
  doc["pmk_fpr"] = crypto_pmk_fpr8();
  doc["uid"]     = uid;
  doc["challenge"] = (uint32_t)esp_random();
  char json[256];
  size_t jn = serializeJson(doc, json, sizeof(json));

  uint8_t frame[GCM_HDR_LEN + GCM_TAG_LEN + 256];
  size_t n;
#if ENABLE_ENCRYPTION
  n = crypto_encrypt((const uint8_t*)json, jn, boot_cnt, nonce, frame, sizeof(frame));
#else
  memcpy(frame, json, jn); n = jn;
#endif
  if (n == 0) return false;

  s_paired = false;
  for (int r = 0; r < 2 && !s_paired; r++) {
    send_frame(frame, n);
    uint32_t t0 = millis();
    while (!s_paired && (millis() - t0) < 700) delay(20);
  }
  DEBUG_PRINT("[PAIR] PAIR_REQ gonderildi, resp="); DEBUG_PRINTLN(s_paired ? "OK" : "yok(grace)");
  return s_paired;
}

#if ENABLE_OTA
// send+ACK sonrasi radyo aciken cagirilir. Kisa pencerede BEGIN gelirse OTA
// dongusune girer; END basariliysa cihaz yeniden baslar (bu fonksiyon donmez).
// BEGIN gelmezse veya idle timeout olursa doner -> normal akis (uyku) devam.
void espnow_ota_listen() {
  uint32_t t0 = millis();
  while (!ota_active() && (millis() - t0) < OTA_LISTEN_WINDOW_MS) delay(10);
  if (!ota_active()) return;                       // OTA yok

  DEBUG_PRINTLN("[OTA] pencere: firmware aliniyor...");
  while (ota_active()) {
    delay(5);
    if ((millis() - ota_last_ms()) > OTA_IDLE_TIMEOUT_MS) {
      DEBUG_PRINTLN("[OTA] idle timeout -> abort");
      ota_abort();
      break;
    }
  }
}
#endif

void espnow_end() {
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  DEBUG_PRINTLN("[ESPNOW] kapatildi");
}
