// Termostat kontrol motoru senaryo testleri.
// termostat.yaml'daki lambda'lar run_engine_tests.py tarafından *.inc dosyalarına çıkarılır
// ve burada sahte ESPHome bileşenleriyle derlenir.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "termostat_helpers.h"

static bool g_verbose = false;
#define ESP_LOGI(tag, ...) do { if (g_verbose) { printf("    [I] " __VA_ARGS__); printf("\n"); } } while (0)
#define ESP_LOGW(tag, ...) do { if (g_verbose) { printf("    [W] " __VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------------------
// Sahte ESPHome ortamı
// ---------------------------------------------------------------------------
static uint32_t g_ms = 0;
uint32_t millis() { return g_ms; }
#define id(x) (x)

template<class T> struct Entity {
  T state{};
  bool has = false;
  bool has_state() const { return has; }
  void publish_state(T v) { state = v; has = true; }
};
struct Number { float state; };
struct Select { std::string state; };
struct Time {
  bool valid;
  int hour;
  bool is_valid() const { return valid; }
};
struct Clock {
  Time t;
  Time now() const { return t; }
};

uint32_t last_sensor_seen = 0, last_relay_on_time = 0, last_relay_off_time = 0;
bool sensor_ok = false, motor_hazir = true, donma_acil = false, sensor_stok = false;
float son_ham_sicaklik = NAN;
Entity<std::string> calisma_durumu, aktif_program_dilimi, sensor_tipi;
Entity<float> aktif_hedef_temp, room_temp;
Entity<bool> sensor_timeout_alert;
Select termostat_modu{"Manuel Sabit Hedef"};
Number donma_koruma_sicakligi{10}, manuel_hedef_sicaklik{22}, histerezis_toleransi{0.5},
    min_calisma_suresi{3}, min_bekleme_suresi{3}, sabah_saat{6}, sabah_hedef{23},
    gunduz_saat{8}, gunduz_hedef{18}, aksam_saat{17}, aksam_hedef{23.5}, gece_saat{23}, gece_hedef{19};
Clock my_time{{true, 12}};

struct Relay {
  bool state = false;
  void turn_on() {
    if (state)
      return;
    state = true;
#include "on.inc"
  }
  void turn_off() {
    if (!state)
      return;
    state = false;
#include "off.inc"
  }
} kombi_role;

void kontrol() {
#include "kontrol.inc"
}
void veri(float t, bool stok) {
#include "veri.inc"
}

// ---------------------------------------------------------------------------
// Test yardımcıları
// ---------------------------------------------------------------------------
static int g_fail = 0, g_total = 0;
static void expect(bool cond, const char *msg) {
  g_total++;
  if (!cond)
    g_fail++;
  printf("  %s %s  [röle=%s, durum='%s']\n", cond ? "✓" : "✗ HATA:", msg, kombi_role.state ? "AÇIK" : "kapalı",
         calisma_durumu.state.c_str());
}
static bool starts(const std::string &s, const char *p) { return s.rfind(p, 0) == 0; }

// Zamanı saniye saniye ilerletir: 10 sn'de bir kontrol, `every` sn'de bir sensör verisi.
static void run(uint32_t sec, float temp, uint32_t every = 5, bool stok = false) {
  for (uint32_t s = 0; s < sec; s++) {
    g_ms += 1000;
    if (!std::isnan(temp) && every && (g_ms / 1000) % every == 0)
      veri(temp, stok);
    if ((g_ms / 1000) % 10 == 0)
      kontrol();
  }
}

int main(int argc, char **argv) {
  g_verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
  // on_boot (öncelik 600) davranışı
  last_relay_off_time = millis();
  last_sensor_seen = millis();

  puts("1) Açılış, sensör verisi yok");
  run(60, NAN);
  expect(!kombi_role.state && calisma_durumu.state == "Sensör verisi bekleniyor...", "veri beklenirken röle kapalı");
  run(20 * 60, NAN);
  expect(sensor_timeout_alert.state, "açılıştan 20 dk sonra hâlâ veri yoksa alarm");

  puts("2) PVVX verisi geliyor (20°C, hedef 22°C)");
  run(20, 20.0);
  expect(kombi_role.state, "ısıtma başladı");
  expect(!sensor_timeout_alert.state, "alarm temizlendi");
  expect(sensor_tipi.state == "PVVX", "sensör tipi PVVX algılandı");

  puts("3) Hedef aşıldı ama minimum çalışma süresi dolmadı");
  run(30, 22.6);
  expect(kombi_role.state && starts(calisma_durumu.state, "Min"), "minimum çalışma koruması");
  run(180, 22.6);
  expect(!kombi_role.state, "minimum çalışma sonrası kapandı");

  puts("4) Sıcaklık hemen düştü");
  run(30, 21.0);
  expect(!kombi_role.state && starts(calisma_durumu.state, "Dinlenme"), "dinlenme koruması");
  run(180, 21.0);
  expect(kombi_role.state, "dinlenme sonrası yandı");

  puts("5) Sıcaklık 40 dk hiç değişmiyor");
  run(40 * 60, 21.6);
  expect(!sensor_timeout_alert.state, "sabit sıcaklıkta yanlış alarm yok");

  puts("6) Isıtırken 'Kapalı' moduna geçiş");
  run(300, 20.0);
  expect(kombi_role.state, "ısıtıyor");
  termostat_modu.state = "Kapalı";
  run(10, 20.0);
  expect(!kombi_role.state && calisma_durumu.state == "Kapalı", "minimum çalışma beklenmeden kapandı");

  puts("7) 'Kapalı' modda acil donma koruması");
  run(200, 4.0);
  expect(kombi_role.state && starts(calisma_durumu.state, "Acil"), "5°C altında devreye girdi");
  run(200, 6.0);
  expect(kombi_role.state, "5-7°C arasında devam ediyor");
  run(20, 7.1);
  expect(!kombi_role.state, "7°C'de kapandı");

  puts("8) Isıtırken PVVX sensörü kayboldu");
  termostat_modu.state = "Manuel Sabit Hedef";
  run(300, 20.0);
  expect(kombi_role.state, "ısıtıyor");
  run(14 * 60, NAN);
  expect(kombi_role.state, "14. dakikada hâlâ çalışıyor");
  run(80, NAN);
  expect(!kombi_role.state && sensor_timeout_alert.state, "15 dk sonra emniyet kapatması + alarm");
  expect(std::isnan(room_temp.state), "oda sıcaklığı 'bilinmiyor' olarak yayınlandı");

  puts("9) Röle elle kapatıldı");
  run(210, 20.0);
  expect(kombi_role.state, "tekrar ısıtıyor");
  kombi_role.turn_off();
  run(30, 20.0);
  expect(!kombi_role.state, "manuel kapatmadan sonra dinlenme süresi bekleniyor");
  run(180, 20.0);
  expect(kombi_role.state, "dinlenme sonrası yandı");

  puts("10) Zaman dilimi programı");
  termostat_modu.state = "Zaman Dilimi Programı (Otomatik)";
  my_time.t = {false, 0};
  run(10, 20.0);
  expect(aktif_hedef_temp.state == 22.0f, "saat senkronize değilken manuel hedef kullanılır");
  my_time.t = {true, 3};
  run(10, 20.0);
  expect(aktif_hedef_temp.state == 19.0f, "03:00 -> önceki günün gece dilimi");
  my_time.t = {true, 7};
  run(10, 20.0);
  expect(aktif_hedef_temp.state == 23.0f, "07:00 -> sabah dilimi");
  my_time.t = {true, 23};
  run(10, 20.0);
  expect(aktif_hedef_temp.state == 19.0f, "23:00 -> gece dilimi");

  puts("11) Geçersiz sensör okuması");
  float onceki = son_ham_sicaklik;
  veri(150.0f, false);
  expect(son_ham_sicaklik == onceki, "150°C okuması yok sayıldı");

  puts("12) Orijinal Xiaomi firmware (~10 dk'da bir veri)");
  termostat_modu.state = "Manuel Sabit Hedef";
  veri(20.0f, true);  // son veri tam bu anda
  kontrol();
  expect(sensor_tipi.state == "Orijinal Xiaomi (BindKey)", "sensör tipi orijinal algılandı");
  run(30 * 60, NAN);
  expect(!sensor_timeout_alert.state, "30 dk veri yokken alarm yok (35 dk sınırı)");
  run(6 * 60, NAN);
  expect(sensor_timeout_alert.state && !kombi_role.state, "35 dk sonra emniyet kapatması");

  puts("13) MAC / BindKey doğrulama");
  uint64_t mac = 0;
  expect(termostat_parse_mac("A4:C1:38:C5:0D:5D", mac) && mac == 0xA4C138C50D5DULL, "geçerli MAC");
  expect(termostat_parse_mac("a4:c1:38:c5:0d:5d", mac), "küçük harf MAC");
  expect(!termostat_parse_mac("A4:C1:38:C5:0D:5D:00", mac), "fazla uzun MAC reddedildi");
  expect(!termostat_parse_mac("A4-C1-38-C5-0D-5D", mac), "yanlış ayraç reddedildi");
  expect(!termostat_parse_mac("ZZ:C1:38:C5:0D:5D", mac), "hex olmayan karakter reddedildi");
  expect(!termostat_parse_mac("00:00:00:00:00:00", mac), "sıfır MAC reddedildi");
  expect(termostat_valid_bindkey("0123456789abcdef0123456789ABCDEF"), "geçerli BindKey");
  expect(!termostat_valid_bindkey("0123456789abcdef"), "kısa BindKey reddedildi");
  expect(!termostat_valid_bindkey("0123456789abcdef0123456789abcdeg"), "hex olmayan BindKey reddedildi");

  printf("\n%s: %d/%d test geçti\n", g_fail ? "BAŞARISIZ" : "BAŞARILI", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
