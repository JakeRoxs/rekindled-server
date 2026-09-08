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

// /statistics
//
//		GET		- Gets current settings state for server.
//		POST	- Sets current settings state for server.

class SettingsHandler : public WebUIHandler {
public:
  SettingsHandler(WebUIService* InService);

  void HandleGet(const httplib::Request& Req, httplib::Response& Res);
  void HandlePost(const httplib::Request& Req, httplib::Response& Res);

  virtual void Register(httplib::Server* Server) override;

protected:
  bool IsWeaponLevelMatchingDisabled();
  bool IsSoulLevelMatchingDisabled();
  bool IsSoulMemoryMatchingDisabled();
};