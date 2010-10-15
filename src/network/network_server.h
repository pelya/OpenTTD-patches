/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_server.h Server part of the network protocol. */

#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#ifdef ENABLE_NETWORK

#include "network_internal.h"

class ServerNetworkGameSocketHandler;
typedef ServerNetworkGameSocketHandler NetworkClientSocket;
typedef Pool<NetworkClientSocket, ClientIndex, 8, MAX_CLIENT_SLOTS> NetworkClientSocketPool;
extern NetworkClientSocketPool _networkclientsocket_pool;

/** Class for handling the server side of the game connection. */
class ServerNetworkGameSocketHandler : public NetworkClientSocketPool::PoolItem<&_networkclientsocket_pool>, public NetworkGameSocketHandler {
protected:
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_JOIN);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GAME_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GETMAP);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ACK);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_CHAT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_QUIT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ERROR);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_RCON);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MOVE);

	NetworkRecvStatus SendCompanyInfo();
	NetworkRecvStatus SendNewGRFCheck();
	NetworkRecvStatus SendWelcome();
	NetworkRecvStatus SendWait();
	NetworkRecvStatus SendNeedGamePassword();
	NetworkRecvStatus SendNeedCompanyPassword();
public:
	ServerNetworkGameSocketHandler(SOCKET s);
	~ServerNetworkGameSocketHandler();

	NetworkRecvStatus CloseConnection(NetworkRecvStatus status);
	void GetClientName(char *client_name, size_t size) const;

	NetworkRecvStatus SendMap();
	NetworkRecvStatus SendErrorQuit(ClientID client_id, NetworkErrorCode errorno);
	NetworkRecvStatus SendQuit(ClientID client_id);
	NetworkRecvStatus SendShutdown();
	NetworkRecvStatus SendNewGame();
	NetworkRecvStatus SendRConResult(uint16 colour, const char *command);
	NetworkRecvStatus SendMove(ClientID client_id, CompanyID company_id);

	NetworkRecvStatus SendClientInfo(NetworkClientInfo *ci);
	NetworkRecvStatus SendError(NetworkErrorCode error);
	NetworkRecvStatus SendChat(NetworkAction action, ClientID client_id, bool self_send, const char *msg, int64 data);
	NetworkRecvStatus SendJoin(ClientID client_id);
	NetworkRecvStatus SendFrame();
	NetworkRecvStatus SendSync();
	NetworkRecvStatus SendCommand(const CommandPacket *cp);
	NetworkRecvStatus SendCompanyUpdate();
	NetworkRecvStatus SendConfigUpdate();
};

void NetworkServer_Tick(bool send_frame);

#define FOR_ALL_CLIENT_SOCKETS_FROM(var, start) FOR_ALL_ITEMS_FROM(NetworkClientSocket, clientsocket_index, var, start)
#define FOR_ALL_CLIENT_SOCKETS(var) FOR_ALL_CLIENT_SOCKETS_FROM(var, 0)

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkServerMonthlyLoop() {}
static inline void NetworkServerYearlyLoop() {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_SERVER_H */
