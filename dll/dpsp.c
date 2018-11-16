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

HRESULT spsock_create(spsock** out_conn, LPDIRECTPLAYSP service_provider) {
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

static DWORD WINAPI spsock_receive_thread(void* context) {
  spsock* conn = context;

  fprintf(dbglog, "starting thread\n");

  // amount of header bytes that are included in the .size attribute,
  // subtract this from the .size attribute to get the frame data size
  const int header_bytes_in_size = sizeof(struct spsock_recvheader) - sizeof(unsigned int);

  struct spsock_recvheader header;
  while (TRUE) {
    int bytes = recv(conn->socket, (char*)&header, sizeof(header), MSG_PEEK);
    if (bytes == 0) {
      fprintf(dbglog, "stopping thread, socket closed\n");
      return 0;
    }
    fprintf(dbglog, "peeked %d\n", bytes);
    if (bytes != sizeof(header)) {
      continue;
    }
    // should have the entire thing now
    assert(recv(conn->socket, (char*)&header, sizeof(header), MSG_WAITALL) == sizeof(header));
    header.size = flip_endianness(header.size);
    header.msg_id = flip_endianness(header.msg_id);
    header.reply_id = flip_endianness(header.reply_id);
    header._obsolete_from_id_should_be_removed = flip_endianness(header._obsolete_from_id_should_be_removed);

    EnterCriticalSection(&conn->lock);

    fprintf(dbglog, "receiving message #%d of size %d, response to %d\n", header.msg_id, header.size, header.reply_id);
    DWORD data_size = header.size - header_bytes_in_size;
    char* data = calloc(1, data_size);
    if (recv(conn->socket, data, data_size, MSG_WAITALL) == 0) {
      fprintf(dbglog, "stopping thread, socket closed\n");
      return 0;
    }

    if (header.reply_id == 0xFFFFFFFF) {
      fprintf(dbglog, "handle DirectPlay message\n");
      IDirectPlaySP_HandleMessage(
          conn->service_provider,
          data + sizeof(GUID),
          data_size - sizeof(GUID),
          data);
    } else {
      // handle reply
    }

    LeaveCriticalSection(&conn->lock);
  }

  fprintf(dbglog, "stopping thread, ended\n");
  return 1;
}

HRESULT spsock_open(spsock* conn) {
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
    fprintf(dbglog, "INVALID_SOCKET:\n");
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
    fprintf(dbglog, "SOCKET_ERROR:\n");
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
    fprintf(dbglog, "THREAD ERROR: %s\n", get_error_message(result));
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

HRESULT spsock_close(spsock* conn) {
  if (conn->socket == INVALID_SOCKET) return DP_OK;

  shutdown(conn->socket, SD_BOTH);
  closesocket(conn->socket);
  conn->socket = INVALID_SOCKET;
  if (WaitForSingleObject(conn->receive_thread, 1000) == WAIT_TIMEOUT) {
    fprintf(dbglog, "!! terminating thread, should've exited cleanly !!");
    TerminateThread(conn->receive_thread, 1);
  }
  WSACleanup();

  return DP_OK;
}

HRESULT spsock_reply(spsock* conn, unsigned int reply_to_id, const char method[4], void* data, DWORD data_size) {
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

HRESULT spsock_send(spsock* conn, const char method[4], void* data, DWORD data_size) {
  return spsock_reply(conn, 0xFFFFFFFF, method, data, data_size);
}

spsock* spsock_load(LPDIRECTPLAYSP sp) {
  spsock* conn;
  DWORD data_size = 0;
  IDirectPlaySP_GetSPData(sp, (void**)&conn, &data_size, DPSET_LOCAL);
  if (conn == NULL) return NULL;

  EnterCriticalSection(&conn->lock);

  return conn;
}

void spsock_release(spsock* conn) {
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
  assert(sizeof(dpsp_header) >= message_size);
  memcpy(message, &header, sizeof(dpsp_header));
}

static HRESULT emit(const char* method, void* data, DWORD data_size) {
  fprintf(dbglog, "Emit(%s): ", method);
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
      fprintf(dbglog, "Could not connect to HostServer for EnumSessions\n");
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
  assert(conn != NULL);

  dpsp_header* source_header = data->lpSPMessageHeader;

  DWORD senddata_size = sizeof(struct spdata_reply) - 1 + data->dwMessageSize;

  // Add header to message to send.
  add_dpsp_header(data->lpMessage, data->dwMessageSize, conn);

  // Message wrapping … nameserver id may be unnecessary?
  struct spdata_reply* senddata = calloc(1, senddata_size);
  senddata->reply_to = source_header->sender;
  senddata->nameserver_id = data->idNameServer;
  senddata->message_size = data->dwMessageSize;
  memcpy(senddata->message, data->lpMessage, data->dwMessageSize);

  spsock_send(conn, DPSP_METHOD_REPLY, senddata, senddata_size);

  free(senddata);
  spsock_release(conn);

  return DP_OK;
}

static HRESULT WINAPI callback_Send(DPSP_SENDDATA* data) {
  emit("Send", data, sizeof(DPSP_SENDDATA));
  spsock* conn = spsock_load(data->lpISP);
  assert(conn->socket != INVALID_SOCKET);

  // Add header to the message.
  add_dpsp_header(data->lpMessage, data->dwMessageSize, conn);

  void* senddata = data->lpMessage;
  DWORD senddata_size = data->dwMessageSize;

  spsock_send(conn, DPSP_METHOD_SEND, senddata, senddata_size);

  spsock_release(conn);

  return DP_OK;
}

struct spdata_createplayer {
  DPID player_id;
  DWORD flags;
};
static HRESULT WINAPI callback_CreatePlayer(DPSP_CREATEPLAYERDATA* data) {
  emit("CreatePlayer", data, sizeof(DPSP_CREATEPLAYERDATA));
  spsock* conn = spsock_load(data->lpISP);
  assert(conn != NULL);

  struct spdata_createplayer senddata = {
    .player_id = data->idPlayer,
    .flags = data->dwFlags,
  };

  spsock_send(conn, DPSP_METHOD_CREATE_PLAYER, &senddata, sizeof(senddata));
  spsock_release(conn);

  return DP_OK;
}

static HRESULT WINAPI callback_DeletePlayer(DPSP_DELETEPLAYERDATA* data) {
  return emit("DeletePlayer", data, sizeof(DPSP_DELETEPLAYERDATA));
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
  sprintf(dbgname, "dprun_log_%s.txt", timestr);
#else
  char* dbgname = "dprun_log.txt";
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

  fprintf(dbglog, "SPInit\n");

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
    fprintf(dbglog, "enumaddress failed: %s\n", get_error_message(result));
    return result;
  }

  fprintf(dbglog, "address: %s:%s\n", conn->host_ip, conn->host_port);

  if (conn->host_ip == NULL || conn->host_port == NULL || IsEqualGUID(&conn->self_id, &GUID_NULL)) {
    return DPERR_INVALIDPARAM;
  }

  IDirectPlaySP_SetSPData(init_data->lpISP, conn, sizeof(*conn), DPSET_LOCAL);

  return DP_OK;
}
