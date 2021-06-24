add_executable(forever forever.cpp)

target_link_libraries(forever koinos_api koinos_api_cpp koinos_wasi_api c c++ c++abi clang_rt.builtins-wasm32)
