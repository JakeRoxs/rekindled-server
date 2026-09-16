# Wrapper toolchain file that locates vcpkg and delegates to its official toolchain.

if(DEFINED ENV{VCPKG_ROOT})
    set(_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
elseif(EXISTS "C:/Tools/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_VCPKG_ROOT "C:/Tools/vcpkg")
elseif(EXISTS "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_VCPKG_ROOT "$ENV{USERPROFILE}/vcpkg")
elseif(EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_VCPKG_ROOT "$ENV{HOME}/vcpkg")
endif()

message(STATUS "vcpkg-toolchain.cmake: Using VCPKG_ROOT=${_VCPKG_ROOT}")

if(_VCPKG_ROOT AND EXISTS "${_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    include("${_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
else()
    message(WARNING "vcpkg not found; falling back to system packages or vendored sources.")
endif()
