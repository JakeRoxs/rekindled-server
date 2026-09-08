# ================================================================================================
#  Rekindled Server
#  Copyright (C) 2021 Tim Leonard
#  Copyright (C) 2026 Jake Morgeson
# ================================================================================================

project(Shared C CXX)

SET(SOURCES   
    Core/Crypto/Cipher.h
    Core/Crypto/CWCCipher.cpp
    Core/Crypto/CWCCipher.h
    Core/Crypto/CWCClientUDPCipher.cpp
    Core/Crypto/CWCClientUDPCipher.h
    Core/Crypto/CWCServerUDPCipher.cpp
    Core/Crypto/CWCServerUDPCipher.h
    Core/Crypto/RSACipher.cpp
    Core/Crypto/RSACipher.h
    Core/Crypto/RSAKeyPair.cpp
    Core/Crypto/RSAKeyPair.h
    Core/Network/NetConnection.h
    Core/Network/NetConnectionTCP.cpp
    Core/Network/NetConnectionTCP.h
    Core/Network/NetConnectionUDP.cpp
    Core/Network/NetConnectionUDP.h
    Core/Network/NetHttpRequest.cpp
    Core/Network/NetHttpRequest.h
    Core/Network/NetIPAddress.cpp
    Core/Network/NetIPAddress.h
    Core/Network/NetUtils.cpp
    Core/Network/NetUtils.h
    Core/Utils/Compression.cpp
    Core/Utils/Compression.h
    Core/Utils/DebugCounter.cpp
    Core/Utils/DebugCounter.h
    Core/Utils/DebugObjects.cpp
    Core/Utils/DebugObjects.h
    Core/Utils/DebugObjects.inc
    Core/Utils/DebugTimer.cpp
    Core/Utils/DebugTimer.h
    Core/Utils/DiffTracker.cpp
    Core/Utils/DiffTracker.h
    Core/Utils/Endian.h
    Core/Utils/Enum.h
    Core/Utils/EnumDef.inc
    Core/Utils/Event.h
    Core/Utils/File.cpp
    Core/Utils/File.h
    Core/Utils/Logging.cpp
    Core/Utils/Logging.h
    Core/Utils/Random.cpp
    Core/Utils/Random.h
    Core/Utils/Strings.cpp
    Core/Utils/Strings.h
    Core/Utils/WinApi.cpp
    Core/Utils/WinApi.h
    Core/Utils/Rtti.cpp
    Core/Utils/Rtti.h
    Core/Utils/Protobuf.cpp
    Core/Utils/Protobuf.h

    
    Game/GameType.cpp
    Game/GameType.h
    Game/PlayerDataUtils.h

    Platform/Platform.h
)
 
if (WIN32)
    SET(PLATFORM_SOURCES
        Platform/Win32/Win32Platform.cpp
    )

    # Let the selected toolchain resolve its own SDK and runtime libraries.
    set(PLATFORM_LIBRARIES
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid
        comdlg32 advapi32 rpcrt4 dbghelp crypt32 ws2_32
    )

elseif (UNIX)
    SET(PLATFORM_SOURCES
        Platform/Linux/LinuxPlatform.cpp
    )
    SET(PLATFORM_LIBRARIES         
        uuid
    )
endif()
 
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${PLATFORM_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../")

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WINSOCKAPI_
        _WIN32_WINNT=0x0A00
    )
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    )
endif()

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    aes_modes
    lib_generic_c
    zlib
    libcurl
    # OpenSSL target from ThirdParty/openssl (resolved by ThirdParty/CMakeLists)
    openssl
    # Shared exposes protobuf headers in its public interface, so consumers need transitive protobuf include support.
    libprotobuf-lite
    ${PLATFORM_LIBRARIES}
)

# Ensure OpenSSL external project is built before Shared, because openssl.conf is generated
# and libcrypto/libssl are produced in the openssl install output.
if(TARGET openssl_fetched_build)
    add_dependencies(${PROJECT_NAME} openssl_fetched_build)
endif()

# Avoid direct legacy link names (crypto/ssl) in case target import names differ on MSVC.
# ThirdParty creates the correct openssl interface target with proper OpenSSL::Crypto/OpenSSL::SSL mapping.

# build unit tests for shared utilities (if any)
if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
