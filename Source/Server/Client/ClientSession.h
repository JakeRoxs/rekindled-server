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

#include "Shared/Core/Network/NetIPAddress.h"
#include "Shared/Core/Crypto/RSAKeyPair.h"
#include "Shared/Core/Utils/Logging.h"

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <cstdint>

#include <google/protobuf/message_lite.h>

#include <steam/steam_api.h>
#include <steam/isteamuser.h>

class NetConnection;
class Frpg2MessageStream;
class Frpg2ReliableUdpMessageStream;
struct Frpg2ReliableUdpMessage;
struct Frpg2Message;

class ClientSession {
public:
  explicit ClientSession(RSAKeyPair& KeyPair);
  ~ClientSession();

  bool Init();
  bool Term();
  void Run();

  void SetServerIP(const std::string& IP) { ServerIP = IP; }
  void SetServerPort(int Port) { ServerPort = Port; }
  void SetServerPublicKey(const std::string& Key) { ServerPublicKey = Key; }

private:
  enum class ClientState {
    LoginServer_Connect,
    LoginServer_RequestServerInfo,

    AuthServer_Connect,
    AuthServer_RequestHandshake,
    AuthServer_RequestServiceStatus,
    AuthServer_ExchangeKeyData,
    AuthServer_GetServerInfo,

    GameServer_Connect,
    GameServer_RequestWaitForUserLogin,
    GameServer_RequestGetAnnounceMessageList,
    GameServer_RequestUpdateLoginPlayerCharacter,
    GameServer_RequestUpdatePlayerStatus,
    GameServer_RequestUpdatePlayerCharacter,
    GameServer_RequestGetRightMatchingArea,
    GameServer_Idle,

    Complete
  };

  void Pump();
  void ChangeState(ClientState State);

  void Handle_LoginServer_Connect();
  void Handle_LoginServer_RequestServerInfo();

  void Handle_AuthServer_Connect();
  void Handle_AuthServer_RequestHandshake();
  void Handle_AuthServer_RequestServiceStatus();
  void Handle_AuthServer_ExchangeKeyData();
  void Handle_AuthServer_GetServerInfo();

  void Handle_GameServer_Connect();
  void Handle_GameServer_RequestWaitForUserLogin();
  void Handle_GameServer_RequestGetAnnounceMessageList();
  void Handle_GameServer_RequestUpdateLoginPlayerCharacter();
  void Handle_GameServer_RequestUpdatePlayerStatus();
  void Handle_GameServer_RequestUpdatePlayerCharacter();
  void Handle_GameServer_RequestGetRightMatchingArea();
  void Handle_GameServer_Idle();

  void WaitForNextMessage(std::shared_ptr<NetConnection> Connection, std::shared_ptr<Frpg2MessageStream> Stream, Frpg2Message& Output);
  void SendAndAwaitWaitForReply(google::protobuf::MessageLite* Request, Frpg2ReliableUdpMessage& Response);
  void SendAndAwaitWaitForReply(google::protobuf::MessageLite* Request, google::protobuf::MessageLite* Response);

  template <typename... Args>
  void Abort(const char* Format, Args... args) {
    ErrorS("Client", Format, args...);
    throw std::exception();
  }

private:
  RSAKeyPair& PrimaryKeyPair;

  ClientState State = ClientState::LoginServer_Connect;
  bool QuitReceived = false;

  std::shared_ptr<NetConnection> LoginServerConnection;
  std::shared_ptr<Frpg2MessageStream> LoginServerMessageStream;

  std::shared_ptr<NetConnection> AuthServerConnection;
  std::shared_ptr<Frpg2MessageStream> AuthServerMessageStream;

  std::shared_ptr<NetConnection> GameServerConnection;
  std::shared_ptr<Frpg2ReliableUdpMessageStream> GameServerMessageStream;

  std::string ServerIP = "127.0.0.1";
  int ServerPort = 50050;
  std::string ServerPublicKey;

  std::string ClientStreamId;
  int ClientAppVersion = 115;
  int LocalCharacterId = 10;
  int ServerCharacterId = 0;

  int ClientSoulLevel = 0;
  int ClientSoulMemory = 0;
  int ClientWeaponLevel = 0;

  std::string AuthServerIP;
  int AuthServerPort = 0;

  bool GotAppTicketResponse = false;
  bool HasAppTicket = false;
  std::vector<uint8_t> AppTicket;
  HAuthTicket AppTicketHandle = k_HAuthTicketInvalid;

  std::vector<uint8_t> GameServerCwcKey;
  uint64_t GameServerAuthToken = 0;
  std::string GameServerIP;
  int GameServerPort = 0;
  uint32_t GamePlayerId = 0;
};
