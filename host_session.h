#pragma once
#include <dplobby.h>
#include "dpwrap.h"

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
  GUID session_id;
  GUID application;
  GUID service_provider;
  // Don't touch dis
  char _private[sizeof(struct session_priv)];
};

typedef BOOL (*session_onmessage)(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobby_message* message);

struct session_init create_host_session();
struct session_init create_join_session();
HRESULT launch_session(struct session_init* desc);
HRESULT dplobby_process_messages(struct session_init* desc, session_onmessage callback);
