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

#include "Server/WebUIService/Handlers/WebUIHandler.h"
#include "Server/GameService/PlayerState.h"

#include <mutex>

class GameClient;

// /bans
//
//		GET		- Gets a list of all active bans.
//		DELETE	- Removes the ban for a specific steam-id.

class BansHandler : public WebUIHandler {
public:
  BansHandler(WebUIService* InService);

  void HandleGet(const httplib::Request& Req, httplib::Response& Res);
  void HandleDelete(const httplib::Request& Req, httplib::Response& Res);

  virtual void Register(httplib::Server* Server) override;
};