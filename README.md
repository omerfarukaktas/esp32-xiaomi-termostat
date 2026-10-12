<div align="center">

# ESP32 Xiaomi Termostat

**Xiaomi Bluetooth sıcaklık sensörüyle çalışan, kararlarını kendi veren, Home Assistant ile yönetilen kablosuz kombi oda termostatı.**

[![Derleme](https://github.com/omerfarukaktas/esp32-xiaomi-termostat/actions/workflows/build.yml/badge.svg)](https://github.com/omerfarukaktas/esp32-xiaomi-termostat/actions/workflows/build.yml)
[![Sürüm](https://img.shields.io/github/v/release/omerfarukaktas/esp32-xiaomi-termostat)](https://github.com/omerfarukaktas/esp32-xiaomi-termostat/releases/latest)
[![Lisans: GPL-3.0](https://img.shields.io/badge/lisans-GPL--3.0-blue.svg)](LICENSE)
[![ESPHome](https://img.shields.io/badge/ESPHome-2025.9-000?logo=esphome)](https://esphome.io)

</div>

Xiaomi sıcaklık sensörünü kablosuz oda termostatına dönüştürün!

Termostat mantıgı Esp32 üzerinde çalışır, Home Assistant serveri kapansa bile termostat mantıgı çalışmaya devam edebilir.

Hazır firmware'i tarayıcıdan yükleyip Wi-Fi'a captive portal üzerinden bağlanarak kolay kurulum; geri kalan her şey Home Assistant arayüzünden ayarlanır.

## İçindekiler

- [Özellikler](#özellikler)
- [Gerekenler](#gerekenler)
- [Bağlantı](#bağlantı)
- [Kurulum](#kurulum)
- [Nasıl Çalışır](#nasıl-çalışır)
- [Home Assistant Varlıkları](#home-assistant-varlıkları)
- [Pano Kartı](#pano-kartı)
- [Buton ve LED](#buton-ve-led)
- [Güncelleme ve Güvenlik](#güncelleme-ve-güvenlik)
- [Sorun Giderme](#sorun-giderme)
- [Geliştirme](#geliştirme)
- [Lisans](#lisans)

## Özellikler

- **Otonom çalışma:** Termostat mantığı tamamen ESP32'de çalışır. Wi-Fi koparsa Bluetooth okuması sürer. Home Assistant suncuusu kapalı olduğunda dahi cihaz programa göre ısıtmaya devam eder.
- **Tek firmware, iki sensör tipi:** [PVVX](https://github.com/pvvx/ATC_MiThermometer) firmware'li veya orijinal Xiaomi firmware'li sensörler desteklenir. Sensör tipi otomatik algılanır.
- **4 çalışma modu:** 4 dilimli zaman programı, manuel hedef (HA otomasyonlarından da ayarlanabilir), donma koruması (tatil) ve kapalı (acil donma korumalı).
- **Kombi koruması:** Minimum çalışma ve minimum bekleme süreleri kombinin kısa aralıklarla yanıp sönmesini engeller.
- **Sensör emniyeti:** Sensörden veri kesilirse kombi kapatılır ve Home Assistant'a uyarı gönderilir.
- **Şifreli bağlantı:** Home Assistant API'si şifrelidir. Anahtarı Home Assistant oluşturup cihaza kaydeder.

## Gerekenler

| Parça | Not |
|---|---|
| ESP32 geliştirme kartı (4 MB flash) | ESP32-WROOM-32 tabanlı `esp32dev` kartlar. Yerleşik BOOT butonu ve LED kullanılır; çıplak modülde harici buton ve LED gerekir. |
| 1 kanallı röle modülü (3.3 V tetiklenebilir) | Kombinin oda termostatı girişine kuru kontak olarak bağlanır. |
| Xiaomi Mijia termometre | LYWSD03MMC önerilir. Diğer modeller yalnızca PVVX firmware ile çalışır (bkz. [Desteklenen sensörler](#desteklenen-sensörler)). |
| 5 V güç kaynağı | USB adaptör yeterlidir. |
| Home Assistant | ESPHome entegrasyonu ile (2025.9 veya üzeri önerilir). |

## Bağlantı

| ESP32 | Bağlantı |
|---|---|
| `GPIO13` | Röle modülü `IN` |
| `5V` / `GND` | Röle modülü `VCC` / `GND` |
| Röle `COM` + `NO` | Kombinin oda termostatı klemensi (çoğu kombide düşük voltajlı kuru kontak) |
| `GPIO0` | Yerleşik BOOT butonu (harici bağlantı yok) |
| `GPIO2` | Yerleşik LED (harici bağlantı yok) |

<details>
<summary>Geliştirme kartı yerine çıplak ESP32 modülü (ESP32-WROOM-32) kullanıyorsanız</summary>

Buton ve LED kartta olmadığından harici olarak bağlanmalıdır:

| ESP32 | Bağlantı |
|---|---|
| `GPIO0` | Buton: bir ucu `GPIO0`, diğer ucu `GND`. `GPIO0` ile `3.3V` arasına 10 kΩ pull-up direnci önerilir. |
| `GPIO2` | LED: `GPIO2` → 330 Ω–1 kΩ direnç → LED anot (+), LED katot (−) → `GND`. |

- `GPIO0` ve `GPIO2` açılış (strapping) pinleridir. Bu bağlantılar geliştirme kartlarındakiyle aynıdır ve normal açılışı bozmaz. Ancak butona basılıyken cihaza enerji verilirse ESP32 firmware yükleme moduna girer.
- `GPIO2`'ye LED'i `GND` yerine `3.3V` tarafına bağlamayın; hem LED ters çalışır hem de açılış etkilenebilir.
- Firmware'i ilk kez yüklemek için 3.3 V bir USB-UART dönüştürücü gerekir (`TX`→`RX`, `RX`→`TX`, `GND`→`GND`). Yükleme sırasında butonu basılı tutarak enerji verin (veya `EN`'i resetleyin).

</details>

> [!WARNING]
> Kombinin bağlantı şemasını mutlaka kombi kılavuzundan kontrol edin. Termostat klemensi 230 V taşıyorsa işlemi yetkili bir servise yaptırın. Röle, açılışta ve her türlü hata durumunda **kapalı** (kombi kapalı) konuma geçer.

## Kurulum

### 1. Sensörü hazırlayın

İki seçenek vardır:

- **PVVX firmware (önerilen):** Sensör birkaç saniyede bir veri gönderir, şifre gerekmez. Chrome veya Edge'de [Telink Flasher](https://pvvx.github.io/ATC_MiThermometer/TelinkFlsh.html) sayfasını açın, sensöre bağlanın ve *Custom Firmware* ile yükleyin. Reklam (advertising) türü **PVVX (Custom)** olmalıdır.
- **Orijinal firmware:** Sensörün yazılımı değişmez, ancak veri ~10 dakikada bir gelir ve 32 karakterlik **BindKey** gerekir. BindKey'i [Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor) ile alabilirsiniz.

Her iki durumda da sensörün **MAC adresini** (örn. `A4:C1:38:12:34:56`) not edin.

#### Desteklenen sensörler

Destek modele değil, sensördeki firmware'e bağlıdır:

| Firmware | Desteklenen modeller |
|---|---|
| PVVX (*Custom* reklam türü) | PVVX firmware'in yüklenebildiği tüm modeller: LYWSD03MMC, MHO-C401, CGG1, CGDK2 vb. ([tam liste](https://github.com/pvvx/ATC_MiThermometer#supported-models)) |
| Orijinal Xiaomi | Yalnızca **LYWSD03MMC** |

Orijinal firmware'li diğer modeller (yuvarlak LYWSDCGQ, MHO-C401, CGG1 vb.) farklı bir veri formatı kullandığından desteklenmez; bu modelleri kullanmak için PVVX firmware yükleyin.

### 2. Firmware'i yükleyin

1. [Son sürüm](https://github.com/omerfarukaktas/esp32-xiaomi-termostat/releases/latest) sayfasından `esp32-xiaomi-termostat.factory.bin` dosyasını indirin.
2. ESP32'yi USB ile bilgisayara bağlayın.
3. Chrome veya Edge'de [ESPHome Web](https://web.esphome.io) sayfasını açın, **Connect** ile kartı seçin ve **Install** ile indirdiğiniz dosyayı yükleyin.
   ESPConnect gibi başka bir web flasher kullanıyorsanız dosyayı **`0x0`** adresine yazın; kartta önceden başka bir yazılım varsa önce flash'ı silin (*Erase*).

<details>
<summary>Komut satırıyla (esptool)</summary>

```bash
esptool.py --chip esp32 erase_flash
esptool.py --chip esp32 write_flash 0x0 esp32-xiaomi-termostat.factory.bin
```
</details>

### 3. Wi-Fi'a bağlayın

1. Cihaz kayıtlı bir ağ bulamazsa **`Termostat-Kurulum`** adlı açık bir Wi-Fi ağı yayınlar.
2. Telefonunuzla bu ağa bağlanın; kurulum sayfası kendiliğinden açılır.
3. Ev ağınızı seçip şifresini girin. Cihaz ağa bağlanır ve kurulum ağı kapanır.

USB üzerinden de kurulum yapılabilir: ESPHome Web sayfasındaki *Connect → Configure Wi-Fi* adımı (Improv Serial) aynı işi yapar.

### 4. Home Assistant'a ekleyin

Home Assistant cihazı otomatik bulur: **Ayarlar → Cihazlar ve Hizmetler → Keşfedildi → ESP32 Xiaomi Termostat → Yapılandır**. Görünmezse **Entegrasyon Ekle → ESPHome** ile cihazın IP adresini girin (port `6053`).

Home Assistant ekleme sırasında bir şifreleme anahtarı oluşturup cihaza kaydeder. Bu andan sonra cihaz yalnızca şifreli bağlantı kabul eder.

### 5. İlk ayarlar

Cihaz sayfasındaki **Yapılandırma** bölümünden:

1. **Xiaomi Sensör MAC Adresi:** Sensörünüzün MAC adresini girin.
2. **Xiaomi BindKey:** Yalnızca orijinal firmware kullanıyorsanız girin.
3. **OTA Güncelleme Şifresi:** En az 8 karakterlik bir şifre belirleyin (bkz. [Güvenlik](#güncelleme-ve-güvenlik)).
4. **Termostat Çalışma Modu:** Modu ve hedef sıcaklıkları ayarlayın.

Birkaç saniye (orijinal firmware'de en geç ~10 dakika) içinde **Oda Sıcaklığı** görünür ve **Sensör Tipi** algılanır. Hazır bir pano için [Pano Kartı](#pano-kartı) bölümüne bakın.

## Nasıl Çalışır

Termostat motoru 10 saniyede bir ve ayarlar değiştiği anda çalışır.

### Çalışma modları

| Mod | Hedef sıcaklık |
|---|---|
| **Zaman Dilimi Programı (Otomatik)** | O anki saate göre Sabah / Gündüz / Akşam / Gece dilimlerinden birinin hedefi. Saat henüz alınamadıysa *Manuel Hedef Sıcaklık*. |
| **Manuel Sabit Hedef** | *Manuel Hedef Sıcaklık*. Elle veya HA otomasyonlarından (`set_target_temperature` eylemi) ayarlanır. Son değer cihazda saklanır. |
| **Donma Koruması** | *Donma Koruma Sıcaklığı* (tatil modu) |
| **Kapalı** | Kombi hemen kapanır. Oda 5 °C'nin altına düşerse acil donma koruması kombiyi yakar, 7 °C'de söndürür. |

Zaman programında her dilim kendi başlangıç saatinden bir sonraki dilime kadar geçerlidir. Gece yarısını aşan dilim, ertesi günün ilk diliminin başlangıcına kadar sürer. Saat önce internetten (SNTP), olmazsa Home Assistant'tan alınır. Saat dilimi Home Assistant'ın saat diliminden (*Ayarlar > Sistem > Genel*) otomatik alınır ve cihazda saklanır; böylece HA veya internet olmadan yeniden başlasa da doğru dilimle çalışır. Cihaz HA'ya hiç bağlanmadıysa varsayılan `Europe/Istanbul` kullanılır.

### Isıtma kararı ve kombi koruması

- **Histerezis:** Kombi `hedef − histerezis` altında yanar, `hedef + histerezis` üstünde söner.
- **Minimum çalışma süresi:** Yanan kombi bu süre dolmadan söndürülmez.
- **Minimum bekleme süresi:** Sönen kombi bu süre dolmadan yeniden yakılmaz. Cihaz açıldığında da bu süre beklenir; elektrik dalgalanmalarında kombi art arda yakılmaz.
- Koruma, röle hangi yoldan değişirse değişsin geçerlidir: termostat, Home Assistant veya BOOT butonu.

### Sensör emniyeti

Belirli bir süre geçerli sıcaklık verisi gelmezse kombi minimum çalışma süresi beklenmeden kapatılır, **Sensör Bağlantı Uyarısı** açılır ve **Oda Sıcaklığı** "bilinmiyor" olur. Veri yeniden gelince her şey kendiliğinden normale döner.

| Durum | Süre |
|---|---|
| PVVX sensör | 15 dk |
| Orijinal firmware sensör | 35 dk |
| Açılıştan beri hiç veri gelmedi (yanlış MAC, sensör yok) | 20 dk |

−30 °C altı ve 70 °C üstü okumalar hatalı kabul edilip yok sayılır.

## Home Assistant Varlıkları

<details>
<summary><b>Kontrol ve ayarlar</b></summary>

| Varlık | Aralık / Varsayılan |
|---|---|
| Termostat Çalışma Modu | 4 mod (bkz. yukarı) |
| Sabah / Gündüz / Akşam / Gece Başlangıç Saati | 0–23 · 6 / 8 / 17 / 23 |
| Sabah / Gündüz / Akşam / Gece Hedef Sıcaklığı | 12–30 °C · 23 / 18 / 23.5 / 19 |
| Manuel Hedef Sıcaklık | 10–30 °C · 22 |
| Donma Koruma Sıcaklığı | 5–15 °C · 10 |
| Histerezis Toleransı | 0.1–2.0 °C · 0.5 |
| Kombi Minimum Çalışma / Bekleme Süresi | 1–15 dk · 3 / 3 |
| Xiaomi Sensör MAC Adresi | `AA:BB:CC:DD:EE:FF` |
| Xiaomi BindKey | 32 hex karakter, yalnızca yazılabilir |
| OTA Güncelleme Şifresi | en az 8 karakter, yalnızca yazılabilir |
| Kombi Rölesi | Test amaçlı elle kumanda; koruma sürelerine uymayan istek reddedilir, termostat kontrolü sonra kendi kararını uygular |

Tüm ayarlar cihazın flash belleğinde saklanır ve yeniden başlatmada korunur.
</details>

<details>
<summary><b>Durum ve teşhis</b></summary>

| Varlık | Açıklama |
|---|---|
| Oda Sıcaklığı | Sensörden gelen sıcaklık |
| Oda Nem, Xiaomi Sensör Pil, Xiaomi Sinyal Seviyesi | Sensör bilgileri (sinyal yalnızca PVVX) |
| Aktif Hedef Sıcaklık | Termostatın o anki hedefi |
| Aktif Program Dilimi | Örn. `Akşam (Konfor) -> Hedef: 23.5°C` |
| Termostat Çalışma Durumu | Örn. `Isıtıyor (Kombi Devrede)`, `Dinlenme Koruması (Bekleniyor: 75 sn)` |
| Sensör Bağlantı Uyarısı | Sensör verisi kesildiğinde açılır |
| Sensör Son Veri | Son geçerli verinin kaç saniye önce geldiği |
| Sensör Tipi | `PVVX` veya `Orijinal Xiaomi (BindKey)` |
| WiFi Sinyal, Çalışma Süresi, Bos RAM, ESPHome Sürümü | Cihaz teşhisi |
</details>

<details>
<summary><b>API eylemleri (otomasyonlar için)</b></summary>

| Eylem | Açıklama |
|---|---|
| `esphome.esp32_xiaomi_termostat_set_target_temperature` | `target`: Manuel hedefi ayarlar ve "Manuel Sabit Hedef" moduna geçer (10–30 °C'ye sınırlanır) |
| `esphome.esp32_xiaomi_termostat_set_hysteresis` | `h`: Histerezis (0.1–2.0 °C) |
| `esphome.esp32_xiaomi_termostat_set_xiaomi_mac` | `mac`: Sensör MAC adresi (doğrulanır) |

Örnek: dışarı çıkınca hedefi düşürmek

```yaml
action: esphome.esp32_xiaomi_termostat_set_target_temperature
data:
  target: 17
```
</details>

## Pano Kartı

[`docs/lovelace-kart.yaml`](docs/lovelace-kart.yaml), termostatın tüm günlük kullanımını tek sayfada toplayan hazır bir Home Assistant panosudur. Dört karttan oluşur:

| Kart | İçerik |
|---|---|
| **Kombi Termostat** | Oda sıcaklığı, aktif hedef, nem ve kombi durumu; altında son 24 saatin grafiği (oda sıcaklığı, hedef ve kombinin çalıştığı aralıklar). |
| **Çalışma** | Çalışma modu, aktif program dilimi, durum, Manuel hedef, donma koruma sıcaklığı ve sensör bağlantı uyarısı. |
| **Zaman Programı** | Sabah / Gündüz / Akşam / Gece dilimlerinin başlangıç saati ve hedef sıcaklığı, dilim başlıklarıyla gruplanmış. |
| **Ayarlar ve Teşhis** | Histerezis, kombi minimum çalışma / bekleme süreleri, sensör pili ve sinyali, son veri süresi, sensör tipi, WiFi sinyali ve çalışma süresi. |

**Ekleme:**

1. **Ayarlar → Panolar → Pano Ekle → Sıfırdan yeni pano** seçin, adını `Termostat` koyun. Pano kenar çubuğunda görünür.
2. Panoyu açın, sağ üstteki kalem simgesiyle düzenleme moduna geçin.
3. **⋮ → Ham yapılandırma düzenleyici** seçin, içeriği silip [`docs/lovelace-kart.yaml`](docs/lovelace-kart.yaml) dosyasının tamamını yapıştırın ve **Kaydet**'e basın.

Mevcut bir panoya eklemek isterseniz dosyadaki `cards:` altındaki dört kartı **Kart Ekle → Manuel** ile tek tek yapıştırabilirsiniz.

> [!NOTE]
> Dosyadaki varlık adları (`sensor.esp32_xiaomi_termostat_oda_sicakligi` gibi), cihaz Home Assistant'a **ESP32 Xiaomi Termostat** adıyla ilk kez eklendiğinde oluşan adlardır. Cihazı daha önce başka bir adla eklediyseniz veya bir alana (oda) atadıysanız adlar farklı olabilir; kartta "Varlık bulunamadı" görürseniz **Ayarlar → Varlıklar** sayfasından gerçek adları bulup dosyadaki `esp32_xiaomi_termostat_` önekini kendi önekinizle değiştirin.

> [!TIP]
> **Kombi** satırına dokunmak (veya BOOT butonuna kısa basmak) röleyi elle açıp kapatır. Bu yalnızca test içindir:
> - Minimum bekleme süresi dolmadan, sensör verisi yokken veya *Kapalı* modda açma isteği; minimum çalışma süresi dolmadan kapatma isteği **reddedilir** (anahtar eski durumuna döner, sebebi loglanır).
> - Kabul edilen istekten sonra termostat yine kendi kararını uygular: gerek yokken açılan kombi minimum çalışma süresi dolunca söner, gerekliyken kapatılan kombi minimum bekleme süresi dolunca yeniden yanar.

## Buton ve LED

Kart üzerindeki **BOOT** butonu (`GPIO0`) ve LED (`GPIO2`) kullanılır. Çıplak modülde bunları harici olarak bağlayın (bkz. [Bağlantı](#bağlantı)).

**BOOT** butonu:

| Basma süresi | İşlev | LED |
|---|---|---|
| Kısa (< 2 sn) | Kombi rölesini test için açar/kapatır (HA'daki **Kombi Rölesi** ile aynı koruma kuralları geçerlidir, bkz. [Pano Kartı](#pano-kartı)) | — |
| 5–10 sn | Wi-Fi ayarlarını siler, kurulum ağını açar | 5. saniyeden sonra hızlı yanıp söner |
| 10–30 sn | Fabrika ayarlarına döner (tüm ayarlar ve API anahtarı silinir) | 10. saniyeden sonra sabit yanar |

LED, normalde Wi-Fi bağlı değilken yanıp söner; bağlıyken yanıyorsa kombi çalışıyordur.

## Güncelleme ve Güvenlik

**Güncelleme:** Cihaz kendiliğinden güncellenmez. Yeni sürümü [Releases](https://github.com/omerfarukaktas/esp32-xiaomi-termostat/releases) sayfasından indirip ESPHome ile ağ üzerinden (`.ota.bin`) ya da USB ile (`.factory.bin`, adres `0x0`) yükleyin. Güncelleme sırasında röle kapanır ve açılışta minimum bekleme süresi uygulanır.

**Güvenlik:**
- Home Assistant API'si şifrelidir; anahtar yalnızca fabrika ayarlarına dönüşle silinir.
- **OTA Güncelleme Şifresi** belirlenmezse yerel ağdaki herhangi bir cihaz termostata firmware yükleyebilir. Kurulumdan sonra mutlaka bir şifre belirleyin.
- BindKey ve OTA şifresi cihazda saklanır; Home Assistant'ta yalnızca `********` olarak görünür.
- Kurulum ağı (`Termostat-Kurulum`) yalnızca cihaz ev ağına bağlanamadığında açılır.

## Sorun Giderme

| Belirti | Çözüm |
|---|---|
| Oda Sıcaklığı hiç gelmiyor | MAC adresini kontrol edin. Orijinal firmware'de BindKey'i girin ve ~10 dk bekleyin. PVVX'te reklam türünün *PVVX (Custom)* olduğundan emin olun. Sensör ile ESP32 arasındaki mesafeyi azaltın. |
| Sık sık "Sensör Bağlantı Uyarısı" | Sensör pilini ve **Xiaomi Sinyal Seviyesi** değerini kontrol edin; ESP32'yi sensöre yaklaştırın. |
| Kombi yanmıyor, durum "Dinlenme Koruması" | Minimum bekleme süresi dolunca yanar. Açılıştan sonra da bu süre beklenir. |
| Home Assistant bağlanamıyor | Cihazı entegrasyondan kaldırıp yeniden ekleyin. Gerekirse BOOT butonuyla fabrika ayarlarına dönün. |
| Wi-Fi değişti | BOOT butonunu 5–10 sn basılı tutun ve [Wi-Fi kurulumunu](#3-wi-fia-bağlayın) tekrarlayın. |

Ayrıntılı kayıtları görmek için cihazı USB ile bağlayıp [ESPHome Web](https://web.esphome.io) üzerinden *Logs* açabilir ya da Home Assistant'taki ESPHome eklentisini kullanabilirsiniz.

## Geliştirme

```
termostat.yaml                ESPHome yapılandırması (tüm termostat mantığı)
include/termostat_helpers.h   MAC / BindKey doğrulama yardımcıları
partitions.csv                4 MB flash için 2 × 1.94 MB OTA bölüm tablosu
tests/                        Kontrol motoru için donanımsız senaryo testleri
tools/termostat_cli.py        Home Assistant olmadan test için terminal aracı
docs/                         Hazır Home Assistant panosu (bkz. Pano Kartı)
.github/workflows/            Derleme, test ve sürüm yayınlama iş akışları
```

**Derleme ve yükleme** ([ESPHome](https://esphome.io/guides/installing_esphome) 2025.9 gerekir):

```bash
esphome compile termostat.yaml
esphome upload termostat.yaml --device <cihaz-ip>   # USB için --device /dev/ttyUSB0
```

> [!NOTE]
> Sistem diliniz Türkçe ise derlemeyi `LC_ALL=C esphome compile termostat.yaml` şeklinde çalıştırın. Xtensa assembler Türkçe yerel ayarında büyük "I" harfini "ı" olarak işlediği için ESP-IDF derlemesi `unknown opcode ... WINDOWBASE` hatasıyla durur.

OTA şifresi belirlenmişse ağ üzerinden yükleme için `termostat.yaml` içindeki `ota.password` değerini aynı şifreyle değiştirin ya da şifreyi Home Assistant'tan geçici olarak boşaltın.

**Testler** (g++ ve PyYAML gerekir):

```bash
python3 tests/run_engine_tests.py        # -v ile ayrıntılı kayıt
```

**Terminal aracı** (`pip install aioesphomeapi`):

```bash
export TERMOSTAT_HOST=<cihaz-ip>
export TERMOSTAT_API_KEY=<Home Assistant'taki API anahtarı>
python3 tools/termostat_cli.py status
python3 tools/termostat_cli.py set-mode otomatik   # otomatik | manuel | donma | kapali
```

API anahtarı Home Assistant arayüzünde gösterilmez. HA'nın `/config/.storage/core.config_entries` dosyasında (File Editor, Studio Code Server veya SSH eklentisiyle) bu cihazın `"domain": "esphome"` kaydındaki `"noise_psk"` değerinden kopyalanabilir. Bu dosyayı düzenlemeyin, yalnızca okuyun.

```bash
git tag v1.0.0
git push origin v1.0.0
```

## Lisans

Copyright (C) 2026 &lt;Ömer Faruk Aktaş&gt;

Bu proje [GNU Genel Kamu Lisansı v3.0](LICENSE) ile lisanslanmıştır. Kodu kullanabilir, değiştirebilir ve dağıtabilirsiniz; ancak dağıttığınız her kopyada ve türev çalışmada telif bildirimi korunmalı ve kaynak kodu aynı lisansla paylaşılmalıdır.

[PVVX firmware](https://github.com/pvvx/ATC_MiThermometer) ve [ESPHome](https://esphome.io) kendi lisanslarına tabidir. Bu proje Xiaomi ile bağlantılı değildir.
