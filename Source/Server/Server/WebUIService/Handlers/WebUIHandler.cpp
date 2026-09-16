/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Server/WebUIService/Handlers/WebUIHandler.h"

#include "Shared/Platform/Platform.h"

#include "Config/BuildConfig.h"

WebUIHandler::WebUIHandler(WebUIService* InService)
    : Service(InService) {
}

void WebUIHandler::RespondJson(httplib::Response& Res, nlohmann::json& Json) {
  std::string result = Json.dump(4);

  Res.set_content(std::move(result), "application/json; charset=utf-8");
}

bool WebUIHandler::ReadJson(const httplib::Request& Req, nlohmann::json& Json) {
  try {
    Json = nlohmann::json::parse(Req.body);
  } catch (nlohmann::json::parse_error) {
    return false;
  }

  return true;
}

void WebUIHandler::SendError(httplib::Response& Res, int Status, const std::string& Message) {
  Res.status = Status;
  Res.set_content(Message, "text/plain; charset=utf-8");
}

bool WebUIHandler::NeedsDataGather() {
  double Time = GetSeconds();

  if (Time - LastDataGather < BuildConfig::WEBUI_GATHER_DATA_MIN_INTERVAL) {
    return false;
  }

  if (Time - LastMarkedAsNeedingDataGather > BuildConfig::WEBUI_GATHER_DATA_TIMEOUT) {
    return false;
  }

  LastDataGather = GetSeconds();

  return true;
}

void WebUIHandler::MarkAsNeedsDataGather() {
  LastMarkedAsNeedingDataGather = GetSeconds();
}