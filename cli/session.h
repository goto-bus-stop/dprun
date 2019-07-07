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

typedef struct session_desc {
  // Public config
  char* player_name;
  GUID session_id;
  GUID application;
  GUID service_provider;
  dpaddress* address;
  char is_host;
  // Don't touch dis
  char _private[sizeof(struct session_priv)];
} session_desc;

typedef BOOL (*session_onmessage)(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobbymsg* message);

struct session_desc session_create();
HRESULT session_launch(session_desc* desc);
HRESULT session_process_messages(session_desc* desc, session_onmessage callback);
BOOL session_process_message(session_desc* desc, session_onmessage callback);
HANDLE session_get_event(session_desc* desc);
int session_get_pid(session_desc* desc);
