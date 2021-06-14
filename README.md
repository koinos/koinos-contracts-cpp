Koinos system smart contracts.

## Building

To build Koinos system contracts, you must first build the and install [CDT](https://github.com/koinos/koinos-cdt).

Set the following env vars as you needed to build the CDT.

`KOINOS_CDT_ROOT`, `KOINOS_WASI_SDK_ROOT`.

Then run the following commands to do an out of source cmake build.

```
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=${KOINOS_CDT_ROOT}/cmake/koinos-wasm-toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

`.wasm` binaries are in your build directory and are ready to be uploaded to Koinos.
