/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Server/Server.h"
#include "Server/GameService/GameService.h"
#include "Server/GameService/GameClient.h"
#include "Server/WebUIService/Handlers/BansHandler.h"
#include "Shared/Core/Network/NetConnection.h"

#include "Shared/Core/Utils/Logging.h"
#include "Shared/Core/Utils/Strings.h"

BansHandler::BansHandler(WebUIService* InService)
    : WebUIHandler(InService) {
}

void BansHandler::Register(httplib::Server* Server) {
  Server->Get("/bans", [this](const httplib::Request& Req, httplib::Response& Res) {
    HandleGet(Req, Res);
  });
  Server->Delete("/bans", [this](const httplib::Request& Req, httplib::Response& Res) {
    HandleDelete(Req, Res);
  });
}

void BansHandler::HandleGet(const httplib::Request& Req, httplib::Response& Res) {
  if (!Service->IsAuthenticated(&Req)) {
    SendError(Res, 401, "Token invalid.");
    return;
  }

  ServerDatabase& Database = Service->GetServer()->GetDatabase();
  std::vector<std::string> BannedSteamIds = Database.GetBannedSteamIds();

  nlohmann::json json;
  {
    auto bansArray = nlohmann::json::array();
    for (std::string& SteamId : BannedSteamIds) {
      auto banJson = nlohmann::json::object();

      uint64_t SteamId64;
      sscanf(SteamId.c_str(), "%016llx", &SteamId64);

      banJson["steamId64"] = std::to_string(SteamId64);
      banJson["steamId"] = SteamId;
      std::string reason = "Manual";

      std::vector<AntiCheatLog> logs = Database.GetAntiCheatLogs(SteamId);
      if (!logs.empty()) {
        reason = "";

        for (AntiCheatLog& log : logs) {
          if (!reason.empty()) {
            reason += "\n";
          }
          reason += StringFormat("%s: %s", log.TriggerName.c_str(), log.Extra.c_str());
        }
      }

      banJson["reason"] = reason;
      bansArray.push_back(banJson);
    }

    json["bans"] = bansArray;
  }

  RespondJson(Res, json);
}

void BansHandler::HandleDelete(const httplib::Request& Req, httplib::Response& Res) {
  if (!Service->IsAuthenticated(&Req)) {
    SendError(Res, 401, "Token invalid.");
    return;
  }

  nlohmann::json json;
  if (!ReadJson(Req, json) ||
      !json.contains("steamId")) {
    SendError(Res, 400, "Malformed body.");
    return;
  }

  std::string SteamId = json["steamId"];

  ServerDatabase& Database = Service->GetServer()->GetDatabase();

  LogS("WebUI", "Unbanning player: %s", SteamId.c_str());
  Database.UnbanPlayer(SteamId);

  nlohmann::json responseJson;
  RespondJson(Res, responseJson);
}