#!/usr/bin/env python3
"""
Termostat kontrol motoru testleri.

termostat.yaml içindeki C++ lambda'larını (termostat_kontrol, sensor_veri_alindi ve röle
olayları) çıkarır, sahte ESPHome bileşenleriyle birlikte bilgisayarda derler ve
senaryo testlerini (tests/engine_test.cpp) çalıştırır. Donanım gerektirmez.

Kullanım:  python3 tests/run_engine_tests.py
Gereken:   g++ (C++17), PyYAML
"""
import pathlib
import subprocess
import sys
import tempfile

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent


class Loader(yaml.SafeLoader):
    pass


Loader.add_constructor("!lambda", lambda loader, node: loader.construct_scalar(node))
Loader.add_constructor("!secret", lambda loader, node: "")


def lambda_of(actions):
    for action in actions:
        if isinstance(action, dict) and "lambda" in action:
            return action["lambda"]
    raise KeyError("lambda bulunamadı")


def main():
    config = yaml.load((ROOT / "termostat.yaml").read_text(encoding="utf-8"), Loader=Loader)
    scripts = {s["id"]: s for s in config["script"]}
    relay = next(s for s in config["switch"] if s["id"] == "kombi_role")
    manuel = next(s for s in config["switch"] if s["id"] == "kombi_role_manuel")

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        (tmp / "kontrol.inc").write_text(lambda_of(scripts["termostat_kontrol"]["then"]), encoding="utf-8")
        (tmp / "veri.inc").write_text(lambda_of(scripts["sensor_veri_alindi"]["then"]), encoding="utf-8")
        (tmp / "on.inc").write_text(lambda_of(relay["on_turn_on"]), encoding="utf-8")
        (tmp / "off.inc").write_text(lambda_of(relay["on_turn_off"]), encoding="utf-8")
        (tmp / "manuel_on.inc").write_text(lambda_of(manuel["turn_on_action"]), encoding="utf-8")
        (tmp / "manuel_off.inc").write_text(lambda_of(manuel["turn_off_action"]), encoding="utf-8")

        binary = tmp / "engine_test"
        build = subprocess.run(
            ["g++", "-std=c++17", "-Wall", "-Wno-unused", f"-I{tmp}", f"-I{ROOT / 'include'}",
             str(ROOT / "tests" / "engine_test.cpp"), "-o", str(binary)],
            capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout + build.stderr)
            return build.returncode
        return subprocess.run([str(binary)] + sys.argv[1:]).returncode


if __name__ == "__main__":
    sys.exit(main())
