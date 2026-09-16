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

#include "Server/Service.h"

#include <httplib.h>

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>

class Server;
class WebUIHandler;

// The webui service hosts a very simple webserver that can be used to
// monitor the status of the server.

class WebUIService
    : public Service {
public:
  WebUIService(Server* OwningServer);
  virtual ~WebUIService();

  virtual bool Init() override;
  virtual bool Term() override;
  virtual void Poll() override;

  virtual std::string GetName() override;

  Server* GetServer() { return ServerInstance; }
  httplib::Server* GetWebServer() { return WebServer.get(); }

public:
  bool CheckAuthToken(const std::string& Token);
  std::string AddAuthToken();
  bool IsAuthenticated(const httplib::Request* Req);
  void ClearExpiredTokens();

  void GatherData();

private:
  struct AuthToken {
    std::string Token;
    double ExpireTime;
  };

  Server* ServerInstance;

  std::unique_ptr<httplib::Server> WebServer;
  std::thread WebThread;

  std::vector<std::shared_ptr<WebUIHandler>> Handlers;

  std::recursive_mutex StateMutex;
  std::unordered_map<std::string, AuthToken> AuthTokens;
};