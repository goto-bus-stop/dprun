#include "../shared.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <assert.h>
#include "../debug.h"
#include "dpsp.h"
#include <stdio.h>
#include <time.h>

#define DPSP_METHOD_ENUM_SESSIONS "enum"
#define DPSP_METHOD_OPEN "open"
#define DPSP_METHOD_SEND "send"
#define DPSP_METHOD_REPLY "repl"
#define DPSP_METHOD_CREATE_PLAYER "crpl"
#define DPSP_METHOD_DELETE_PLAYER "dlpl"

static FILE* dbglog;
static char log_getcaps = TRUE;

typedef struct spsock {
  char host_ip[16];
  char host_port[6];
  GUID self_id;
  LPDIRECTPLAYSP service_provider;
  SOCKET socket;
  CRITICAL_SECTION lock;
  HANDLE receive_thread;
  unsigned int next_msg_id;
} spsock;

static void spsock_print_error() {
  char *s = get_error_message(WSAGetLastError());
  fprintf(dbglog, "%s\n", s);
  CoTaskMemFree(s);
}

static int flip_endianness(int n) {
  return ((n & 0xFF000000) >> 24 ) |
         ((n & 0x00FF0000) >> 8) |
         ((n & 0x0000FF00) << 8) |
         ((n & 0x000000FF) << 24);
}

static HRESULT spsock_create(spsock** out_conn, LPDIRECTPLAYSP service_provider) {
  spsock* conn = calloc(1, sizeof(spsock));
  conn->service_provider = service_provider;
  conn->socket = INVALID_SOCKET;
  InitializeCriticalSection(&conn->lock);
  *out_conn = conn;
  return DP_OK;
}

struct spsock_recvheader {
  unsigned int size;
  unsigned int msg_id;
  unsigned int reply_id;
  unsigned int _obsolete_from_id_should_be_removed;
};

static void spsock_handle_dp_message(LPDIRECTPLAYSP sp, char* data, DWORD data_size, DWORD header_size) {
  fprintf(dbglog, "[spsock_handle_dp_message] IDirectPlaySP_HandleMessage(%p, %p, %ld, %p)\n",
      sp, data + header_size, data_size - header_size, data);
  IDirectPlaySP_HandleMessage(sp, data + header_size, data_size - header_size, data);
}

static DWORD WINAPI spsock_receive_thread(void* context) {
  spsock* conn = context;

  fprintf(dbglog, "[spsock_receive_thread] starting thread\n");

  // amount of header bytes that are included in the .size attribute,
  // subtract this from the .size attribute to get the frame data size
  const int header_bytes_in_size = sizeof(struct spsock_recvheader) - sizeof(unsigned int);

  struct spsock_recvheader header;
  while (TRUE) {
    int bytes = recv(conn->socket, (char*)&header, sizeof(header), MSG_PEEK);
    if (bytes == 0) {
      fprintf(dbglog, "[spsock_receive_thread] stopping thread, socket closed\n");
      return 0;
    }
    fprintf(dbglog, "[spsock_receive_thread] peeked %d\n", bytes);
    if (bytes != sizeof(header)) {
      continue;
    }
    // should have the entire thing now
    bytes = recv(conn->socket, (char*)&header, sizeof(header), MSG_WAITALL);
    fprintf(dbglog, "[spsock_receive_thread] assert(%d == %d)\n", bytes, sizeof(header));
    assert(bytes == sizeof(header));
    header.size = flip_endianness(header.size);
    header.msg_id = flip_endianness(header.msg_id);
    header.reply_id = flip_endianness(header.reply_id);
    header._obsolete_from_id_should_be_removed = flip_endianness(header._obsolete_from_id_should_be_removed);

    EnterCriticalSection(&conn->lock);

    fprintf(dbglog, "[spsock_receive_thread] receiving message #%d of size %d, response to %d\n", header.msg_id, header.size, header.reply_id);
    DWORD data_size = header.size - header_bytes_in_size;
    char* data = calloc(1, data_size);
    if (recv(conn->socket, data, data_size, MSG_WAITALL) == 0) {
      fprintf(dbglog, "[spsock_receive_thread] stopping thread, socket closed\n");
      LeaveCriticalSection(&conn->lock);
      return 0;
    }

    // Release lock before handling message,
    // because the message may trigger replies.
    LeaveCriticalSection(&conn->lock);

    if (header.reply_id == 0xFFFFFFFF) {
      fprintf(dbglog, "[spsock_receive_thread] handle DirectPlay message\n");
      spsock_handle_dp_message(conn->service_provider, data, data_size, sizeof(GUID));
    } else {
      // handle reply
    }
  }

  fprintf(dbglog, "[spsock_receive_thread] stopping thread, ended\n");
  return 1;
}

static HRESULT spsock_open(spsock* conn) {
  EnterCriticalSection(&conn->lock);

  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);

  struct addrinfo* info = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  getaddrinfo(conn->host_ip, conn->host_port, &hints, &info);

  SOCKET sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
  if (sock == INVALID_SOCKET) {
    HRESULT result = WSAGetLastError();
    fprintf(dbglog, "[spsock_open] INVALID_SOCKET:\n");
    spsock_print_error();
    freeaddrinfo(info);
    WSACleanup();
    LeaveCriticalSection(&conn->lock);
    return result;
  }

  int no_delay = 1;
  int send_buffer = 0;
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&no_delay, sizeof(no_delay)) == SOCKET_ERROR) {
    spsock_print_error();
  }
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void*)&send_buffer, sizeof(send_buffer)) == SOCKET_ERROR) {
    spsock_print_error();
  }

  if (connect(sock, info->ai_addr, info->ai_addrlen) == SOCKET_ERROR) {
    HRESULT result = WSAGetLastError();
    fprintf(dbglog, "[spsock_open] SOCKET_ERROR:\n");
    spsock_print_error();
    closesocket(sock);
    freeaddrinfo(info);
    WSACleanup();
    LeaveCriticalSection(&conn->lock);
    return result;
  }

  freeaddrinfo(info);

  HANDLE thread = CreateThread(NULL, 0, spsock_receive_thread, conn, 0, NULL);
  if (thread == NULL) {
    HRESULT result = GetLastError();
    fprintf(dbglog, "[spsock_open] THREAD ERROR: %s\n", get_error_message(result));
    closesocket(sock);
    WSACleanup();
    LeaveCriticalSection(&conn->lock);
    return result;
  }

  conn->socket = sock;
  conn->receive_thread = thread;

  LeaveCriticalSection(&conn->lock);
  return DP_OK;
}

static HRESULT spsock_close(spsock* conn) {
  if (conn->socket == INVALID_SOCKET) return DP_OK;
  fprintf(dbglog, "[spsock_close] shutting down socket\n");

  shutdown(conn->socket, SD_BOTH);
  closesocket(conn->socket);
  conn->socket = INVALID_SOCKET;
  if (WaitForSingleObject(conn->receive_thread, 1000) == WAIT_TIMEOUT) {
    fprintf(dbglog, "[spsock_close] !! terminating thread, should've exited cleanly !!\n");
    TerminateThread(conn->receive_thread, 1);
  }
  WSACleanup();

  return DP_OK;
}

static HRESULT spsock_reply(spsock* conn, unsigned int reply_to_id, const char method[4], void* data, DWORD data_size) {
  if (conn->socket == INVALID_SOCKET) return DPERR_GENERIC;

  // | length | id | reply | method | frame data |
  int senddata_size = 16 + data_size;
  void* senddata = malloc(senddata_size);
  unsigned int* header = (unsigned int*)senddata;
  // little to big endian
  header[0] = flip_endianness(senddata_size - sizeof(header[0]));
  header[1] = flip_endianness(conn->next_msg_id);
  header[2] = flip_endianness(reply_to_id);

  memcpy(&header[3], method, 4);
  memcpy(&header[4], data, data_size);

  send(conn->socket, senddata, senddata_size, 0);
  conn->next_msg_id++;
  if (conn->next_msg_id == 0xFFFFFFFF) {
    conn->next_msg_id = 0;
  }

  free(senddata);

  return DP_OK;
}

static HRESULT spsock_send(spsock* conn, const char method[4], void* data, DWORD data_size) {
  return spsock_reply(conn, 0xFFFFFFFF, method, data, data_size);
}

static spsock* spsock_load(LPDIRECTPLAYSP sp) {
  spsock* conn;
  DWORD data_size = 0;
  IDirectPlaySP_GetSPData(sp, (void**)&conn, &data_size, DPSET_LOCAL);
  if (conn == NULL) return NULL;

  EnterCriticalSection(&conn->lock);

  return conn;
}

static void spsock_release(spsock* conn) {
  LeaveCriticalSection(&conn->lock);
}

/**
 * Header for DirectPlay messages, identifying the player that sent it.
 *
 * MAYBE this can be a DPID instead? it's important that the host application
 * can relate this to players, so right now we let the host application decide
 * player GUIDs. A DPID is smaller and makes code on this C end simpler, but
 * it's not always available (EnumSessions has no DPID, maybe others), and I
 * don't know which ID to pick in case there are more than 1 players on the local
 * machine (which is the case for the game host).
 */
typedef struct dpsp_header {
  GUID sender;
} dpsp_header;

static void add_dpsp_header(void* message, DWORD message_size, spsock* conn) {
  // Add header to the message.
  dpsp_header header = {
    .sender = conn->self_id,
  };
  fprintf(dbglog, "[add_dpsp_header] assert(%d <= %ld)\n", sizeof(dpsp_header), message_size);
  assert(sizeof(dpsp_header) <= message_size);
  memcpy(message, &header, sizeof(dpsp_header));
}

static HRESULT get_player_guid(LPDIRECTPLAYSP sp, DPID player, GUID* guid_out) {
  if (player == 0) {
    fprintf(dbglog, "[get_player_guid] for nameserver\n");
    memcpy(guid_out, &GUID_NULL, sizeof(GUID));
    return DP_OK;
  }

  GUID* guid;
  DWORD guid_size = sizeof(GUID);
  fprintf(dbglog, "[get_player_guid] for player %ld\n", player);
  HRESULT result = IDirectPlaySP_GetSPPlayerData(
      sp, player, (void**)&guid, &guid_size, DPSET_REMOTE);
  if (SUCCEEDED(result)) {
    memcpy(guid_out, guid, sizeof(GUID));
  } else {
    fprintf(dbglog, "[get_player_guid] FAILED %s\n", get_error_message(result));
    memcpy(guid_out, &GUID_NULL, sizeof(GUID));
  }
  return result;
}

static HRESULT set_player_guid(LPDIRECTPLAYSP sp, DPID player, GUID guid) {
  char guidstr[GUID_STR_LEN];
  guid_stringify(&guid, guidstr);
  fprintf(dbglog, "Setting player GUID for %ld: %s\n", player, guidstr);

  return IDirectPlaySP_SetSPPlayerData(
      sp, player, &guid, sizeof(guid), DPSET_REMOTE);
}

static HRESULT emit(const char* method, void* data, DWORD data_size) {
  fprintf(dbglog, "[emit] %s: ", method);
  char* chars = data;
  for (int i = 0; i < data_size; i++) {
    fprintf(dbglog, "%02X", chars[i]);
  }
  fprintf(dbglog, "\n");
  return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI callback_EnumSessions(DPSP_ENUMSESSIONSDATA* data) {
  emit("EnumSessions", data, sizeof(DPSP_ENUMSESSIONSDATA));
  emit("DPSP_MSG_ENUMSESSIONS", data->lpMessage, data->dwMessageSize);
  spsock* conn = spsock_load(data->lpISP);

  if (conn->socket == INVALID_SOCKET) {
    HRESULT result = spsock_open(conn);
    if (FAILED(result)) {
      fprintf(dbglog, "[callback_EnumSessions] Could not connect to HostServer for EnumSessions\n");
      spsock_release(conn);
      return result;
    }
  }

  // Add header to the message.
  add_dpsp_header(data->lpMessage, data->dwMessageSize, conn);

  void* senddata = data->lpMessage;
  DWORD senddata_size = data->dwMessageSize;

  spsock_send(conn, DPSP_METHOD_ENUM_SESSIONS, senddata, senddata_size);

  spsock_release(conn);

  return DP_OK;
}

struct spdata_reply {
  GUID reply_to;
  DPID nameserver_id;
  DWORD message_size;
  char message[1];
};
static HRESULT WINAPI callback_Reply(DPSP_REPLYDATA* data) {
  emit("Reply", data, sizeof(DPSP_REPLYDATA));
  spsock* conn = spsock_load(data->lpISP);
  fprintf(dbglog, "[callback_Reply] assert(%p != NULL)\n", conn);
  assert(conn != NULL);

  fprintf(dbglog, "[callback_Reply] source_header\n");
  dpsp_header* source_header = data->lpSPMessageHeader;
  if (source_header == NULL) {
    fprintf(dbglog, "[callback_Reply] !! no message header, discarding !!\n");
    return DPERR_GENERIC;
  }

  DWORD senddata_size = sizeof(struct spdata_reply) - 1 + data->dwMessageSize;

  // Add header to message to send.
  fprintf(dbglog, "[callback_Reply] add_dpsp_header\n");
  add_dpsp_header(data->lpMessage, data->dwMessageSize, conn);

  // Message wrapping … nameserver id may be unnecessary?
  struct spdata_reply* senddata = calloc(1, senddata_size);
  fprintf(dbglog, "[callback_Reply] assert(%p != NULL); senddata_size=%ld\n", senddata, senddata_size);
  assert(senddata != NULL);
  fprintf(dbglog, "[callback_Reply] reply_to(%p)\n", source_header);
  senddata->reply_to = source_header->sender;
  fprintf(dbglog, "[callback_Reply] nameserver_id\n");
  senddata->nameserver_id = data->idNameServer;
  fprintf(dbglog, "[callback_Reply] message_size\n");
  senddata->message_size = data->dwMessageSize;
  fprintf(dbglog, "[callback_Reply] memcpy(%p, %p, %ld)\n", senddata->message, data->lpMessage, data->dwMessageSize);
  memcpy(senddata->message, data->lpMessage, data->dwMessageSize);

  fprintf(dbglog, "[callback_Reply] spsock_send\n");
  spsock_send(conn, DPSP_METHOD_REPLY, senddata, senddata_size);

  free(senddata);
  spsock_release(conn);

  return DP_OK;
}

struct spdata_send {
  DWORD flags;
  GUID player_to;
  GUID player_from;
  BOOL system_message;
  DWORD message_size;
  char message[1];
};
static HRESULT WINAPI callback_Send(DPSP_SENDDATA* data) {
  emit("Send", data, sizeof(DPSP_SENDDATA));
  spsock* conn = spsock_load(data->lpISP);
  fprintf(dbglog, "[callback_Send] assert(%p != NULL && %d != INVALID_SOCKET)\n", conn, conn != NULL ? conn->socket : INVALID_SOCKET);
  assert(conn != NULL && conn->socket != INVALID_SOCKET);

  DWORD senddata_size = sizeof(struct spdata_send) - 1 + data->dwMessageSize;

  // Add header to message to send.
  add_dpsp_header(data->lpMessage, data->dwMessageSize, conn);

  struct spdata_send* senddata = calloc(1, senddata_size);
  senddata->flags = data->dwFlags;
  senddata->player_to = GUID_NULL;
  senddata->player_from = GUID_NULL;
  senddata->system_message = data->bSystemMessage;
  senddata->message_size = data->dwMessageSize;
  memcpy(senddata->message, data->lpMessage, data->dwMessageSize);

  HRESULT result = get_player_guid(data->lpISP, data->idPlayerTo, &senddata->player_to);
  if (FAILED(result)) {
    fprintf(dbglog, "[callback_Send] !! no player GUID for player %ld\n", data->idPlayerTo);
    spsock_release(conn);
    free(senddata);
    return result;
  }

  result = get_player_guid(data->lpISP, data->idPlayerFrom, &senddata->player_from);
  if (FAILED(result)) {
    fprintf(dbglog, "[callback_Send] !! no player GUID for player %ld\n", data->idPlayerFrom);
    spsock_release(conn);
    free(senddata);
    return result;
  }

  fprintf(dbglog, "[callback_Send] Sending %ld bytes\n", senddata_size);
  spsock_send(conn, DPSP_METHOD_SEND, senddata, senddata_size);
  free(senddata);

  spsock_release(conn);

  return DP_OK;
}

struct spdata_createplayer {
  DPID player_id;
  GUID player_guid;
  DWORD flags;
};
static HRESULT WINAPI callback_CreatePlayer(DPSP_CREATEPLAYERDATA* data) {
  emit("CreatePlayer", data, sizeof(DPSP_CREATEPLAYERDATA));
  spsock* conn = spsock_load(data->lpISP);
  fprintf(dbglog, "[callback_CreatePlayer] assert(%p != NULL)\n", conn);
  assert(conn != NULL);

  if (data->dwFlags & 8) {
    HRESULT result = set_player_guid(data->lpISP, data->idPlayer, conn->self_id);
    if (FAILED(result)) {
      spsock_release(conn);
      return result;
    }
  }

  struct spdata_createplayer senddata = {
    .player_id = data->idPlayer,
    .player_guid = GUID_NULL,
    .flags = data->dwFlags,
  };
  HRESULT result = get_player_guid(data->lpISP, data->idPlayer, &senddata.player_guid);
  if (FAILED(result)) {
    spsock_release(conn);
    return result;
  }

  spsock_send(conn, DPSP_METHOD_CREATE_PLAYER, &senddata, sizeof(senddata));
  spsock_release(conn);

  return DP_OK;
}

struct spdata_deleteplayer {
  DPID player_id;
  DWORD flags;
};
static HRESULT WINAPI callback_DeletePlayer(DPSP_DELETEPLAYERDATA* data) {
  emit("DeletePlayer", data, sizeof(DPSP_DELETEPLAYERDATA));
  spsock* conn = spsock_load(data->lpISP);
  fprintf(dbglog, "[callback_DeletePlayer] assert(%p != NULL)\n", conn);
  assert(conn != NULL);

  struct spdata_deleteplayer senddata = {
    .player_id = data->idPlayer,
    .flags = data->dwFlags,
  };

  spsock_send(conn, DPSP_METHOD_DELETE_PLAYER, &senddata, sizeof(senddata));
  spsock_release(conn);

  return DP_OK;
}

static HRESULT WINAPI callback_GetAddress(DPSP_GETADDRESSDATA* data) {
  return emit("GetAddress", data, sizeof(DPSP_GETADDRESSDATA));
}

static HRESULT WINAPI callback_GetCaps(DPSP_GETCAPSDATA* data) {
  if (log_getcaps) {
    emit("GetCaps", data, sizeof(DPSP_GETCAPSDATA));
  }

  if (data->lpCaps->dwSize < sizeof(DPCAPS)) {
    return DPERR_INVALIDPARAMS;
  }

  data->lpCaps->dwFlags = DPCAPS_ASYNCSUPPORTED;
  data->lpCaps->dwMaxBufferSize = 1024;
  data->lpCaps->dwMaxQueueSize = 0;
  data->lpCaps->dwMaxPlayers = 65536;
  data->lpCaps->dwHundredBaud = 0;
  data->lpCaps->dwLatency = 50;
  data->lpCaps->dwMaxLocalPlayers = 65536;
  data->lpCaps->dwHeaderLength = sizeof(dpsp_header);
  data->lpCaps->dwTimeout = 500;

  return DP_OK;
}

struct spdata_open {
  char create;
  char return_status;
  int open_flags;
  int session_flags;
};
static HRESULT WINAPI callback_Open(DPSP_OPENDATA* data) {
  emit("Open", data, sizeof(DPSP_OPENDATA));
  spsock* conn = spsock_load(data->lpISP);

  if (conn->socket == INVALID_SOCKET) {
    HRESULT result = spsock_open(conn);
    if (result != DP_OK) {
      spsock_release(conn);
      return result;
    }
  }

  struct spdata_open senddata = {
    .create = data->bCreate,
    .return_status = data->bReturnStatus,
    .open_flags = data->dwOpenFlags,
    .session_flags = data->dwSessionFlags,
  };

  spsock_send(conn, DPSP_METHOD_OPEN, &senddata, sizeof(senddata));
  spsock_release(conn);

  log_getcaps = FALSE;

  return DP_OK;
}

static HRESULT WINAPI callback_CloseEx(DPSP_CLOSEDATA* data) {
  return emit("CloseEx", data, sizeof(DPSP_CLOSEDATA));
}

static HRESULT WINAPI callback_ShutdownEx(DPSP_SHUTDOWNDATA* data) {
  emit("ShutdownEx", data, sizeof(DPSP_SHUTDOWNDATA));

  spsock* conn = spsock_load(data->lpISP);
  if (conn == NULL) {
    return DP_OK;
  }

  HRESULT result = spsock_close(conn);
  if (result != DP_OK) {
    spsock_release(conn);
    return result;
  }

  spsock_release(conn);
  return DP_OK;
}

static HRESULT WINAPI callback_GetAddressChoices(DPSP_GETADDRESSCHOICESDATA* data) {
  return emit("GetAddressChoices", data, sizeof(DPSP_GETADDRESSCHOICESDATA));
}

static HRESULT WINAPI callback_SendEx(DPSP_SENDEXDATA* data) {
  return emit("SendEx", data, sizeof(DPSP_SENDEXDATA));
}

static HRESULT WINAPI callback_SendToGroupEx(DPSP_SENDTOGROUPEXDATA* data) {
  return emit("SendToGroupEx", data, sizeof(DPSP_SENDTOGROUPEXDATA));
}

static HRESULT WINAPI callback_Cancel(DPSP_CANCELDATA* data) {
  return emit("Cancel", data, sizeof(DPSP_CANCELDATA));
}

static HRESULT WINAPI callback_GetMessageQueue(DPSP_GETMESSAGEQUEUEDATA* data) {
  return emit("GetMessageQueue", data, sizeof(DPSP_GETMESSAGEQUEUEDATA));
}

static BOOL WINAPI parse_address(REFGUID data_type, DWORD data_size, LPCVOID data, LPVOID context) {
  spsock* conn = context;
  if (IsEqualGUID(data_type, &DPAID_INet)) {
    memcpy(conn->host_ip, data, data_size);
  } else if (IsEqualGUID(data_type, &DPAID_INetPort)) {
    const int* port_ptr = data;
    sprintf(conn->host_port, "%d", *port_ptr);
  } else if (IsEqualGUID(data_type, &DPAID_SelfID)) {
    fprintf(dbglog, "[parse_address] assert(%ld == %d)\n", data_size, sizeof(GUID));
    assert(data_size == sizeof(GUID));
    memcpy(&conn->self_id, data, data_size);
  }

  return TRUE;
}

static FILE* create_dbglog() {
#ifdef USE_TIMED_DBGLOG
  char timestr[60];
  time_t t; time(&t);
  struct tm* nf = localtime(&t);
  strftime(timestr, 60, "%Y-%m-%d_%H-%M-%S", nf);
  char dbgname[MAX_PATH];
  sprintf(dbgname, "c:\\dprun_log_%s.txt", timestr);
#else
  char* dbgname = "c:\\dprun_log.txt";
#endif
  return fopen(dbgname, "w");
}

HRESULT dpsp_init(SPINITDATA* init_data) {
  // TODO figure out why this crashes when SPInit is called as an exported exe
  // function rather than a dll function
  if (!IsEqualGUID(init_data->lpGuid, &DPSPGUID_DPRUN)) {
    return DPERR_UNAVAILABLE;
  }

  dbglog = create_dbglog();

  fprintf(dbglog, "[dpsp_init] SPInit()\n");

  init_data->dwSPHeaderSize = sizeof(dpsp_header);
  init_data->dwSPVersion = (DPSP_MAJORVERSIONMASK & DPSP_MAJORVERSION) | DPRUN_VERSION;

  init_data->lpCB->EnumSessions = callback_EnumSessions;
  init_data->lpCB->Reply = callback_Reply;
  init_data->lpCB->Send = callback_Send;
  init_data->lpCB->CreatePlayer = callback_CreatePlayer;
  init_data->lpCB->DeletePlayer = callback_DeletePlayer;
  init_data->lpCB->GetAddress = callback_GetAddress;
  init_data->lpCB->GetCaps = callback_GetCaps;
  init_data->lpCB->Open = callback_Open;
  init_data->lpCB->CloseEx = callback_CloseEx;
  init_data->lpCB->ShutdownEx = callback_ShutdownEx;
  init_data->lpCB->GetAddressChoices = callback_GetAddressChoices;
  init_data->lpCB->SendEx = callback_SendEx;
  init_data->lpCB->SendToGroupEx = callback_SendToGroupEx;
  init_data->lpCB->Cancel = callback_Cancel;
  init_data->lpCB->GetMessageQueue = callback_GetMessageQueue;

  // These are not defined by dpwsockx so it's
  // probably fine to not have them
  init_data->lpCB->GetAddressChoices = NULL;
  init_data->lpCB->SendEx = NULL;
  init_data->lpCB->SendToGroupEx = NULL;
  init_data->lpCB->Cancel = NULL;

  spsock* conn;
  HRESULT result = spsock_create(&conn, init_data->lpISP);
  if (FAILED(result)) return result;

  result = IDirectPlaySP_EnumAddress(init_data->lpISP, parse_address, init_data->lpAddress, init_data->dwAddressSize, conn);

  if (FAILED(result)) {
    fprintf(dbglog, "[dpsp_init] enumaddress failed: %s\n", get_error_message(result));
    return result;
  }

  fprintf(dbglog, "[dpsp_init] address: %s:%s\n", conn->host_ip, conn->host_port);

  if (conn->host_ip == NULL || conn->host_port == NULL || IsEqualGUID(&conn->self_id, &GUID_NULL)) {
    return DPERR_INVALIDPARAM;
  }

  IDirectPlaySP_SetSPData(init_data->lpISP, conn, sizeof(*conn), DPSET_LOCAL);

  return DP_OK;
}
