add_executable(resources resources.cpp)

target_link_libraries(resources koinos_proto_embedded koinos_api koinos_api_cpp koinos_wasi_api c c++ c++abi clang_rt.builtins-wasm32)
