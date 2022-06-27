add_executable( add_thunk  add_thunk.cpp)

target_link_libraries( add_thunk koinos_proto_embedded koinos_api koinos_api_cpp koinos_wasi_api c c++ c++abi clang_rt.builtins-wasm32)
