# ZN-HostsRedirect

A lightweight **Zygisk Next** module that redirects `/system/etc/hosts` and `/etc/hosts` reads inside `netd` to `/data/adb/hostsredirect/hosts` without mounting over system files. Built for **bindhosts (Mode 4)** on Android 15+ (`arm64-v8a`).

## Requirements
- Root manager (Magisk / KernelSU / APatch)
- **Zygisk Next**
- Android 15+ (API 35+), `arm64-v8a`

## How It Works
- **Targeted injection**: `zn_modules.txt` restricts loading strictly to `netd` (`name=netd companion lib/libhostsredirect.so`), so app processes and Zygote are never touched.
- **Fast-path `__openat` hook**: Non-read-only calls and non-hosts paths tail-call directly into `old_openat`.
- **Autonomous companion**: On load, the companion creates `/data/adb/hostsredirect` and a default `hosts` file if missing, applies `0o644` and `u:object_r:system_file:s0`, and caches `st_ino`/`st_mtim` before passing file descriptors to `netd` via `SCM_RIGHTS`.
- **16 KB page-size ready**: Linked with `-z max-page-size=16384` and `--pack-dyn-relocs=relr`.

## Build
Requires **Gradle 9.9.0** (via `./gradlew`), **CMake**, and **Android NDK** (`ANDROID_NDK_HOME` or `ANDROID_HOME` set):

```sh
./gradlew zipRelease
```

The flashable module archive is written to `release/ZN-hostsredirect-v4.1-release.zip`.
