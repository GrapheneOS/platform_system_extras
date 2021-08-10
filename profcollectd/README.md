# Profcollect

Profcollect is a system daemon that facilitates sampling profile collection and reporting for native
platform applications.

Profcollect can only be enabled on `userdebug` or `eng` builds.

## Supported platforms

Currently Profcollect only supports collecting profiles from Coresight ETM enabled ARM devices.

Instructions to enable Coresight ETM can be found from the
[simpleperf manual](https://android.googlesource.com/platform/system/extras/+/refs/heads/master/simpleperf/doc/collect_etm_data_for_autofdo.md).

## Usage

Profcollect has two components: `profcollectd`, the system daemon, and `profcollectctl`, the command
line interface.

### Collection

`profcollectd` can be started from `adb` directly (under root), or automatically on system boot by
setting system property through:

```
adb shell device_config put profcollect_native_boot enabled true
```

Profcollect collects profiles periodically, as well as through triggers like app launch events. Only
a percentage of these events result in a profile collection to avoid using too much resource, these
are controlled by the following configurations:

| Event      | Config                 |
|------------|------------------------|
| App launch | applaunch\_trace\_freq |

Setting the frequency value to `0` disables collection for the corresponding event.

### Reporting

*In development*
