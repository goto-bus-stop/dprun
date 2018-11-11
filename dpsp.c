#include "shared.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <assert.h>
#include "debug.h"
#include "dpsp.h"
#include <stdio.h>
#include <time.h>

#define DPSP_HEADER_SIZE 20
#define DPSP_METHOD_OPEN "open"
#define DPSP_METHOD_SEND "send"
#define DPSP_METHOD_REPLY "repl"
#define DPSP_METHOD_CREATE_PLAYER "crpl"
#define DPSP_METHOD_DELETE_PLAYER "dlpl"

static FILE* dbglog;

/**
 * Code below here executes in the host application.
 */

#define REG_DPRUN "Software\\Microsoft\\DirectPlay\\Service Providers\\DPRun"

static char g_dprun_path[MAX_PATH] = {0};
static char* get_dprun_path() {
  GetModuleFileName(NULL, g_dprun_path, MAX_PATH);
  // dprun.exe → dprun.dll
  // TODO try to make directplay call into the exe instead…
  int end = strlen(g_dprun_path);
  memcpy(&g_dprun_path[end - 4], ".dll", 5);
  return g_dprun_path;
}

HRESULT dpsp_register() {
  char* dprun_path = get_dprun_path();
  char* dprun_desc = "DPRun Lobbying Proxy";
  char dprun_guid[39];
  wchar_t* dprun_guidw;
  StringFromIID(&DPSPGUID_DPRUN, &dprun_guidw);

  for (int i = 0; i < 38; i++) {
    // Lossy cast is fine here bc it only includes ascii chars
    dprun_guid[i] = (char) dprun_guidw[i];
  }
  dprun_guid[38] = '\0';
  CoTaskMemFree(dprun_guidw);

  HKEY dprun_key;
  LSTATUS result = RegCreateKeyEx(
      HKEY_LOCAL_MACHINE,
      REG_DPRUN,
      0,
      NULL,
      REG_OPTION_VOLATILE, // Keep this key in memory only
      KEY_WRITE | KEY_WOW64_32KEY,
      NULL,
      &dprun_key,
      NULL);

  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "DescriptionA", 0, REG_SZ, (void*)dprun_desc, strlen(dprun_desc) + 1);
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "DescriptionW", 0, REG_SZ, (void*)dprun_desc, strlen(dprun_desc) + 1);
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "Guid", 0, REG_SZ, (void*)dprun_guid, strlen(dprun_guid) + 1);
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "Path", 0, REG_SZ, (void*)dprun_path, strlen(dprun_path) + 1);
  DWORD val = 0;
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "dwReserved1", 0, REG_DWORD, (void*)&val, sizeof(val));
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "dwReserved2", 0, REG_DWORD, (void*)&val, sizeof(val));
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "Private", 0, REG_DWORD, (void*)&val, sizeof(val));

  if (FAILED(result)) {
    return HRESULT_FROM_WIN32(result);
  }

  return DP_OK;
}

HRESULT dpsp_unregister() {
  LSTATUS result = RegDeleteKeyEx(
      HKEY_LOCAL_MACHINE,
      REG_DPRUN,
      KEY_WOW64_32KEY,
      0);

  if (FAILED(result)) {
    return HRESULT_FROM_WIN32(result);
  }

  return DP_OK;
}

/**
 * Code below here executes in the dll.
 */

typedef struct spsock {
  char host_ip[16];
  char host_port[6];
  SOCKET socket;
  CRITICAL_SECTION lock;
  HANDLE receive_thread;
  unsigned int msg_id;
} spsock;

static void spsock_print_error() {
  char *s = get_error_message(WSAGetLastError());
  fprintf(dbglog, "%s\n", s);
  CoTaskMemFree(s);
}

HRESULT spsock_create(spsock** out_conn) {
  spsock* conn = calloc(1, sizeof(spsock));
  conn->socket = INVALID_SOCKET;
  InitializeCriticalSection(&conn->lock);
  *out_conn = conn;
  return DP_OK;
}

struct spsock_recvheader {
  unsigned int size;
  unsigned int msg_id;
  unsigned int reply_id;
};
static DWORD WINAPI spsock_receive_thread(void* context) {
  spsock* conn = context;

  // amount of header bytes that are included in the .size attribute,
  // subtract this from the .size attribute to get the frame data size
  const int header_bytes_in_size = sizeof(struct spsock_recvheader) - sizeof(unsigned int);

  struct spsock_recvheader header;
  while (TRUE) {
    int bytes = recv(conn->socket, (char*)&header, sizeof(header), MSG_PEEK);
    if (bytes == 0) {
      return 0;
    }
    if (bytes != sizeof(header)) {
      continue;
    }
    // should have the entire thing now
    assert(recv(conn->socket, (char*)&header, sizeof(header), MSG_WAITALL) == sizeof(header));

    EnterCriticalSection(&conn->lock);

    fprintf(dbglog, "receiving message #%d of size %d, response to %d\n", header.msg_id, header.size, header.reply_id);
    char* data = calloc(1, header.size - header_bytes_in_size);
    if (recv(conn->socket, data, header.size - header_bytes_in_size, MSG_WAITALL) == 0) {
      return 0;
    }

    LeaveCriticalSection(&conn->lock);
  }
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

static int to_big_endian(int little) {
  return ((little & 0xFF000000) >> 24 ) |
         ((little & 0x00FF0000) >> 8) |
         ((little & 0x0000FF00) << 8) |
         ((little & 0x000000FF) << 24);
}

HRESULT spsock_send(spsock* conn, const char method[4], void* data, DWORD data_size) {
  if (conn->socket == INVALID_SOCKET) return DPERR_GENERIC;

  // | length | id | method | frame data |
  void* senddata = malloc(12 + data_size);
  unsigned int* header = (unsigned int*)senddata;
  header[0] = to_big_endian(8 + data_size);
  header[1] = to_big_endian(conn->msg_id);

  memcpy(&header[2], method, 4);
  memcpy(&header[3], data, data_size);

  send(conn->socket, senddata, 12 + data_size, 0);
  conn->msg_id++;
  if (conn->msg_id == 0xFFFFFFFF) {
    conn->msg_id = 0;
  }

  free(senddata);

  return DP_OK;
}

spsock* spsock_load(LPDIRECTPLAYSP sp) {
  spsock* conn;
  DWORD data_size = 0;
  IDirectPlaySP_GetSPData(sp, (void**)&conn, &data_size, DPSET_LOCAL);

  EnterCriticalSection(&conn->lock);

  return conn;
}

void spsock_release(spsock* conn) {
  LeaveCriticalSection(&conn->lock);
}

static HRESULT emit(const char* method, void* data, DWORD data_size) {
  fprintf(dbglog, "Emit(%s): ", method);
  for (int i = 0; i < data_size; i++) {
    fprintf(dbglog, "%02X", ((char*)data)[i]);
  }
  fprintf(dbglog, "\n");
  return DPERR_UNSUPPORTED;
}

static HRESULT WINAPI callback_EnumSessions(DPSP_ENUMSESSIONSDATA* data) {
  return emit("EnumSessions", data, sizeof(DPSP_ENUMSESSIONSDATA));
}

static HRESULT WINAPI callback_Reply(DPSP_REPLYDATA* data) {
  return emit("Reply", data, sizeof(DPSP_REPLYDATA));
}

static HRESULT WINAPI callback_Send(DPSP_SENDDATA* data) {
  return emit("Send", data, sizeof(DPSP_SENDDATA));
}

struct spdata_createplayer {
  DPID player_id;
  DWORD flags;
};
static HRESULT WINAPI callback_CreatePlayer(DPSP_CREATEPLAYERDATA* data) {
  emit("CreatePlayer", data, sizeof(DPSP_CREATEPLAYERDATA));
  spsock* conn = spsock_load(data->lpISP);

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
  emit("GetCaps", data, sizeof(DPSP_GETCAPSDATA));

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
  data->lpCaps->dwHeaderLength = DPSP_HEADER_SIZE;
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

  HRESULT result = spsock_open(conn);
  if (result != DP_OK) {
    spsock_release(conn);
    return result;
  }

  struct spdata_open senddata = {
    .create = data->bCreate,
    .return_status = data->bReturnStatus,
    .open_flags = data->dwOpenFlags,
    .session_flags = data->dwSessionFlags,
  };

  spsock_send(conn, DPSP_METHOD_OPEN, &senddata, sizeof(senddata));
  spsock_release(conn);

  return result;
}

static HRESULT WINAPI callback_CloseEx(DPSP_CLOSEDATA* data) {
  return emit("CloseEx", data, sizeof(DPSP_CLOSEDATA));
}

static HRESULT WINAPI callback_ShutdownEx(DPSP_SHUTDOWNDATA* data) {
  emit("ShutdownEx", data, sizeof(DPSP_SHUTDOWNDATA));

  spsock* conn = spsock_load(data->lpISP);

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
  if (!IsEqualGUID(init_data->lpGuid, &DPSPGUID_DPRUN)) {
    return DPERR_UNAVAILABLE;
  }

  dbglog = create_dbglog();

  fprintf(dbglog, "SPInit\n");

  init_data->dwSPHeaderSize = DPSP_HEADER_SIZE;
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
  HRESULT result = spsock_create(&conn);
  if (FAILED(result)) return result;

  result = IDirectPlaySP_EnumAddress(init_data->lpISP, parse_address, init_data->lpAddress, init_data->dwAddressSize, conn);

  if (FAILED(result)) {
    fprintf(dbglog, "enumaddress failed: %s\n", get_error_message(result));
    return result;
  }

  fprintf(dbglog, "address: %s:%s (from %ld)\n", conn->host_ip, conn->host_port, init_data->dwAddressSize);

  IDirectPlaySP_SetSPData(init_data->lpISP, conn, sizeof(*conn), DPSET_LOCAL);

  return DP_OK;
}
