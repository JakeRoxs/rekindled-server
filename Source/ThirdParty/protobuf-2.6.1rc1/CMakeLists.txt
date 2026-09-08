# ================================================================================================
#  Rekindled Server
#  Copyright (C) 2021 Tim Leonard
#  Copyright (C) 2026 Jake Morgeson
# ================================================================================================

project(libprotobuf-lite C CXX)

if (UNIX)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
endif()

SET(SOURCES
    vsprojects/config.h

    src/google/protobuf/io/coded_stream.h
    src/google/protobuf/io/coded_stream_inl.h
    src/google/protobuf/io/zero_copy_stream.h
    src/google/protobuf/io/zero_copy_stream_impl_lite.h

    src/google/protobuf/stubs/atomicops.h
    src/google/protobuf/stubs/atomicops_internals_x86_msvc.h
    src/google/protobuf/stubs/common.h
    src/google/protobuf/stubs/hash.h
    src/google/protobuf/stubs/map_util.h
    src/google/protobuf/stubs/once.h
    src/google/protobuf/stubs/platform_macros.h
    src/google/protobuf/stubs/stl_util.h
    src/google/protobuf/stubs/stringprintf.h
    src/google/protobuf/stubs/template_util.h
    src/google/protobuf/stubs/type_traits.h
    
    src/google/protobuf/extension_set.h
    src/google/protobuf/generated_message_util.h
    src/google/protobuf/message_lite.h
    src/google/protobuf/repeated_field.h
    src/google/protobuf/wire_format_lite.h
    src/google/protobuf/wire_format_lite_inl.h

    src/google/protobuf/io/coded_stream.cc
    src/google/protobuf/io/zero_copy_stream.cc
    src/google/protobuf/io/zero_copy_stream_impl_lite.cc

    src/google/protobuf/stubs/common.cc
    src/google/protobuf/stubs/once.cc
    src/google/protobuf/stubs/stringprintf.cc

    src/google/protobuf/repeated_field.cc
    src/google/protobuf/extension_set.cc
    src/google/protobuf/generated_message_util.cc
    src/google/protobuf/message_lite.cc
    src/google/protobuf/wire_format_lite.cc
)

if (UNIX)
    SET(PLATFORM_SOURCES src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc)
else()
    SET(PLATFORM_SOURCES src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc)
endif()
    
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${PLATFORM_SOURCES})

if (UNIX)
    target_link_libraries(${PROJECT_NAME} Threads::Threads)
    # Avoid GCC/Clang deprecation warnings in old protobuf code
    # from std::iterator (deprecated in C++17+).
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated-declarations -Wno-deprecated-copy)
endif()

# All modern compilers (MSVC 2015+, GCC, Clang, MinGW) provide the standard
# unordered_map/unordered_set. Use the config that defines HASH_MAP_CLASS /
# HASH_SET_CLASS / HASH_NAMESPACE, which hash.h's generic path requires.
target_include_directories(${PROJECT_NAME} PUBLIC src gccprojects)

if (MSVC)
    # Modern MSVC (2015+) removed <hash_map>. Route hash.h to its generic branch
    # (std::unordered_map via HASH_MAP_CLASS) instead of its MSVC-specific branch,
    # which relies on the removed <hash_map> / hash_compare API.
    target_compile_definitions(${PROJECT_NAME} PRIVATE -D_STLPORT_VERSION)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS -D_USRDLL -DLIBPROTOBUF_EXPORTS)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4244 /wd4267 /wd4018 /wd4355 /wd4800 /wd4251 /wd4996 /wd4146 /wd4305)
elseif (UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_PTHREAD)
    target_compile_options(${PROJECT_NAME} PRIVATE "-pthread")
endif()
