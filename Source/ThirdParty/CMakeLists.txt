# ================================================================================================
#  Rekindled Server
#  Copyright (C) 2022 Tim Leonard
# ================================================================================================

add_subdirectory(aes_modes)
add_subdirectory(aes)
add_subdirectory(protobuf-2.6.1rc1)
add_subdirectory(steam)

set(REKINDLED_SQLITE_SOURCE_DIR "" CACHE PATH "Path to a local sqlite source tree to use instead of downloading it")
# Prefers SQLite amalgamation package, which contains sqlite3.c/sqlite3.h/sqlite3ext.h in a stable layout for fallback builds.
set(REKINDLED_SQLITE_ARCHIVE "https://sqlite.org/2026/sqlite-amalgamation-3510300.zip" CACHE STRING "URL to a SQLite source archive for FetchContent resolution when system sqlite is unavailable")
set(REKINDLED_SQLITE_ARCHIVE_HASH "" CACHE STRING "Optional SHA256 hash (e.g., SHA256=<hash>) for REKINDLED_SQLITE_ARCHIVE to enable archive integrity validation and avoid redundant re-downloads")
option(REKINDLED_SQLITE_ALLOW_REMOTE_FETCH "Allow automatically fetching SQLite via network when system sqlite is not available" ON)

set(REKINDLED_OPENSSL_SOURCE_DIR "" CACHE PATH "Path to a local openssl source tree to use instead of resolving OpenSSL via vcpkg or the system")

include(FetchContent)


set(REKINDLED_DEP_FETCH_STATUS_FILE "${CMAKE_BINARY_DIR}/dependency-fetch-status.txt" CACHE FILEPATH "Path to a file recording which deps were fetched vs vendored")
# Clear/initialize status file on each configure pass
file(WRITE "${REKINDLED_DEP_FETCH_STATUS_FILE}" "")
function(rekindled_append_dep_status)
    # (name, status)
    if(NOT ARGN)
        return()
    endif()
    file(APPEND "${REKINDLED_DEP_FETCH_STATUS_FILE}" "${ARGN}\n")
endfunction()

# zlib
# Standardized on vcpkg (or a system zlib) as the provider; the previously
# vendored zlib sources are no longer built. A `zlib` target is exposed so the
# consumers that link by that name keep working.
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    message(FATAL_ERROR
        "zlib was not found.\n"
        "Provide it via vcpkg (recommended; see vcpkg-toolchain.cmake / vcpkg.json) or install a\n"
        "system zlib development package (e.g. zlib1g-dev on Linux, or set VCPKG_ROOT to a vcpkg checkout).")
endif()
if(NOT TARGET zlib)
    # Expose a `zlib` INTERFACE target so consumers that link by that name keep
    # working. We cannot rely on FindZLIB's ZLIB::ZLIB imported target here: the
    # vcpkg toolchain only populates the per-config variants
    # (ZLIB_LIBRARY_RELEASE/_DEBUG), which does not flow through reliably into a
    # usable link location (consumers end up with a bare 'zlib.lib'). An
    # INTERFACE target carrying the resolved full paths (with a config-specific
    # generator expression for multi-config generators) guarantees a correct
    # link line for every generator.
    add_library(zlib INTERFACE)
    if(ZLIB_LIBRARY_DEBUG AND ZLIB_LIBRARY_RELEASE)
        target_link_libraries(zlib INTERFACE
            $<$<STREQUAL:$<CONFIG>,Debug>:${ZLIB_LIBRARY_DEBUG}>
            $<$<NOT:$<STREQUAL:$<CONFIG>,Debug>>:${ZLIB_LIBRARY_RELEASE}>
        )
    elseif(ZLIB_LIBRARY_RELEASE)
        target_link_libraries(zlib INTERFACE ${ZLIB_LIBRARY_RELEASE})
    elseif(ZLIB_LIBRARY)
        target_link_libraries(zlib INTERFACE ${ZLIB_LIBRARY})
    endif()
    target_include_directories(zlib INTERFACE "${ZLIB_INCLUDE_DIR}")
endif()
if(DEFINED VCPKG_TARGET_TRIPLET)
    rekindled_append_dep_status("zlib: vcpkg")
else()
    rekindled_append_dep_status("zlib: system")
endif()

# HttpLib (cpp-httplib)
# Embedded WebUI HTTP server and the outbound HTTP client (master-server
# registration, public IP lookup). cpp-httplib is a header-only C++
# HTTP server/client library resolved via FetchContent (a local source tree,
# or a pinned git tag) so nothing is vendored by hand.
# TLS support is wired to the project's resolved OpenSSL further down.
set(REKINDLED_HTTPLIB_SOURCE_DIR "" CACHE PATH "Path to a local cpp-httplib source tree to use instead of downloading it")
option(REKINDLED_HTTPLIB_ALLOW_REMOTE_FETCH "Allow automatically fetching cpp-httplib via network when local source is unavailable" ON)
set(REKINDLED_HTTPLIB_GIT_REPOSITORY "https://github.com/yhirose/cpp-httplib.git" CACHE STRING "cpp-httplib git repository to fetch")
set(REKINDLED_HTTPLIB_GIT_TAG "v0.54.1" CACHE STRING "cpp-httplib branch/tag to checkout (pinned release)")

# Disable httplib's own OpenSSL auto-detection: the project resolves OpenSSL
# itself below and wires TLS support into this INTERFACE target explicitly.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "Project manages cpp-httplib TLS support" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "Disable zlib compression in cpp-httplib" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "Disable brotli support in cpp-httplib" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "Disable zstd support in cpp-httplib" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "Disable install rules for cpp-httplib" FORCE)
set(HTTPLIB_TEST OFF CACHE BOOL "Disable cpp-httplib unit tests" FORCE)

if(REKINDLED_HTTPLIB_SOURCE_DIR)
    rekindled_append_dep_status("httplib: local source")
    FetchContent_Declare(
        httplib
        SOURCE_DIR "${REKINDLED_HTTPLIB_SOURCE_DIR}"
    )
else()
    if(NOT REKINDLED_HTTPLIB_ALLOW_REMOTE_FETCH)
        message(FATAL_ERROR "No cpp-httplib source provided and remote fetch is disabled (REKINDLED_HTTPLIB_ALLOW_REMOTE_FETCH=OFF). Provide REKINDLED_HTTPLIB_SOURCE_DIR.")
    endif()

    message(STATUS "Fetching cpp-httplib from ${REKINDLED_HTTPLIB_GIT_REPOSITORY}@${REKINDLED_HTTPLIB_GIT_TAG}")
    rekindled_append_dep_status("httplib: fetched git")

    FetchContent_Declare(
        httplib
        GIT_REPOSITORY "${REKINDLED_HTTPLIB_GIT_REPOSITORY}"
        GIT_TAG "${REKINDLED_HTTPLIB_GIT_TAG}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
endif()

FetchContent_MakeAvailable(httplib)

# cpp-httplib provides both `httplib` and the `httplib::httplib` alias, so
# either name is already linkable from downstream targets.

# SQLite
# Fallback path: local source -> system package -> archive fetch.
set(REKINDLED_SQLITE_SOURCE_DIR_USED FALSE)
if(REKINDLED_SQLITE_SOURCE_DIR)
    if(EXISTS "${REKINDLED_SQLITE_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Building sqlite from local source: ${REKINDLED_SQLITE_SOURCE_DIR}")
        rekindled_append_dep_status("sqlite: local source")
        add_subdirectory(${REKINDLED_SQLITE_SOURCE_DIR})
        set(REKINDLED_SQLITE_SOURCE_DIR_USED TRUE)
    else()
        message(WARNING "REKINDLED_SQLITE_SOURCE_DIR=${REKINDLED_SQLITE_SOURCE_DIR} does not appear to be a CMake-enabled sqlite tree; continuing system detection")
    endif()
endif()

if(NOT REKINDLED_SQLITE_SOURCE_DIR_USED)
    # Prefer the system sqlite package when available. On some platforms the
    # FindSQLite3 module only sets variables (no imported targets), and on others
    # the imported target may exist even if SQLite3_FOUND is unset.
    find_package(SQLite3 QUIET)
    message(STATUS "After find_package(SQLite3): SQLite3_FOUND=${SQLite3_FOUND} SQLite3_LIBRARY='${SQLite3_LIBRARY}' SQLite3_LIBRARIES='${SQLite3_LIBRARIES}' SQLite3_INCLUDE_DIR='${SQLite3_INCLUDE_DIR}' SQLite3_INCLUDE_DIRS='${SQLite3_INCLUDE_DIRS}'")

    # Normalize older FindSQLite3 outputs (singular vars) into the newer plural vars.
    if(NOT DEFINED SQLite3_LIBRARIES OR NOT SQLite3_LIBRARIES)
        if(DEFINED SQLite3_LIBRARY AND SQLite3_LIBRARY)
            set(SQLite3_LIBRARIES "${SQLite3_LIBRARY}")
        endif()
    endif()
    if(NOT DEFINED SQLite3_INCLUDE_DIRS OR NOT SQLite3_INCLUDE_DIRS)
        if(DEFINED SQLite3_INCLUDE_DIR AND SQLite3_INCLUDE_DIR)
            set(SQLite3_INCLUDE_DIRS "${SQLite3_INCLUDE_DIR}")
        endif()
    endif()

    message(STATUS "Normalized SQLite variables: SQLite3_LIBRARIES='${SQLite3_LIBRARIES}' SQLite3_INCLUDE_DIRS='${SQLite3_INCLUDE_DIRS}'")

    # Some FindSQLite3 implementations claim success but do not define a target.
    # Try to recover by emitting a usable target via pkg-config.
    set(REKINDLED_SQLITE_HAS_TARGET FALSE)
    if(TARGET SQLite::SQLite3 OR TARGET SQLite3::SQLite3)
        set(REKINDLED_SQLITE_HAS_TARGET TRUE)
    endif()

    set(REKINDLED_SQLITE_HAS_LIBS FALSE)
    if(DEFINED SQLite3_LIBRARIES AND SQLite3_LIBRARIES)
        set(REKINDLED_SQLITE_HAS_LIBS TRUE)
    endif()

    set(REKINDLED_SQLITE_AVAILABLE FALSE)
    if(SQLite3_FOUND OR REKINDLED_SQLITE_HAS_TARGET OR REKINDLED_SQLITE_HAS_LIBS)
        set(REKINDLED_SQLITE_AVAILABLE TRUE)
    endif()

    if(NOT REKINDLED_SQLITE_AVAILABLE)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(_sqlite3_pc sqlite3)
            if(_sqlite3_pc_FOUND)
                message(STATUS "pkg-config sqlite3 found: libs='${_sqlite3_pc_LIBRARIES}' includes='${_sqlite3_pc_INCLUDE_DIRS}'")
                set(SQLite3_FOUND TRUE)
                set(SQLite3_LIBRARIES "${_sqlite3_pc_LIBRARIES}")
                set(SQLite3_INCLUDE_DIRS "${_sqlite3_pc_INCLUDE_DIRS}")
                set(REKINDLED_SQLITE_HAS_LIBS TRUE)
                set(REKINDLED_SQLITE_AVAILABLE TRUE)
            endif()
        endif()
    endif()

    if(REKINDLED_SQLITE_AVAILABLE)
        message(STATUS "Using system sqlite: SQLite3_FOUND=${SQLite3_FOUND} SQLite3_LIBRARIES='${SQLite3_LIBRARIES}' SQLite3_INCLUDE_DIRS='${SQLite3_INCLUDE_DIRS}'")

        # Ensure we have a usable library variable when FindSQLite3 reports success.
        if(SQLite3_FOUND AND (NOT DEFINED SQLite3_LIBRARIES OR NOT SQLite3_LIBRARIES))
            message(WARNING "SQLite3_FOUND=TRUE but no SQLite3_LIBRARIES set; attempting to use SQLite3_LIBRARY")
            if(DEFINED SQLite3_LIBRARY AND SQLite3_LIBRARY)
                set(SQLite3_LIBRARIES "${SQLite3_LIBRARY}")
                set(REKINDLED_SQLITE_HAS_LIBS TRUE)
            endif()
        endif()

        # Provide a consistent, portable target name for consumers.
        # Some systems already provide a `SQLite::SQLite3` / `SQLite3::SQLite3` target.
        # Other environments may only supply variables (e.g., pkg-config).
        if(NOT TARGET sqlite)
            if(TARGET SQLite::SQLite3)
                add_library(sqlite ALIAS SQLite::SQLite3)
            elseif(TARGET SQLite3::SQLite3)
                add_library(sqlite ALIAS SQLite3::SQLite3)
            elseif(REKINDLED_SQLITE_HAS_LIBS)
                add_library(sqlite INTERFACE)
                target_link_libraries(sqlite INTERFACE ${SQLite3_LIBRARIES})
                if(DEFINED SQLite3_INCLUDE_DIRS AND SQLite3_INCLUDE_DIRS)
                    target_include_directories(sqlite INTERFACE ${SQLite3_INCLUDE_DIRS})
                endif()
            endif()
        endif()

        # Provide the canonical CMake target name if it is missing.
        if(TARGET sqlite AND NOT TARGET SQLite::SQLite3)
            add_library(SQLite::SQLite3 ALIAS sqlite)
        endif()
    endif()

    if(NOT TARGET sqlite)
        if(NOT REKINDLED_SQLITE_ALLOW_REMOTE_FETCH)
            message(FATAL_ERROR "Could not locate system SQLite3 and automatic remote fetch is disabled.\n" 
                                "Install SQLite3 development package (e.g. vcpkg sqlite3 on Windows or libsqlite3-dev on Linux).\n")
        endif()

        # Attempt to fetch SQLite from GitHub (latest backports) as a non-vendored fallback.
        # This path supports todo 037: externalized dependency usage from upstream repo.
        set(_sqlite_fetch_url "${REKINDLED_SQLITE_ARCHIVE}")
        if(NOT _sqlite_fetch_url)
            set(_sqlite_fetch_url "https://sqlite.org/2026/sqlite-autoconf-3510300.tar.gz")
        endif()

        set(_sqlite_archive "${CMAKE_BINARY_DIR}/_deps/sqlite-fetch.tar.gz")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")

        set(_sqlite_use_cached_archive FALSE)
        if(EXISTS "${_sqlite_archive}")
            if(REKINDLED_SQLITE_ARCHIVE_HASH)
                file(SHA256 "${_sqlite_archive}" _sqlite_current_hash)
                set(_sqlite_expected_hash "${REKINDLED_SQLITE_ARCHIVE_HASH}")
                string(REPLACE "SHA256=" "" _sqlite_expected_hash_norm "${_sqlite_expected_hash}")
                if(_sqlite_current_hash STREQUAL _sqlite_expected_hash_norm)
                    set(_sqlite_use_cached_archive TRUE)
                    message(STATUS "Using cached SQLite archive (hash matches): ${_sqlite_archive}")
                else()
                    message(WARNING "Cached SQLite archive hash mismatch: expected=${_sqlite_expected_hash_norm} got=${_sqlite_current_hash}; re-downloading")
                    file(REMOVE "${_sqlite_archive}")
                endif()
            else()
                set(_sqlite_use_cached_archive TRUE)
                message(STATUS "Using existing cached SQLite archive: ${_sqlite_archive}")
            endif()
        endif()

        if(NOT _sqlite_use_cached_archive)
            if(REKINDLED_SQLITE_ARCHIVE_HASH)
                file(DOWNLOAD "${_sqlite_fetch_url}" "${_sqlite_archive}" EXPECTED_HASH "${REKINDLED_SQLITE_ARCHIVE_HASH}" STATUS _dl_status SHOW_PROGRESS)
            else()
                file(DOWNLOAD "${_sqlite_fetch_url}" "${_sqlite_archive}" STATUS _dl_status SHOW_PROGRESS)
            endif()
            list(GET _dl_status 0 _dl_res)

            if(_dl_res EQUAL 0 AND EXISTS "${_sqlite_archive}")
                message(STATUS "Fetched SQLite archive to ${_sqlite_archive}")
                rekindled_append_dep_status("sqlite: fetched")
            else()
                message(FATAL_ERROR "Could not locate a usable SQLite target. Install a system sqlite3 development package (e.g. libsqlite3-dev on Linux, vcpkg sqlite3 on Windows) so FindSQLite3 succeeds. Fetch attempt from ${_sqlite_fetch_url} returned ${_dl_res}.")
            endif()
        else()
            rekindled_append_dep_status("sqlite: cached")
        endif()

            # Use the cached archive directly, without forcing FetchContent to re-download
            # from a URL protocol each configure. This improves reliability in offline/
            # cached dependency scenarios.
            set(_sqlite_extracted_dir "${CMAKE_BINARY_DIR}/_deps/sqlite_fetch_unpack")
            set(_sqlite_source_from_cache "")

            if(EXISTS "${_sqlite_extracted_dir}")
                file(GLOB_RECURSE _sqlite3_from_cache
                    "${_sqlite_extracted_dir}/sqlite3.c"
                    "${_sqlite_extracted_dir}/*/sqlite3.c"
                )
                if(_sqlite3_from_cache)
                    list(GET _sqlite3_from_cache 0 _sqlite_source_from_cache)
                    message(STATUS "Reusing cached extracted sqlite3.c from ${_sqlite_source_from_cache}")
                endif()
            endif()

            if(NOT _sqlite_source_from_cache)
                # Remove stale/unexpected extraction that does not contain sqlite3.c.
                if(EXISTS "${_sqlite_extracted_dir}")
                    file(REMOVE_RECURSE "${_sqlite_extracted_dir}")
                endif()

                file(MAKE_DIRECTORY "${_sqlite_extracted_dir}")
                file(ARCHIVE_EXTRACT INPUT "${_sqlite_archive}" DESTINATION "${_sqlite_extracted_dir}")

                # Some archive layouts may put sqlite3.c in a nested folder; fallback detection below will handle this.
                if(NOT EXISTS "${_sqlite_extracted_dir}/sqlite3.c")
                    set(_sqlite_manual_extract_dir "${CMAKE_BINARY_DIR}/_deps/sqlite_fetch_unpack")
                    file(REMOVE_RECURSE "${_sqlite_manual_extract_dir}")
                    file(MAKE_DIRECTORY "${_sqlite_manual_extract_dir}")
                    file(ARCHIVE_EXTRACT INPUT "${_sqlite_archive}" DESTINATION "${_sqlite_manual_extract_dir}")
                    set(_sqlite_extracted_dir "${_sqlite_manual_extract_dir}")
                endif()
            else()
                # Already have extracted sqlite3.c; avoid re-extraction and preserve cache.
                set(_sqlite_extracted_dir "${CMAKE_BINARY_DIR}/_deps/sqlite_fetch_unpack")
            endif()

            if(_sqlite_source_from_cache)
                set(_sqlite3_source "${_sqlite_source_from_cache}")
            endif()

            # SQLite upstream source does not provide a CMake project we can use directly.
            # Build from the amalgamation sqlite3.c source file.
            if(NOT DEFINED _sqlite3_source)
                set(_sqlite3_source "${_sqlite_extracted_dir}/sqlite3.c")
            endif()
            if(NOT EXISTS "${_sqlite3_source}")
                message(STATUS "sqlite3.c not found at ${_sqlite3_source}; trying nested directory detect in ${_sqlite_extracted_dir}")

                file(GLOB _sqlite_entries "${_sqlite_extracted_dir}/*")
                foreach(_entry IN LISTS _sqlite_entries)
                    if(IS_DIRECTORY "${_entry}" AND EXISTS "${_entry}/sqlite3.c")
                        set(_sqlite3_source "${_entry}/sqlite3.c")
                        message(STATUS "Found sqlite3.c at ${_sqlite3_source} in nested folder")
                        break()
                    endif()
                endforeach()
            endif()

            if(NOT EXISTS "${_sqlite3_source}")
                message(STATUS "Still no sqlite3.c; searching recursively under ${_sqlite_extracted_dir}")
                file(GLOB_RECURSE _sqlite3_files "${_sqlite_extracted_dir}/*.c")
                foreach(_entry IN LISTS _sqlite3_files)
                    get_filename_component(_name "${_entry}" NAME)
                    if(_name STREQUAL "sqlite3.c")
                        set(_sqlite3_source "${_entry}")
                        message(STATUS "Found sqlite3.c at ${_sqlite3_source} via recursive scan")
                        break()
                    endif()
                endforeach()
            endif()

            if(EXISTS "${_sqlite3_source}")
                get_filename_component(_sqlite_include_dir "${_sqlite3_source}" DIRECTORY)
                add_library(sqlite STATIC "${_sqlite3_source}")
                target_include_directories(sqlite PUBLIC "${_sqlite_include_dir}")
                add_library(SQLite::SQLite3 ALIAS sqlite)
                set(REKINDLED_SQLITE_AVAILABLE TRUE)
            else()
                message(WARNING "Fetched SQLite archive does not include sqlite3.c (unexpected).")
            endif()
        endif()
    endif()  # end if(NOT REKINDLED_SQLITE_SOURCE_DIR_USED)

    # Ensure a usable sqlite target exists so downstream projects can link.
    # This is a safety net for environments where the vendored sqlite subdirectory
    # does not produce the expected `sqlite` target.
    if(NOT TARGET sqlite)
        message(WARNING "No sqlite target present after dependency resolution; creating an INTERFACE target to allow configuration to proceed.")
        add_library(sqlite INTERFACE)
        if(DEFINED SQLite3_LIBRARIES AND SQLite3_LIBRARIES)
            target_link_libraries(sqlite INTERFACE ${SQLite3_LIBRARIES})
        endif()
        if(DEFINED SQLite3_INCLUDE_DIRS AND SQLite3_INCLUDE_DIRS)
            target_include_directories(sqlite INTERFACE ${SQLite3_INCLUDE_DIRS})
        endif()
    endif()

# OpenSSL
# Standardized on vcpkg (or a system OpenSSL 3.x) as the provider. The project
# no longer builds OpenSSL from a fetched 1.1.1 source tree; the vcpkg toolchain
# (see vcpkg-toolchain.cmake / vcpkg.json) or a system OpenSSL development
# package supplies a supported OpenSSL 3.x.
if(DEFINED REKINDLED_OPENSSL_SOURCE_DIR AND REKINDLED_OPENSSL_SOURCE_DIR AND EXISTS "${REKINDLED_OPENSSL_SOURCE_DIR}/include/openssl/opensslv.h")
    # Escape hatch: build from a manually-provided local OpenSSL source tree.
    rekindled_append_dep_status("openssl: local source")
    message(STATUS "Building OpenSSL from REKINDLED_OPENSSL_SOURCE_DIR=${REKINDLED_OPENSSL_SOURCE_DIR}")
    set(_openssl_source_build_dir "${CMAKE_BINARY_DIR}/_deps/openssl-local-build")
    file(MAKE_DIRECTORY "${_openssl_source_build_dir}")
    add_subdirectory("${REKINDLED_OPENSSL_SOURCE_DIR}" "${_openssl_source_build_dir}")
    add_library(openssl INTERFACE)
    if(TARGET OpenSSL::Crypto AND TARGET OpenSSL::SSL)
        target_link_libraries(openssl INTERFACE OpenSSL::Crypto OpenSSL::SSL)
    else()
        target_link_libraries(openssl INTERFACE ssl crypto)
    endif()
    target_include_directories(openssl INTERFACE "${REKINDLED_OPENSSL_SOURCE_DIR}/include")
else()
    # Primary path: resolve OpenSSL 3.x through the active toolchain. With the
    # vcpkg toolchain this is vcpkg's prebuilt OpenSSL 3; otherwise a system
    # OpenSSL development package (e.g. libssl-dev on Linux).
    find_package(OpenSSL QUIET)
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR
            "OpenSSL 3.x was not found.\n"
            "Provide it via vcpkg (recommended; see vcpkg-toolchain.cmake / vcpkg.json) or install a\n"
            "system OpenSSL development package (e.g. 'libssl-dev' on Linux, or set VCPKG_ROOT to a vcpkg\n"
            "checkout). Alternatively, set REKINDLED_OPENSSL_SOURCE_DIR to a local OpenSSL source tree.")
    endif()

    add_library(openssl INTERFACE)
    target_link_libraries(openssl INTERFACE OpenSSL::Crypto OpenSSL::SSL)
    if(DEFINED OPENSSL_INCLUDE_DIR AND OPENSSL_INCLUDE_DIR)
        target_include_directories(openssl INTERFACE ${OPENSSL_INCLUDE_DIR})
    endif()

    # Record provenance (vcpkg toolchain active vs plain system package).
    if(DEFINED VCPKG_TARGET_TRIPLET)
        rekindled_append_dep_status("openssl: vcpkg ${OPENSSL_VERSION}")
    else()
        rekindled_append_dep_status("openssl: system ${OPENSSL_VERSION}")
    endif()
endif()

# Wire TLS support into cpp-httplib with the resolved OpenSSL.
# cpp-httplib is header-only and conditionally compiled on
# CPPHTTPLIB_OPENSSL_SUPPORT, so this must be a consistent INTERFACE property
# on the `httplib` target rather than a per-source #define (which would mix
# SSL and non-SSL variants across translation units).
if(TARGET httplib AND TARGET openssl)
    target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE openssl)
    rekindled_append_dep_status("httplib: TLS enabled via resolved openssl")
endif()

if (WIN32)
    add_subdirectory(detours)
endif()
