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
#include "Server/WebUIService/Handlers/AuthHandler.h"

#include "Shared/Core/Utils/Logging.h"

AuthHandler::AuthHandler(WebUIService* InService)
    : WebUIHandler(InService) {
}

void AuthHandler::Register(httplib::Server* Server) {
  Server->Get("/auth", [this](const httplib::Request& Req, httplib::Response& Res) {
    HandleGet(Req, Res);
  });
  Server->Post("/auth", [this](const httplib::Request& Req, httplib::Response& Res) {
    HandlePost(Req, Res);
  });
}

void AuthHandler::HandleGet(const httplib::Request& Req, httplib::Response& Res) {
  if (!Service->IsAuthenticated(&Req)) {
    SendError(Res, 401, "Token invalid.");
    return;
  }

  nlohmann::json json;
  RespondJson(Res, json);
}

void AuthHandler::HandlePost(const httplib::Request& Req, httplib::Response& Res) {
  nlohmann::json json;
  if (!ReadJson(Req, json) ||
      !json.contains("username") ||
      !json.contains("password")) {
    SendError(Res, 400, "Malformed body.");
    return;
  }

  std::string Username = json["username"];
  std::string Password = json["password"];

  std::string CorrectUsername = Service->GetServer()->GetConfig().WebUIServerUsername;
  std::string CorrectPassword = Service->GetServer()->GetConfig().WebUIServerPassword;

  if (Username == CorrectUsername &&
      Password == CorrectPassword &&
      !CorrectUsername.empty() &&
      !CorrectPassword.empty()) {
    RuntimeConfig& Config = Service->GetServer()->GetMutableConfig();

    nlohmann::json json;
    json["token"] = Service->AddAuthToken();
    json["gameType"] = Config.GameType;

    LogS("WebUI", "User has logged in to webui.");

    RespondJson(Res, json);
  } else {
    SendError(Res, 401, "Token login.");
    return;
  }
}
