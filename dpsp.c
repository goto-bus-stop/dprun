#include "shared.h"
#include "dpsp.h"
#include <stdio.h>

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
  memcpy(&g_dprun_path[end - 4], ".dll", 3);
  return g_dprun_path;
}

HRESULT dpsp_register() {
  char* dprun_path = get_dprun_path();
  char* dprun_desc = "DPRun Lobbying Proxy";
  char dprun_guid[39];
  wchar_t* dprun_guidw;
  StringFromIID(&DPSPGUID_DPRUN, &dprun_guidw);

  for (int i = 0; i < 38; i++) {
    // Lossy cast is fine here bc. it only includes ascii chars
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

  if (result != ERROR_SUCCESS) {
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

  if (result != ERROR_SUCCESS) {
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
  return emit("CreatePlayeer", data, sizeof(DPSP_CREATEPLAYERDATA));
}
static HRESULT WINAPI callback_DeletePlayer(DPSP_DELETEPLAYERDATA* data) {
  return emit("DeletePlayer", data, sizeof(DPSP_DELETEPLAYERDATA));
}
static HRESULT WINAPI callback_GetAddress(DPSP_GETADDRESSDATA* data) {
  return emit("GetAddress", data, sizeof(DPSP_GETADDRESSDATA));
}
static HRESULT WINAPI callback_GetCaps(DPSP_GETCAPSDATA* data) {
  return emit("GetCaps", data, sizeof(DPSP_GETCAPSDATA));
}
static HRESULT WINAPI callback_Open(DPSP_OPENDATA* data) {
  return emit("Open", data, sizeof(DPSP_OPENDATA));
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

HRESULT dpsp_init(SPINITDATA* init_data) {
  dbglog = fopen("dprun_log.txt", "w");
  wchar_t* gotguid;
  wchar_t* selfguid;
  StringFromIID(init_data->lpGuid, &gotguid);
  StringFromIID(&DPSPGUID_DPRUN, &selfguid);
  fprintf(dbglog, "guid: %S %S\n", gotguid, selfguid);
  CoTaskMemFree(gotguid); CoTaskMemFree(selfguid);

  if (!IsEqualGUID(init_data->lpGuid, &DPSPGUID_DPRUN)) {
    fprintf(dbglog, "not equal\n");
    return DPERR_UNAVAILABLE;
  }

  fprintf(dbglog, "equal\n");

  init_data->dwSPHeaderSize = 0;
  init_data->dwSPVersion = DPSP_MAJORVERSIONMASK & DPSP_MAJORVERSION;

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

  return DP_OK;
}
