/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#pragma once

#include "Server/Database/ServerDatabase.h"
#include "Shared/Platform/Platform.h"
#include "Shared/Core/Crypto/RSAKeyPair.h"
#include "Shared/Core/Network/NetIPAddress.h"
#include "Shared/Core/Utils/Logging.h"

#include <memory>
#include <vector>
#include <filesystem>
#include <atomic>

#include <steam/steam_api.h>
#include <steam/isteamuser.h>

class ClientSession;

// This is a very-very-very simple client emulator. Its used to
// as a super simple way to server behaviour.
//
// The actual state machine logic is in ClientSession.
// This class provides configuration and lifecycle management.

class Client {
public:
  struct ClientConfig {
    std::string ServerIP = "127.0.0.1";
    int ServerPort = 50050;
    std::string ServerPublicKey =
        "-----BEGIN RSA PUBLIC KEY-----\n"
        "MIIBCgKCAQEAtSGwOqmYyMldifSB99oqPc4jnWbOvtU9441/anExQtajz8AGA+V2\n"
        "uq9s6PNGZCkCCFYlxq7iXr+PTrL20irkqyNAX8Fjub+hckwBFtOGWOf2/ENJk9A8\n"
        "uyhfpmOVZ9+qB76ZcdwdSVWrCmzlgKjPU2RVz0moE1CHFtBr6gfdG+LlUBUEHr1X\n"
        "lnMlhNdRni+9Ju8X3Mt/EEdS++F+1s8/9VVMdf7RCPru09rR2fc9sD72DB7d8WeH\n"
        "MJssXGmcb6sZsU0u/3zNS8lGatDLivSwRrxeOUeUCIgu8ZrSTq0fCnHjUZ2WU6im\n"
        "Df1boE+E786Rf9cyK6I61zSUDMaqke7f8QIDAQAB\n"
        "-----END RSA PUBLIC KEY-----\n";
    bool DisablePersistentData = false;
    size_t InstanceId = 0;
  };

  Client();
  explicit Client(const ClientConfig& config);
  ~Client();

  bool Init();
  void OverrideConfig(bool disablePersistentData, size_t instanceId);
  bool Term();
  void RunUntilQuit();

private:
  static inline std::atomic<size_t> gClientCount{0};
  bool WasConnected = false;

  RSAKeyPair PrimaryKeyPair;
  ServerDatabase Database;

  std::filesystem::path SavedPath;
  std::filesystem::path DatabasePath;

  ClientConfig Config;

  std::unique_ptr<ClientSession> Session;
};
