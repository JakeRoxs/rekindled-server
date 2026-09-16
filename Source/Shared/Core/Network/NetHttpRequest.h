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

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#endif

// Simple class for sending HTTP requests. Implemented on cpp-httplib, which
// replaces libcurl as the master-server / outbound HTTP client.

enum class NetHttpMethod {
  OPTIONS,
  GET,
  HEAD,
  POST,
  PUT,
  METHOD_DELETE,
  TRACE,
  CONNECT
};

class NetHttpResponse {
public:
  std::vector<uint8_t> GetBody();
  bool GetWasSuccess();

private:
  friend class NetHttpRequest;

  std::vector<uint8_t> Body;
  bool WasSuccess = false;
};

class NetHttpRequest {
public:
  NetHttpRequest() = default;
  ~NetHttpRequest();

  NetHttpRequest(const NetHttpRequest&) = delete;
  NetHttpRequest& operator=(const NetHttpRequest&) = delete;

  void SetUrl(const std::string& Path);
  void SetMethod(NetHttpMethod Method);
  void SetBody(const std::string& Body);
  void SetBody(const std::vector<uint8_t>& Body);

  bool Send();
  bool SendAsync();

  bool InProgress();

  std::shared_ptr<NetHttpResponse> GetResponse();

private:
  void Execute();

  std::vector<uint8_t> Body;
  NetHttpMethod Method = NetHttpMethod::GET;
  std::string Url = "/";
  std::shared_ptr<NetHttpResponse> Response;

  std::thread AsyncThread;
  std::atomic<bool> Started{false};
  std::atomic<bool> Done{false};
};
