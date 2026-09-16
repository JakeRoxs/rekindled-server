/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Server/WebUIService/WebUIService.h"

#include "Server/WebUIService/Handlers/AuthHandler.h"
#include "Server/WebUIService/Handlers/PlayersHandler.h"
#include "Server/WebUIService/Handlers/StatisticsHandler.h"
#include "Server/WebUIService/Handlers/SettingsHandler.h"
#include "Server/WebUIService/Handlers/DebugStatisticsHandler.h"
#include "Server/WebUIService/Handlers/MessageHandler.h"
#include "Server/WebUIService/Handlers/BansHandler.h"
#include "Server/WebUIService/Handlers/ShardingHandler.h"

#include "Server/Server.h"
#include "Shared/Core/Utils/Logging.h"
#include "Shared/Core/Utils/Strings.h"
#include "Shared/Core/Utils/Random.h"
#include "Shared/Core/Utils/DebugObjects.h"

#include "Config/BuildConfig.h"
#include "Config/RuntimeConfig.h"

WebUIService::WebUIService(Server* OwningServer)
    : ServerInstance(OwningServer) {
  Handlers.push_back(std::make_shared<AuthHandler>(this));
  Handlers.push_back(std::make_shared<PlayersHandler>(this));
  Handlers.push_back(std::make_shared<StatisticsHandler>(this));
  Handlers.push_back(std::make_shared<SettingsHandler>(this));
  Handlers.push_back(std::make_shared<MessageHandler>(this));
  Handlers.push_back(std::make_shared<BansHandler>(this));
  Handlers.push_back(std::make_shared<DebugStatisticsHandler>(this));
  Handlers.push_back(std::make_shared<ShardingHandler>(this));
}

WebUIService::~WebUIService() {
  if (WebServer) {
    WebServer->stop();
  }
  if (WebThread.joinable()) {
    WebThread.join();
  }
}

bool WebUIService::Init() {
  int Port = ServerInstance->GetConfig().WebUIServerPort;

  std::filesystem::path StaticPath = std::filesystem::current_path() / "../../Source/WebUI/Static";
  if (!std::filesystem::exists(StaticPath)) {
    StaticPath = std::filesystem::current_path() / "WebUI/Static";
  }
  if (!std::filesystem::exists(StaticPath)) {
    ErrorS("WebUI", "Failed to find webui file-serving directory.");
    return false;
  }

  WebServer = std::make_unique<httplib::Server>();

  // Serve the static WebUI assets.
  WebServer->set_mount_point("/", StaticPath.string());

  for (auto Handler : Handlers) {
    Handler->Register(WebServer.get());
  }

  // Run the (blocking) listener on a background thread, since httplib's
  // listen() blocks for the lifetime of the server. Bind all interfaces so
  // the WebUI and sharding endpoints are reachable by local and remote clients.
  WebThread = std::thread([this, Port]() {
    if (!WebServer->listen("0.0.0.0", Port)) {
      ErrorS("WebUI", "Failed to start WebUI listener.");
    }
  });

  Log("WebUI service is now listening at http://localhost:%i/", Port);

  return true;
}

bool WebUIService::Term() {
  if (WebServer) {
    WebServer->stop();
  }
  if (WebThread.joinable()) {
    WebThread.join();
  }

  return true;
}

void WebUIService::Poll() {
  DebugTimerScope Scope(Debug::WebUIService_PollTime);

  ClearExpiredTokens();
  GatherData();
}

std::string WebUIService::GetName() {
  return "WebUI";
}

bool WebUIService::CheckAuthToken(const std::string& Token) {
  std::scoped_lock lock(StateMutex);

  return AuthTokens.find(Token) != AuthTokens.end();
}

std::string WebUIService::AddAuthToken() {
  std::scoped_lock lock(StateMutex);

  std::vector<uint8_t> Bytes;
  Bytes.resize(64);
  FillRandomBytes(Bytes.data(), (int)Bytes.size());

  AuthToken NewToken;
  NewToken.Token = BytesToHex(Bytes);
  NewToken.ExpireTime = GetSeconds() + BuildConfig::WEBUI_AUTH_TIMEOUT;
  AuthTokens[NewToken.Token] = NewToken;

  return NewToken.Token;
}

bool WebUIService::IsAuthenticated(const httplib::Request* Req) {
  std::scoped_lock lock(StateMutex);

  std::string AuthToken = Req->get_header_value("Auth-Token");
  if (AuthToken.empty()) {
    return false;
  }

  return CheckAuthToken(AuthToken);
}

void WebUIService::ClearExpiredTokens() {
  std::scoped_lock lock(StateMutex);

  double Time = GetSeconds();
  for (auto iter = AuthTokens.begin(); iter != AuthTokens.end(); /* empty */) {
    if (Time > iter->second.ExpireTime) {
      iter = AuthTokens.erase(iter);
    } else {
      iter++;
    }
  }
}

void WebUIService::GatherData() {
  for (auto Handler : Handlers) {
    if (Handler->NeedsDataGather()) {
      Handler->GatherData();
    }
  }
}
