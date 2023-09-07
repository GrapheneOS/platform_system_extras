# Pin Tool

This tool is used for multiple operations related to memory pinning and inspection.

## Build and Install the tool

To build and push the binary to device
```
mm pintool
and push $ANDROID_REPO/out/target/product/<lunchtarget>/system/bin/pintool /data/local/tmp/pintool
adb shell
cd /data/local/tmp/pintool
```

## How to use the tool to generate pinner files

One of the core usages of the tool is to inspect resident memory and
generate pinlist.meta files which are then consumed by the Android PinnerService
to pin memory (mlock) and avoid it from being evicted.

A sample usage of this tool in a PGO style fashion can be like this:


This is a sample flow of how to use this tool:
1. Run the app that makes use of the file you want to pin. To have resident
memory for the file.

2. Run this program to generate the pinlist.meta file.
```
./pintool probe -p <file_to_probe> <options>
```
This will inspect the memory for the provided <file_to_probe> and generate
a pinlist.meta file with the resident memory ranges.

Note: Running ./pintool to obtain more usage documentation.

3. Output "pinlist.meta" can be incorporate it within your build
process to have it bundled inside your apk (inside assets/<file_to_pin.apk>)
to have the PinnerService know what memory ranges to pin within your apk.
Note: The PinnerService will need to support pinning your apk, so you may
need to explicitly request a pin.

## Other potential uses

Outside of pinner service, the tool can be used to inspect resident memory for
any file in memory.

## Extra information

the pinlist.meta depends on the apk contents and needs to be regenrated if
you are pushing a new version of your apk.