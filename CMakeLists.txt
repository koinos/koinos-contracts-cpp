cmake_minimum_required(VERSION 3.10.2)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
   set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
   set(CMAKE_XCODE_ATTRIBUTE_CC           "${CMAKE_SOURCE_DIR}/ci/ccache_clang")
   set(CMAKE_XCODE_ATTRIBUTE_CXX          "${CMAKE_SOURCE_DIR}/ci/ccache_clang++")
   set(CMAKE_XCODE_ATTRIBUTE_LD           "${CMAKE_SOURCE_DIR}/ci/ccache_clang")
   set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS   "${CMAKE_SOURCE_DIR}/ci/ccache_clang++")
endif()

project(koinos_system_contracts VERSION 0.1.0 LANGUAGES CXX)

#option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)

# This is to force color output when using ccache with Unix Makefiles
#if(${FORCE_COLORED_OUTPUT})
#    if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
#       add_compile_options (-fdiagnostics-color=always)
#    elseif( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" )
#       add_compile_options (-fcolor-diagnostics)
#    endif ()
#endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)
#set(Boost_NO_BOOST_CMAKE ON)

#if (MSVC)
    # warning level 4 and all warnings as errors
#    add_compile_options(/W4 /WX)
#else()
    # lots of warnings and all warnings as errors
#    add_compile_options(-Werror -Wno-unknown-pragmas)
#endif()

if(${CMAKE_GENERATOR} MATCHES "Xcode")
  set(CMAKE_XCODE_GENERATE_SCHEME YES)
endif()

add_subdirectory(contracts)

