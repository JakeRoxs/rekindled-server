/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Server/WebUIService/Handlers/ShardingHandler.h"
#include "Server/Server.h"
#include "Server/ServerManager.h"
#include "Server/GameService/GameService.h"
#include "Server/GameService/GameClient.h"

#include "Shared/Core/Utils/Logging.h"
#include "Shared/Core/Utils/Strings.h"

#include <thread>
#include <condition_variable>

ShardingHandler::ShardingHandler(WebUIService* InService)
    : WebUIHandler(InService) {
}

void ShardingHandler::Register(httplib::Server* Server) {
  Server->Post("/sharding", [this](const httplib::Request& Req, httplib::Response& Res) {
    HandlePost(Req, Res);
  });
}

void ShardingHandler::HandlePost(const httplib::Request& Req, httplib::Response& Res) {
  ServerManager& Manager = Service->GetServer()->GetManager();

  nlohmann::json json;
  if (!ReadJson(Req, json) ||
      !json.contains("serverName") ||
      !json.contains("serverPassword") ||
      !json.contains("serverGameType") ||
      !json.contains("machineId")) {
    SendError(Res, 400, "Malformed body.");
    return;
  }

  std::string ServerName = json["serverName"];
  std::string ServerPassword = json["serverPassword"];
  std::string ServerGameType = json["serverGameType"];
  std::string MachineId = json["machineId"];

  std::string UserIp = Req.remote_addr;
  if (size_t Pos = UserIp.find(":"); Pos != std::string::npos) {
    UserIp = UserIp.substr(0, Pos);
  }
  std::string RequestHash = UserIp + "|" + MachineId;
  std::string ServerId = "";

  Server* Instance = nullptr;

  GameType ServerEnumGameType = GameType::Unknown;
  if (!ParseGameType(ServerGameType.c_str(), ServerEnumGameType)) {
    SendError(Res, 400, "Malformed body, unknown game type.");
    return;
  }

  // Find existing server created by user, or created a new one.
  if (auto Iter = RequestHashToServerId.find(RequestHash); Iter != RequestHashToServerId.end()) {
    ServerId = Iter->second;
    Instance = Manager.FindServer(ServerId);
  }

  if (Instance == nullptr) {
    std::mutex mutex;
    std::condition_variable convar;
    bool Success = false;

    std::unique_lock lock(mutex);

    Manager.QueueCallback([&Manager, ServerName, ServerPassword, &ServerId, &ServerEnumGameType, &Success, &mutex, &convar]() mutable {
      std::unique_lock inner_lock(mutex);
      Success = Manager.NewServer(ServerName, ServerPassword, ServerEnumGameType, ServerId);
      convar.notify_all();
    });

    convar.wait(lock);

    if (!Success) {
      SendError(Res, 500, "Failed to start server.");
      return;
    }

    Instance = Manager.FindServer(ServerId);
    if (!Instance) {
      SendError(Res, 500, "Failed to find server.");
      return;
    }
  }

  RequestHashToServerId[RequestHash] = ServerId;

  // Send back a response with the login details for the server.
  const RuntimeConfig& Config = Instance->GetConfig();

  std::string Hostname = Config.ServerHostname.length() > 0 ? Config.ServerHostname : Instance->GetPublicIP().ToString();

  nlohmann::json responseJson;
  responseJson["id"] = Instance->GetId();
  responseJson["webUsername"] = Config.WebUIServerUsername;
  responseJson["webPassword"] = Config.WebUIServerPassword;
  responseJson["webUrl"] = StringFormat("http://%s:%i/", Hostname.c_str(), Config.WebUIServerPort);
  RespondJson(Res, responseJson);
}
