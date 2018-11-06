#pragma once
#include <dplobby.h>

/**
 * DirectPlay wrappers
 */
HRESULT dplobby_create(LPDIRECTPLAYLOBBY3A* out_lobby);
HRESULT dplobby_run_application(LPDIRECTPLAYLOBBY3A lobby, DWORD* app_id, LPDPLCONNECTION connection, HANDLE event);

HRESULT dpname_create(char* name, LPDPNAME* out_name);

HRESULT dpsess_create(LPDPSESSIONDESC2* out_session_desc);
HRESULT dpsess_create_host(GUID application, LPDPSESSIONDESC2* out_session_desc);

HRESULT dpconn_create(LPDPSESSIONDESC2 session_desc, LPDPNAME player_name, LPDPLCONNECTION* out_connection);
void dpconn_set_host(LPDPLCONNECTION connection, char is_host);
void dpconn_set_service_provider(LPDPLCONNECTION connection, GUID service_provider);

/**
 * Session hosting
 */

struct session_priv {
  LPDIRECTPLAYLOBBY3A dplobby;
  DWORD app_id;
  HANDLE message_event;
};

struct session_init {
  // Public config
  char* player_name;
  GUID application;
  GUID service_provider;
  // Don't touch dis
  char _private[sizeof(struct session_priv)];
};

struct dplobby_message {
  DWORD flags;
  void* data;
  DWORD data_size;
};

typedef BOOL (*dplobby_onmessage)(struct dplobby_message* message);
void dplobby_message_free(struct dplobby_message* message);

struct session_init create_host_session();
HRESULT host_session(struct session_init* desc);
HRESULT dplobby_process_messages(struct session_init* desc, dplobby_onmessage callback);
