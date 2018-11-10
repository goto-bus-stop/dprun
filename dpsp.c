#include "shared.h"
#include "debug.h"
#include "dpsp.h"
#include <stdio.h>
#include <time.h>

#define DPSP_HEADER_SIZE 20

static FILE* dbglog;

/**
 * Code below here executes in the host application.
 */

static const char* REG_DPRUN = "Software\\Microsoft\\DirectPlay\\Service Providers\\DPRun";

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

static HRESULT WINAPI callback_CreatePlayer(DPSP_CREATEPLAYERDATA* data) {
  return emit("CreatePlayer", data, sizeof(DPSP_CREATEPLAYERDATA));
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

  data->lpCaps->dwFlags = 0;
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

static HRESULT WINAPI callback_Open(DPSP_OPENDATA* data) {
  emit("Open", data, sizeof(DPSP_OPENDATA));

  // TODO open connection to host application
  return DP_OK;
}

static HRESULT WINAPI callback_CloseEx(DPSP_CLOSEDATA* data) {
  return emit("CloseEx", data, sizeof(DPSP_CLOSEDATA));
}

static HRESULT WINAPI callback_ShutdownEx(DPSP_SHUTDOWNDATA* data) {
  return emit("ShutdownEx", data, sizeof(DPSP_SHUTDOWNDATA));
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

struct sp_connect_data {
  char host_ip[16];
  char host_port[6];
};

BOOL WINAPI parse_address(REFGUID data_type, DWORD data_size, LPCVOID data, LPVOID context);

FILE* create_dbglog() {
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

  struct sp_connect_data* address_data = calloc(1, sizeof(struct sp_connect_data));
  HRESULT hr = IDirectPlaySP_EnumAddress(init_data->lpISP, parse_address, init_data->lpAddress, init_data->dwAddressSize, address_data);

  if (FAILED(hr)) {
    fprintf(dbglog, "enumaddress failed: %s\n", get_error_message(hr));
    return hr;
  }

  fprintf(dbglog, "address: %s:%s (from %ld)\n", address_data->host_ip, address_data->host_port, init_data->dwAddressSize);

  return DP_OK;
}

BOOL WINAPI parse_address(REFGUID data_type, DWORD data_size, LPCVOID data, LPVOID context) {
  struct sp_connect_data* address_data = context;
  if (IsEqualGUID(data_type, &DPAID_INet)) {
    memcpy(address_data->host_ip, data, data_size);
  } else if (IsEqualGUID(data_type, &DPAID_INetPort)) {
    sprintf(address_data->host_port, "%d", *(int*)data);
  }

  return TRUE;
}
