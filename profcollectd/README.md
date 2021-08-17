# Profcollect

Profcollect is a system daemon that facilitates sampling profile collection and reporting for native
platform applications.

Profcollect can only be enabled on `userdebug` or `eng` builds.

## Supported Platforms

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
| Periodic   | collection\_interval   |
| App launch | applaunch\_trace\_freq |

Setting the frequency value to `0` disables collection for the corresponding event.

### Processing

The raw tracing data needs to be combined with the original binary to create the AutoFDO branch
list. This is a costly process, thus it is done separately from the profile collection. Profcollect
attempts to process all the traces when the device is idle and connected to a power supply. It can
also be initiated by running:

```
adb shell profcollectctl process
```

### Reporting

#### Manual

After actively using the device for a period of time, the device should have gathered enough data to
generate a good quality PGO profile that represents typical system usage. Run the following command
to create a profile report:

```
$ adb shell profcollectctl report
Creating profile report
Report created at: 12345678-0000-abcd-8000-12345678abcd
```

You can then fetch the report by running (under root):

```
adb pull /data/misc/profcollectd/report/12345678-0000-abcd-8000-12345678abcd.zip
```

#### Automated Uploading to Server

*In development*

### Post Processing

For each trace file, run:

```
simpleperf inject \
    -i {TRACE_FILE_NAME} \
    -o {OUTPUT_FILE_NAME}.data \
    --binary {BINARY_NAME} \
    --symdir out/target/product/{PRODUCT_NAME}/symbols
```

Afterwards, run [AutoFDO](https://github.com/google/autofdo) to generate Clang PGO profiles:

```
create_llvm_prof \
    --profiler text \
    --binary=${BINARY_PATH} \
    --profile=${INPUT_FILE_NAME} \
    --out={OUTPUT_FILE_NAME}.profdata
```

Finally, merge all the PGO profiles into one profile:

```
find {INPUT_DIR} -name *.profdata > proflist
prebuilts/clang/host/linux-x86/llvm-binutils-stable/llvm-profdata merge \
    --binary \
    --sample \
    --input-files proflist \
    --output merged.profdata
```

More profile data usually generates better quality profiles. You may combine data from multiple
devices running the same build to improve profile quality, and/or reduce the performance impact for
each device (by reducing collection frequency).
