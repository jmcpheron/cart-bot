# PlatformIO extra_script: read ../secrets.env and inject WiFi credentials as
# compile-time defines. No file → no defines → firmware falls back to AP-only.
import os

Import("env")  # noqa: F821  (PlatformIO injects this)

SECRETS = os.path.join(env["PROJECT_DIR"], "..", "secrets.env")

def _parse(path):
    creds = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, val = line.partition("=")
            creds[key.strip()] = val.strip().strip('"').strip("'")
    return creds

if os.path.exists(SECRETS):
    creds = _parse(SECRETS)
    for key in ("HOME_WIFI_SSID", "HOME_WIFI_PASS"):
        if creds.get(key):
            env.Append(CPPDEFINES=[(key, env.StringifyMacro(creds[key]))])
    print("load_secrets: injected HOME_WIFI_* from secrets.env")
else:
    print("load_secrets: no secrets.env — building AP-only firmware")
