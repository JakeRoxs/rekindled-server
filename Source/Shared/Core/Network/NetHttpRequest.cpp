/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Shared/Core/Network/NetHttpRequest.h"
#include "Shared/Core/Utils/Logging.h"

// Outbound HTTPS requests (public IP lookup, Discord webhooks, master-server
// calls) use cpp-httplib's SSL client. TLS support is enabled project-wide on
// the `httplib` INTERFACE target (see ThirdParty/CMakeLists.txt).
#include <httplib.h>

namespace {

// Split "scheme://host:port/path" into the "scheme://host:port" base that the
// httplib::Client constructor expects, and the "/path" handed to each request.
bool SplitUrl(const std::string& Url, std::string& Base, std::string& Path) {
  auto SchemeEnd = Url.find("://");
  const size_t AfterScheme = (SchemeEnd == std::string::npos) ? 0 : SchemeEnd + 3;
  const auto PathStart = Url.find('/', AfterScheme);

  if (PathStart == std::string::npos) {
    Base = Url;
    Path = "/";
  } else {
    Base = Url.substr(0, PathStart);
    Path = Url.substr(PathStart);
  }
  return true;
}

} // namespace

NetHttpRequest::~NetHttpRequest() {
  if (AsyncThread.joinable()) {
    AsyncThread.join();
  }
}

std::vector<uint8_t> NetHttpResponse::GetBody() {
  return Body;
}

bool NetHttpResponse::GetWasSuccess() {
  return WasSuccess;
}

void NetHttpRequest::SetUrl(const std::string& InPath) {
  Url = InPath;
}

void NetHttpRequest::SetMethod(NetHttpMethod InMethod) {
  Method = InMethod;
}

void NetHttpRequest::SetBody(const std::vector<uint8_t>& InBody) {
  Body = InBody;
}

void NetHttpRequest::SetBody(const std::string& InBody) {
  Body.assign((uint8_t*)InBody.data(), (uint8_t*)InBody.data() + InBody.size());
}

void NetHttpRequest::Execute() {
  std::string Base;
  std::string Path;
  SplitUrl(Url, Base, Path);

  httplib::Client Client(Base);
  if (!Client.is_valid()) {
    if (Response) {
      Response->WasSuccess = false;
    }
    Done.store(true);
    return;
  }

  // Verify the server certificate for outbound TLS connections to prevent
  // man-in-the-middle tampering of master-server, webhook, and IP-lookup calls.
  Client.enable_server_certificate_verification(true);
  Client.set_connection_timeout(10);
  Client.set_read_timeout(10);

  httplib::Headers Headers = {
      {"Accept", "application/json"},
      {"Content-Type", "application/json"},
      {"charset", "utf-8"},
  };

  // Materialize the request body string only for methods that send a payload;
  // GET/HEAD/OPTIONS/TRACE/CONNECT never use it, so this avoids an unnecessary
  // allocation + copy on every request.
  std::string BodyStr;
  if (Method == NetHttpMethod::POST ||
      Method == NetHttpMethod::PUT ||
      Method == NetHttpMethod::METHOD_DELETE) {
    BodyStr.assign(Body.begin(), Body.end());
  }

  httplib::Result Result;
  switch (Method) {
  case NetHttpMethod::GET:
    Result = Client.Get(Path, Headers);
    break;
  case NetHttpMethod::HEAD:
    Result = Client.Head(Path, Headers);
    break;
  case NetHttpMethod::OPTIONS:
    Result = Client.Options(Path, Headers);
    break;
  case NetHttpMethod::POST:
    Result = Client.Post(Path, Headers, BodyStr, "application/json");
    break;
  case NetHttpMethod::PUT:
    Result = Client.Put(Path, Headers, BodyStr, "application/json");
    break;
  case NetHttpMethod::METHOD_DELETE:
    Result = Client.Delete(Path, Headers, BodyStr, "application/json");
    break;
  case NetHttpMethod::TRACE:
  case NetHttpMethod::CONNECT:
    // Not used by the server; httplib has no TRACE/CONNECT client call.
    break;
  }

  if (Response) {
    Response->WasSuccess = (bool)Result;
    if (Result) {
      const auto& Resp = *Result;
      Response->Body.assign(Resp.body.begin(), Resp.body.end());
    }
  }

  Done.store(true);
}

bool NetHttpRequest::Send() {
  if (InProgress()) {
    return false;
  }

  Response = std::make_shared<NetHttpResponse>();
  Started.store(true);
  Execute();
  return true;
}

bool NetHttpRequest::SendAsync() {
  if (InProgress()) {
    return false;
  }

  Response = std::make_shared<NetHttpResponse>();
  Started.store(true);
  AsyncThread = std::thread([this]() { Execute(); });
  return true;
}

bool NetHttpRequest::InProgress() {
  return Started.load() && !Done.load();
}

std::shared_ptr<NetHttpResponse> NetHttpRequest::GetResponse() {
  Ensure(!InProgress());
  return Response;
}
