#include <stdio.h>
#include <dplobby.h>
#include "host_session.h"
#include "debug.h"

#define CHECK(tag, result) if (result != DP_OK) { printf("%s failed: %s\n", tag, get_error_message(result)); return result; }

HRESULT dplobby_create(LPDIRECTPLAYLOBBY3A* out_lobby) {
  LPDIRECTPLAYLOBBY dpli;
  void* convert;

  HRESULT result = DirectPlayLobbyCreate(NULL, &dpli, NULL, NULL, 0);
  CHECK("DirectPlayLobbyCreate", result);

  result = IDirectPlayLobby_QueryInterface(dpli, &IID_IDirectPlayLobby3A, &convert);
  CHECK("QueryInterface", result);

  *out_lobby = (LPDIRECTPLAYLOBBY3A) convert;

  return result;
}

HRESULT dplobby_run_application(LPDIRECTPLAYLOBBY3A lobby, DWORD* app_id, LPDPLCONNECTION connection, HANDLE event) {
  return IDirectPlayLobby_RunApplication(lobby, 0, app_id, connection, event);
}

HRESULT dpname_create(char* name, LPDPNAME* out_name) {
  LPDPNAME dp_name = calloc(1, sizeof(DPNAME));
  if (dp_name == NULL) return DPERR_OUTOFMEMORY;

  dp_name->dwSize = sizeof(DPNAME);
  dp_name->dwFlags = 0;
  dp_name->lpszShortNameA = name;
  dp_name->lpszLongNameA = name;

  *out_name = dp_name;
  return DP_OK;
}

HRESULT dpsess_create(LPDPSESSIONDESC2* out_session_desc) {
  LPDPSESSIONDESC2 session_desc = calloc(1, sizeof(DPSESSIONDESC2));
  if (session_desc == NULL) return DPERR_OUTOFMEMORY;

  session_desc->dwSize = sizeof(DPSESSIONDESC2);
  session_desc->dwFlags = 0;
  session_desc->guidInstance = GUID_NULL;
  session_desc->guidApplication = GUID_NULL;
  session_desc->dwMaxPlayers = 0;
  session_desc->dwCurrentPlayers = 0;
  session_desc->lpszSessionNameA = "";
  session_desc->lpszPasswordA = "";
  session_desc->dwReserved1 = 0;
  session_desc->dwReserved2 = 0;
  session_desc->dwUser1 = 0;
  session_desc->dwUser2 = 0;
  session_desc->dwUser3 = 0;
  session_desc->dwUser4 = 0;

  *out_session_desc = session_desc;
  return DP_OK;
}

HRESULT dpsess_create_host(GUID application, LPDPSESSIONDESC2* out_session_desc) {
  LPDPSESSIONDESC2 session_desc;
  HRESULT result = dpsess_create(&session_desc);
  CHECK("dpsess_create", result);

  CoCreateGuid(&session_desc->guidInstance);
  session_desc->guidApplication = application;

  *out_session_desc = session_desc;
  return DP_OK;
}

HRESULT dpconn_create(LPDPSESSIONDESC2 session_desc, LPDPNAME player_name, LPDPLCONNECTION* out_connection) {
  LPDPLCONNECTION connection = calloc(1, sizeof(DPLCONNECTION));
  if (connection == NULL) return DPERR_OUTOFMEMORY;

  connection->dwSize = sizeof(DPLCONNECTION);
  connection->dwFlags = 0;
  connection->lpSessionDesc = session_desc;
  connection->lpPlayerName = player_name;
  connection->guidSP = GUID_NULL;
  connection->lpAddress = NULL;
  connection->dwAddressSize = 0;

  *out_connection = connection;
  return DP_OK;
}

void dpconn_set_host(LPDPLCONNECTION connection, char is_host) {
  connection->dwFlags = is_host ? DPLCONNECTION_CREATESESSION : DPLCONNECTION_JOINSESSION;
}

void dpconn_set_service_provider(LPDPLCONNECTION connection, GUID service_provider) {
  connection->guidSP = service_provider;
}

struct session_priv* get_private(struct session_init* desc) {
  return (struct session_priv*) desc->_private;
}

struct session_init create_host_session() {
  struct session_init desc = {
    .player_name = NULL,
    .application = GUID_NULL,
    .service_provider = GUID_NULL,
  };

  get_private(&desc)->dplobby = NULL;
  get_private(&desc)->app_id = 0;
  get_private(&desc)->message_event = CreateEvent(NULL, FALSE, FALSE, NULL);

  return desc;
}

HRESULT host_session(struct session_init* desc) {
  LPDIRECTPLAYLOBBY3A lobby = NULL;
  DWORD app_id;
  LPDPNAME dp_player_name = NULL;
  LPDPSESSIONDESC2 dp_session_desc = NULL;
  LPDPLCONNECTION dp_connection = NULL;
  HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);

  HRESULT result = dplobby_create(&lobby);
  CHECK("dplobby_create", result);

  result = dpname_create(desc->player_name, &dp_player_name);
  CHECK("dpname_create", result);

  result = dpsess_create_host(desc->application, &dp_session_desc);
  CHECK("dpsess_create_host", result);

  result = dpconn_create(dp_session_desc, dp_player_name, &dp_connection);
  CHECK("dpconn_create", result);
  dpconn_set_host(dp_connection, TRUE);
  dpconn_set_service_provider(dp_connection, desc->service_provider);

  result = dplobby_run_application(lobby, &app_id, dp_connection, event);
  CHECK("RunApplication", result);

  get_private(desc)->dplobby = lobby;
  get_private(desc)->app_id = app_id;
  get_private(desc)->message_event = event;

  // TODO also clean up when errors happen
  if (dp_connection != NULL) free(dp_connection);
  if (dp_session_desc != NULL) free(dp_session_desc);
  if (dp_player_name != NULL) free(dp_player_name);

  return result;
}

void dplobby_message_free(struct dplobby_message* message) {
  if (message->data != NULL) free(message->data);
  free(message);
}

BOOL _receive_message(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobby_onmessage callback) {
  DWORD message_flags = 0;
  DWORD data_size = 0;

  HRESULT result = IDirectPlayLobby_ReceiveLobbyMessage(lobby, 0, app_id, &message_flags, NULL, &data_size);
  if (result != DPERR_BUFFERTOOSMALL) {
    return FALSE;
  }

  message_flags = 0;
  void* data = calloc(1, data_size);
  result = IDirectPlayLobby_ReceiveLobbyMessage(lobby, 0, app_id, &message_flags, data, &data_size);

  struct dplobby_message* message = calloc(1, sizeof(struct dplobby_message));
  message->flags = message_flags;
  message->data = data;
  message->data_size = data_size;

  return callback(message);
}

HRESULT dplobby_process_messages(struct session_init* desc, dplobby_onmessage callback) {
  struct session_priv* data = get_private(desc);

  printf("App: %ld\n", data->app_id);

  while (WaitForSingleObject(data->message_event, INFINITE) == WAIT_OBJECT_0) {
    if (_receive_message(data->dplobby, data->app_id, callback) == FALSE) {
      break;
    }
  }

  return DP_OK;
}
