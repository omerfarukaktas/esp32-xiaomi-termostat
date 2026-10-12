#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
ESP32 Xiaomi Termostat - Terminal Kontrol Aracı (Home Assistant Olmadan Test)

Kullanım:
  python3 tools/termostat_cli.py [--host IP] [--key ANAHTAR] status
  python3 tools/termostat_cli.py set-temp 24.5        -> Manuel moda geçer ve hedef sıcaklığı ayarlar
  python3 tools/termostat_cli.py set-mode otomatik    -> Çalışma modunu değiştirir
                                                   (otomatik | manuel | donma | kapali)
  python3 tools/termostat_cli.py toggle-relay         -> Kombi rölesini manuel açar/kapatır (test)
  python3 tools/termostat_cli.py set-mac AA:BB:CC:DD:EE:FF -> Xiaomi sensör MAC adresini günceller

Bağlantı:
  --host  veya TERMOSTAT_HOST ortam değişkeni (varsayılan: esp32-xiaomi-termostat.local)
  --key   veya TERMOSTAT_API_KEY ortam değişkeni. Cihaz Home Assistant'a eklendikten sonra
          API şifrelidir. Anahtarı Home Assistant üretir ve arayüzde göstermez; HA'nın
          /config/.storage/core.config_entries dosyasında, bu cihazın "esphome" kaydındaki
          "noise_psk" alanından kopyalanabilir.
"""
import argparse
import asyncio
import os
import re
import sys

import aioesphomeapi

ESP_PORT = 6053

MODLAR = {
    "otomatik": "Zaman Dilimi Programı (Otomatik)",
    "manuel": "Manuel Sabit Hedef",
    "donma": "Donma Koruması",
    "kapali": "Kapalı",
}

DURUM_SIRASI = [
    "Oda Sıcaklığı", "Aktif Hedef Sıcaklık", "Oda Nem", "Xiaomi Sensör Pil",
    "Xiaomi Sinyal Seviyesi", "Kombi Rölesi", "Aktif Program Dilimi",
    "Termostat Çalışma Durumu", "Termostat Çalışma Modu", "Sensör Tipi", "Manuel Hedef Sıcaklık",
    "Histerezis Toleransı",
    "Kombi Minimum Çalışma Süresi", "Kombi Minimum Bekleme Süresi", "Sensör Bağlantı Uyarısı",
    "Sensör Son Veri", "Xiaomi Sensör MAC Adresi", "WiFi Sinyal", "Bos RAM",
]

MAC_RE = re.compile(r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


class Termostat:
    def __init__(self, host, key):
        self.host = host
        self.cli = aioesphomeapi.APIClient(host, ESP_PORT, password="", noise_psk=key or None)
        self.entities = []

    async def __aenter__(self):
        try:
            await self.cli.connect(login=True)
        except aioesphomeapi.APIConnectionError as e:
            sys.exit(f"❌ {self.host} cihazına bağlanılamadı: {e}\n"
                     "   Cihaz şifreli ise --key / TERMOSTAT_API_KEY ile API anahtarını verin.")
        self.entities, _ = await self.cli.list_entities_services()
        return self

    async def __aexit__(self, *exc):
        await self.cli.disconnect()

    def key(self, name):
        for e in self.entities:
            if e.name == name:
                return e.key
        sys.exit(f"❌ Cihazda '{name}' varlığı bulunamadı (firmware güncel mi?)")

    async def states(self, wait=2.0):
        received = {}
        names = {e.key: e.name for e in self.entities}

        def on_state(state):
            if getattr(state, "missing_state", False):
                return
            received[names.get(state.key, "")] = getattr(state, "state", None)

        self.cli.subscribe_states(on_state)
        await asyncio.sleep(wait)
        return received


async def show_status(t: Termostat):
    received = await t.states()
    print("=" * 64)
    print(f"🔥 ESP32 XIAOMI TERMOSTAT CANLI DURUM ({t.host})")
    print("=" * 64)
    for k in DURUM_SIRASI:
        if k not in received:
            continue
        v = received[k]
        if isinstance(v, bool):
            v_str = "AÇIK" if v else "KAPALI"
        elif isinstance(v, float):
            v_str = "—" if v != v else f"{v:.2f}"
        else:
            v_str = str(v)
        print(f"  {k:32}: {v_str}")
    print("=" * 64)


async def set_temp(t: Termostat, val: float):
    t.cli.select_command(t.key("Termostat Çalışma Modu"), MODLAR["manuel"])
    t.cli.number_command(t.key("Manuel Hedef Sıcaklık"), val)
    await asyncio.sleep(1)
    print(f"✅ Termostat Modu 'Manuel Sabit Hedef' ve Hedef Sıcaklık {val} °C olarak ayarlandı.")


async def set_mode(t: Termostat, mode: str):
    t.cli.select_command(t.key("Termostat Çalışma Modu"), MODLAR[mode])
    await asyncio.sleep(1)
    print(f"✅ Termostat Modu '{MODLAR[mode]}' olarak ayarlandı.")


async def toggle_relay(t: Termostat):
    received = await t.states(wait=1.0)
    curr = received.get("Kombi Rölesi")
    if curr is None:
        sys.exit("❌ Kombi rölesinin mevcut durumu okunamadı.")
    new_state = not curr
    t.cli.switch_command(t.key("Kombi Rölesi"), new_state)
    await asyncio.sleep(1)
    print(f"✅ Kombi Rölesi {'AÇILDI (ON)' if new_state else 'KAPATILDI (OFF)'}.")
    print("   Not: Termostat motoru koruma süreleri dolunca röleyi tekrar kendi kararına göre yönetir.")


async def set_mac(t: Termostat, mac: str):
    t.cli.text_command(t.key("Xiaomi Sensör MAC Adresi"), mac.upper())
    await asyncio.sleep(1)
    print(f"✅ Xiaomi sensör MAC adresi '{mac.upper()}' olarak güncellendi ve hafızaya kaydedildi.")


def parse_args():
    p = argparse.ArgumentParser(description="ESP32 Xiaomi Termostat terminal aracı",
                                formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    p.add_argument("--host", default=os.environ.get("TERMOSTAT_HOST", "esp32-xiaomi-termostat.local"))
    p.add_argument("--key", default=os.environ.get("TERMOSTAT_API_KEY"), help="API şifreleme anahtarı")
    sub = p.add_subparsers(dest="cmd")
    sub.add_parser("status")
    st = sub.add_parser("set-temp")
    st.add_argument("value", type=float)
    sm = sub.add_parser("set-mode")
    sm.add_argument("mode", choices=sorted(MODLAR))
    sub.add_parser("toggle-relay")
    sc = sub.add_parser("set-mac")
    sc.add_argument("mac")
    args = p.parse_args()
    if args.cmd == "set-temp" and not 10.0 <= args.value <= 30.0:
        p.error("hedef sıcaklık 10.0 - 30.0 °C aralığında olmalı")
    if args.cmd == "set-mac" and not MAC_RE.match(args.mac):
        p.error("MAC adresi AA:BB:CC:DD:EE:FF biçiminde olmalı")
    return args


async def main():
    args = parse_args()
    async with Termostat(args.host, args.key) as t:
        if args.cmd in (None, "status"):
            await show_status(t)
        elif args.cmd == "set-temp":
            await set_temp(t, args.value)
        elif args.cmd == "set-mode":
            await set_mode(t, args.mode)
        elif args.cmd == "toggle-relay":
            await toggle_relay(t)
        elif args.cmd == "set-mac":
            await set_mac(t, args.mac)


if __name__ == "__main__":
    asyncio.run(main())
