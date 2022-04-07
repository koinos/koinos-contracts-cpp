add_executable(add_vrf add_vrf.cpp)

target_link_libraries(add_vrf koinos_proto_embedded koinos_api koinos_api_cpp koinos_wasi_api c c++ c++abi clang_rt.builtins-wasm32)
