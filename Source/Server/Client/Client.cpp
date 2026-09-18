/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Client/Client.h"
#include "Client/ClientSession.h"
#include "Config/BuildConfig.h"
#include "Shared/Core/Utils/Logging.h"
#include "Shared/Core/Utils/File.h"
#include "Shared/Core/Utils/Strings.h"

#include <thread>
#include <chrono>

Client::Client()
    : Client(ClientConfig()) {
}

Client::Client(const ClientConfig& config)
    : Config(config) {
  SavedPath = std::filesystem::current_path() / std::filesystem::path("ClientSaved");
  DatabasePath = SavedPath / std::filesystem::path("database.sqlite");
}

Client::~Client() {
}

bool Client::Init() {
  if (!Config.DisablePersistentData) {
    if (!std::filesystem::is_directory(SavedPath)) {
      if (!std::filesystem::create_directories(SavedPath)) {
        ErrorS("Client", "Failed to create save path: %s", SavedPath.string().c_str());
        return false;
      }
    }

    if (!Database.Open(DatabasePath)) {
      ErrorS("Client", "Failed to open database at '%s'.", DatabasePath.string().c_str());
      return false;
    }
  }

  Session = std::make_unique<ClientSession>(PrimaryKeyPair);
  Session->SetServerIP(Config.ServerIP);
  Session->SetServerPort(Config.ServerPort);
  Session->SetServerPublicKey(Config.ServerPublicKey);

  if (!Session->Init()) {
    return false;
  }

  return true;
}

void Client::OverrideConfig(bool disablePersistentData, size_t instanceId) {
  Config.DisablePersistentData = disablePersistentData;
  Config.InstanceId = instanceId;
}

bool Client::Term() {
  if (Session) {
    Session->Term();
  }

  if (!Config.DisablePersistentData) {
    if (!Database.Close()) {
      ErrorS("Client", "Failed to close database.");
      return false;
    }
  }

  return true;
}

void Client::RunUntilQuit() {
  if (!Session) {
    ErrorS("Client", "Session not initialized.");
    return;
  }

  Session->Run();
}
