#pragma once
// ESP32 Xiaomi Termostat - yardımcı fonksiyonlar (termostat.yaml tarafından kullanılır)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cctype>
#include <cstdint>
#include <string>

// HA'da gizli alanların (OTA şifresi, BindKey) yerine gösterilen değer.
static const char *const TERMOSTAT_MASKE = "********";

// "A4:C1:38:C5:0D:5D" biçimini katı şekilde doğrular ve 48-bit adrese çevirir.
// Biçim dışı her girdi (eksik/fazla karakter, '-' ayraç, hex olmayan karakter) reddedilir.
inline bool termostat_parse_mac(const std::string &s, uint64_t &out) {
  if (s.size() != 17)
    return false;
  uint64_t mac = 0;
  for (size_t i = 0; i < 17; i++) {
    const char c = s[i];
    if (i % 3 == 2) {
      if (c != ':')
        return false;
      continue;
    }
    if (!std::isxdigit(static_cast<unsigned char>(c)))
      return false;
    const int v = std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : (std::toupper(c) - 'A' + 10);
    mac = (mac << 4) | static_cast<uint64_t>(v);
  }
  if (mac == 0)
    return false;
  out = mac;
  return true;
}

// Metni büyük harfe çevirir (MAC/BindKey'in HA'da tutarlı görünmesi için).
inline std::string termostat_upper(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

// Xiaomi BindKey: tam 32 hex karakter.
inline bool termostat_valid_bindkey(const std::string &s) {
  if (s.size() != 32)
    return false;
  for (char c : s)
    if (!std::isxdigit(static_cast<unsigned char>(c)))
      return false;
  return true;
}
