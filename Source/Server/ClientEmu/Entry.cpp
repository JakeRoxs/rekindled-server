/*
 * Rekindled Server Client Emulator
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "../Client/ClientSession.h"
#include "../Server/Database/ServerDatabase.h"
#include "../../Shared/Core/Crypto/RSAKeyPair.h"
#include "../../Shared/Core/Utils/Logging.h"
#include "../../Shared/Core/Utils/Strings.h"
#include "../../Shared/Platform/Platform.h"

#include <thread>
#include <filesystem>

#include <steam/steam_api.h>
#include <steam/isteamuser.h>

extern "C" void __cdecl SteamWarningHook(int nSeverity, const char* pchDebugText) {
  LogS("Steam", "%i: %s", nSeverity, pchDebugText);
}

// Simple client emulator that connects to a Rekindled server
// and simulates client traffic for load testing and debugging.
//
// Usage: ClientEmu [-count N] [-ip ADDRESS] [-port PORT]
//   -count N    Number of simulated clients (default: 1)
//   -ip ADDRESS Server IP address (default: 127.0.0.1)
//   -port PORT  Server port (default: 50050)

int main(int argc, char* argv[]) {
  int ClientCount = 1;
  std::string ServerIP = "127.0.0.1";
  int ServerPort = 50050;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-count" && i + 1 < argc) {
      ClientCount = atoi(argv[++i]);
    } else if (arg == "-ip" && i + 1 < argc) {
      ServerIP = argv[++i];
    } else if (arg == "-port" && i + 1 < argc) {
      ServerPort = atoi(argv[++i]);
    }
  }

  Log("Client Emulator");
  Log("  Count: %d", ClientCount);
  Log("  Server: %s:%d", ServerIP.c_str(), ServerPort);

  if (!PlatformInit()) {
    Error("Failed to initialize platform specific functionality.");
    return 1;
  }

  if (!SteamAPI_Init()) {
    Error("Failed to initialize steam api, please ensure steam is running.");
    return 1;
  }

  SteamUtils()->SetWarningMessageHook(&SteamWarningHook);

  std::vector<std::thread> ClientThreads;
  ClientThreads.reserve(ClientCount);

  for (int i = 0; i < ClientCount; i++) {
    ClientThreads.emplace_back([i, ServerIP, ServerPort]() {
      RSAKeyPair KeyPair;

      std::string DefaultPublicKey =
          "-----BEGIN RSA PUBLIC KEY-----\n"
          "MIIBCgKCAQEAtSGwOqmYyMldifSB99oqPc4jnWbOvtU9441/anExQtajz8AGA+V2\n"
          "uq9s6PNGZCkCCFYlxq7iXr+PTrL20irkqyNAX8Fjub+hckwBFtOGWOf2/ENJk9A8\n"
          "uyhfpmOVZ9+qB76ZcdwdSVWrCmzlgKjPU2RVz0moE1CHFtBr6gfdG+LlUBUEHr1X\n"
          "lnMlhNdRni+9Ju8X3Mt/EEdS++F+1s8/9VVMdf7RCPru09rR2fc9sD72DB7d8WeH\n"
          "MJssXGmcb6sZsU0u/3zNS8lGatDLivSwRrxeOUeUCIgu8ZrSTq0fCnHjUZ2WU6im\n"
          "Df1boE+E786Rf9cyK6I61zSUDMaqke7f8QIDAQAB\n"
          "-----END RSA PUBLIC KEY-----\n";

      if (!KeyPair.LoadPublicKeyFromString(DefaultPublicKey)) {
        ErrorS("Client", "Client-%d: Failed to load rsa keypair.", i);
        return;
      }

      ClientSession Session(KeyPair);
      Session.SetServerIP(ServerIP);
      Session.SetServerPort(ServerPort);
      Session.SetServerPublicKey(DefaultPublicKey);

      if (!Session.Init()) {
        ErrorS("Client", "Client-%d: Failed to initialize.", i);
        return;
      }

      Session.Run();

      Session.Term();
    });
  }

  for (auto& thread : ClientThreads) {
    thread.join();
  }

  SteamAPI_Shutdown();

  if (!PlatformTerm()) {
    Error("Failed to tidy up platform specific functionality.");
    return 1;
  }

  return 0;
}
