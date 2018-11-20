#include "../shared.h"
#include <stdio.h>
#include <dplobby.h>
#include "dpwrap.h"
#include "../debug.h"

void dplobbymsg_free(dplobbymsg* message) {
  if (message->data != NULL) free(message->data);
  free(message);
}

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

HRESULT dplobby_receive_message(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobbymsg** out_message) {
  DWORD message_flags = 0;
  DWORD data_size = 0;

  HRESULT result = IDirectPlayLobby_ReceiveLobbyMessage(lobby, 0, app_id, &message_flags, NULL, &data_size);
  if (result != DPERR_BUFFERTOOSMALL) {
    return result;
  }

  message_flags = 0;
  void* data = calloc(1, data_size);
  if (data == NULL) return DPERR_OUTOFMEMORY;
  result = IDirectPlayLobby_ReceiveLobbyMessage(lobby, 0, app_id, &message_flags, data, &data_size);
  CHECK("ReceiveLobbyMessage", result);

  dplobbymsg* message = calloc(1, sizeof(dplobbymsg));
  message->flags = message_flags;
  message->data = data;
  message->data_size = data_size;

  *out_message = message;
  return DP_OK;
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

void dpsess_generate_id(LPDPSESSIONDESC2 session_desc) {
  CoCreateGuid(&session_desc->guidInstance);
}

void dpsess_set_id(LPDPSESSIONDESC2 session_desc, GUID session_id) {
  session_desc->guidInstance = session_id;
}

void dpsess_set_application(LPDPSESSIONDESC2 session_desc, GUID application) {
  session_desc->guidApplication = application;
}

HRESULT dpsess_create_host(GUID application, LPDPSESSIONDESC2* out_session_desc) {
  LPDPSESSIONDESC2 session_desc;
  HRESULT result = dpsess_create(&session_desc);
  CHECK("dpsess_create", result);

  dpsess_generate_id(session_desc);
  dpsess_set_application(session_desc, application);

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

HRESULT dpaddrelement_create(GUID data_type, void* data, DWORD data_size, DPCOMPOUNDADDRESSELEMENT** out_element) {
  DPCOMPOUNDADDRESSELEMENT* element = calloc(1, sizeof(DPCOMPOUNDADDRESSELEMENT));
  if (element == NULL) return DPERR_OUTOFMEMORY;
  element->guidDataType = data_type;
  element->dwDataSize = data_size;
  element->lpData = data;

  *out_element = element;
  return DP_OK;
}

HRESULT dpaddress_create(dpaddress** out_address) {
  dpaddress* address = calloc(1, sizeof(dpaddress));
  if (address == NULL) return DPERR_OUTOFMEMORY;
  address->elements = NULL;
  address->num_elements = 0;

  *out_address = address;
  return DP_OK;
}

HRESULT dpaddress_add(dpaddress* address, DPCOMPOUNDADDRESSELEMENT* element) {
  DWORD prev_num_elements = address->num_elements;
  DPCOMPOUNDADDRESSELEMENT* prev_elements = address->elements;

  address->elements = calloc(address->num_elements + 1, sizeof(DPCOMPOUNDADDRESSELEMENT));
  if (address->elements == NULL) return DPERR_OUTOFMEMORY;

  if (prev_elements != NULL) {
    memcpy(address->elements, prev_elements, sizeof(DPCOMPOUNDADDRESSELEMENT) * prev_num_elements);
  }
  memcpy(&address->elements[prev_num_elements], element, sizeof(DPCOMPOUNDADDRESSELEMENT));

  address->num_elements += 1;

  return DP_OK;
}

HRESULT dpaddress_create_element(dpaddress* address, GUID data_type, void* data, DWORD data_size) {
  DPCOMPOUNDADDRESSELEMENT* element = NULL;
  HRESULT result = dpaddrelement_create(data_type, data, data_size, &element);
  if (SUCCEEDED(result)) {
    result = dpaddress_add(address, element);
  }
  if (element != NULL) {
    free(element);
  }
  return result;
}

HRESULT dpaddress_finish(dpaddress* address, LPDIRECTPLAYLOBBY3A lobby, void** out_elements, DWORD* out_size) {
  DWORD size = 0;
  HRESULT result = IDirectPlayLobby_CreateCompoundAddress(
      lobby,
      address->elements,
      address->num_elements,
      NULL,
      &size);
  if (result != DPERR_BUFFERTOOSMALL) {
    return result;
  }

  void* data = malloc(size);
  result = IDirectPlayLobby_CreateCompoundAddress(
      lobby,
      address->elements,
      address->num_elements,
      data,
      &size);

  if (result != DP_OK) {
    return result;
  }

  *out_elements = data;
  *out_size = size;

  return DP_OK;
}
