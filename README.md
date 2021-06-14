# Koinos System Smart Contracts

## Building

To build Koinos system contracts, you must first build and install the [CDT](https://github.com/koinos/koinos-cdt).

Set the following env vars as was needed to build the CDT.

`KOINOS_CDT_ROOT` and `KOINOS_WASI_SDK_ROOT`.

Then run the following commands to do an out of source cmake build.

```
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=${KOINOS_CDT_ROOT}/cmake/koinos-wasm-toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

`.wasm` binaries are in your build directory and are ready to be uploaded to Koinos.
