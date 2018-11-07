#pragma once

/**
 * DirectPlay wrappers
 */

typedef struct dpaddress {
  DPCOMPOUNDADDRESSELEMENT* elements;
  DWORD num_elements;
} dpaddress;

HRESULT dpaddrelement_create(GUID data_type, void* data, DWORD data_size, DPCOMPOUNDADDRESSELEMENT** out_element);
HRESULT dpaddress_create(dpaddress** out_address);
HRESULT dpaddress_add(dpaddress* address, DPCOMPOUNDADDRESSELEMENT* element);
HRESULT dpaddress_create_element(dpaddress* address, GUID data_type, void* data, DWORD data_size);
HRESULT dpaddress_finish(dpaddress* address, void** out_elements, DWORD* out_size);

typedef struct dplobby_message {
  DWORD flags;
  void* data;
  DWORD data_size;
} dplobby_message;
void dplobby_message_free(dplobby_message* message);

HRESULT dplobby_create(LPDIRECTPLAYLOBBY3A* out_lobby);
HRESULT dplobby_run_application(LPDIRECTPLAYLOBBY3A lobby, DWORD* app_id, LPDPLCONNECTION connection, HANDLE event);
HRESULT dplobby_receive_message(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobby_message** out_message);

HRESULT dpname_create(char* name, LPDPNAME* out_name);

HRESULT dpsess_create(LPDPSESSIONDESC2* out_session_desc);
HRESULT dpsess_create_host(GUID application, LPDPSESSIONDESC2* out_session_desc);

HRESULT dpconn_create(LPDPSESSIONDESC2 session_desc, LPDPNAME player_name, LPDPLCONNECTION* out_connection);
void dpconn_set_host(LPDPLCONNECTION connection, char is_host);
void dpconn_set_service_provider(LPDPLCONNECTION connection, GUID service_provider);
