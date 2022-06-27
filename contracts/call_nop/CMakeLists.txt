add_executable( call_nop  call_nop.cpp)

target_link_libraries( call_nop koinos_proto_embedded koinos_api koinos_api_cpp koinos_wasi_api c c++ c++abi clang_rt.builtins-wasm32)
