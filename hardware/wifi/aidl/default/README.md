# `hardware/wifi/aidl/default` — the AIDL wifi HAL, vendored into the common tree

This is the AOSP/LineageOS **AIDL wifi HAL** (the maintained AIDL wrapper around the legacy
`libwifi-hal.so` vendor library), imported into `device/samsung/universal7870-common` so the 7870
devices keep a working wifi HAL that

* does not depend on the HIDL implementation that `universal7870-common-patches` used to import
  (`hardware/interfaces/0001-Revert-Remove-HIDL-implementation-for-the-WiFi.patch` — dropped),
* is not affected by upstream churn in `hardware/interfaces/wifi/aidl/default`,
* can be patched locally, like every other HAL in this tree (`hardware/camera`, `hardware/amplifier`, …).

| | |
|---|---|
| upstream | `hardware/interfaces/wifi/aidl/default` @ `lineage-22.2` |
| pinned commit | `89252f8fa9f56b373581beee2ca0ffe19bfdcd93` (2026-06-01) |
| copied | 2026-09-29, verbatim except the deviations below |
| built as | `android.hardware.wifi-service-legacy` (see `Android.bp`) |
| installed as | `/vendor/bin/hw/android.hardware.wifi-service` (unchanged path!) |

## Soong module renames

| upstream module | here | note |
|---|---|---|
| `wifi_hal_cc_defaults` (soong_config_module_type) | `wifi_hal_legacy_cc_defaults` | same `config_namespace: "wifi"` and variables, so existing `soong_config_set`s still apply |
| `android.hardware.wifi-service-cppflags-defaults` | `…-legacy-cppflags-defaults` | |
| `android.hardware.wifi-service-lib` | `…-legacy-lib` | the `cc_library_static` with all the sources |
| `android.hardware.wifi-service` | `…-legacy` | `stem: "android.hardware.wifi-service"` |
| `android.hardware.wifi-service-lazy` | `…-legacy-lazy` | `stem: "android.hardware.wifi-service-lazy"` |
| `android.hardware.wifi-service.xml` (vintf fragment) | `…-legacy.xml_vintf` | fragment content unchanged |
| `android.hardware.wifi-service-tests` | `…-legacy-tests` | `cc_test`, only built with `WITH_TESTS=true` |
| `default-android.hardware.wifi-service*.{rc,xml}` (filegroups) | `default-android.hardware.wifi-service-legacy*` | for products that want the default rc/fragment |

## Why the *installed* names stay standard

`system/sepolicy/vendor/file_contexts` labels exactly these paths:

```
/(vendor|system/vendor)/bin/hw/android\.hardware\.wifi-service       u:object_r:hal_wifi_default_exec:s0
/(vendor|system/vendor)/bin/hw/android\.hardware\.wifi-service-lazy  u:object_r:hal_wifi_default_exec:s0
```

and `hal_wifi_default_exec` is declared in **private** policy (`system/sepolicy/private/hal_wifi.te`),
which device trees cannot reference. A device-tree module that installs a differently named binary
would therefore have no SELinux label for the process domain and the HAL would fail to start on an
enforcing build. So the module is renamed, the binary is not (`stem:`), and no sepolicy change is
needed.

The same reasoning applies to the two interface strings that the framework and `hwservicemanager`
look for — they are **not** renamed anywhere:

* init service name: `vendor.wifi_hal_legacy` (`*.rc`)
* AIDL instance: `android.hardware.wifi.IWifi/default`, declared in
  `android.hardware.wifi-service-legacy.xml` as `<name>android.hardware.wifi</name>`,
  `<version>3</version>`

## Deviations from upstream (complete list)

1. Soong module names as in the table above (plus the license module
   `device_samsung_universal7870_wifi_legacy_license` and `NOTICE`, because
   `hardware_interfaces_license` is package-scoped to `hardware/interfaces`), and no
   `default_team` metadata.
2. **Added** on the static library:
   ```python
   header_libs: ["wifi_legacy_headers"],
   export_header_lib_headers: ["wifi_legacy_headers"],
   ```
   This is the structural version of the `wipe_hal_fn` slot-shift fix: `wifi_legacy_hal.h` includes
   `<hardware_legacy/wifi_hal.h>`, and on Android 15 that header only exists in
   `hardware/interfaces/wifi/legacy_headers` (153 entries) — the same generation the vendor
   `libwifi-hal.so` is built against. Without the pin, the include is satisfied through the linked
   vendor library and a stale 130-entry generation shifts index 106 from
   `wifi_get_supported_iface_name` to `wifi_virtual_interface_delete` → SIGSEGV in `configureChip()`.
3. rc / fragment **file names** got the `-legacy` suffix; their contents only gained a comment.
4. Nothing else: all `.cpp`/`.h`, `tests/`, `THREADING.README` are byte-identical to upstream.

## Using it

In the device tree that should ship it (currently `device/samsung/a3y17lte/device.mk`):

```make
PRODUCT_PACKAGES += \
    android.hardware.wifi-service-legacy \
    …
```

Nothing else is required: the module installs the standard binary path, ships its own init rc and
VINTF fragment, and `WifiHal.createWifiHalMockable()` in `packages/modules/Wifi` finds the service
through the fragment (`android.hardware.wifi.IWifi/default`) — it never looks at binary or module
names. Verify after flashing:

```bash
adb logcat -d | grep WifiHal        # "using the AIDL implementation", "IWifi binder. Local Version: 3"
adb shell getprop init.svc.vendor.wifi_hal_legacy    # running
```

## Keeping it in sync

When the tree moves to a newer LineageOS branch, diff your copy against upstream and re-apply the
deviations:

```bash
diff -ru --exclude='*.rc' --exclude='*.xml' --exclude=Android.bp --exclude=NOTICE \
     hardware/interfaces/wifi/aidl/default \
     device/samsung/universal7870-common/hardware/wifi/aidl/default | less
```

Update the pinned commit in this README and in `Android.bp` when you do. On LOS 23.x the AIDL
interface version may have moved past 3 — then the fragment's `<version>` and the
`android.hardware.wifi-V<n>-ndk` dependency have to follow the new upstream, not just the `.cpp`
files (the framework requires the service to be at least the version it was built against).
