#include <stdio.h>
#include <dplobby.h>
#include "host_session.h"
#include "dpwrap.h"
#include "debug.h"

#define CHECK(tag, result) if (result != DP_OK) { printf("%s failed: %s\n", tag, get_error_message(result)); return result; }

struct session_priv* get_private(struct session_init* desc) {
  return (struct session_priv*) desc->_private;
}

void init_private(struct session_init* desc) {
  get_private(desc)->dplobby = NULL;
  get_private(desc)->app_id = 0;
  get_private(desc)->message_event = CreateEvent(NULL, FALSE, FALSE, NULL);
}

struct session_init create_host_session() {
  struct session_init desc = {
    .player_name = NULL,
    .session_id = GUID_NULL,
    .application = GUID_NULL,
    .service_provider = GUID_NULL,
  };

  init_private(&desc);

  return desc;
}

struct session_init create_join_session(GUID session_id) {
  struct session_init desc = {
    .player_name = NULL,
    .session_id = session_id,
    .application = GUID_NULL,
    .service_provider = GUID_NULL,
  };

  init_private(&desc);

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

  desc->session_id = dp_session_desc->guidInstance;

  get_private(desc)->dplobby = lobby;
  get_private(desc)->app_id = app_id;
  get_private(desc)->message_event = event;

  // TODO also clean up when errors happen
  if (dp_connection != NULL) free(dp_connection);
  if (dp_session_desc != NULL) free(dp_session_desc);
  if (dp_player_name != NULL) free(dp_player_name);

  return result;
}

BOOL _receive_message(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, session_onmessage callback) {
  dplobby_message* message = NULL;
  HRESULT result = dplobby_receive_message(lobby, app_id, &message);

  if (result != DP_OK) {
    return FALSE;
  }
  if (message == NULL) {
    return FALSE;
  }

  return callback(lobby, app_id, message);
}

HRESULT dplobby_process_messages(struct session_init* desc, session_onmessage callback) {
  struct session_priv* data = get_private(desc);

  printf("App: %ld\n", data->app_id);

  while (WaitForSingleObject(data->message_event, INFINITE) == WAIT_OBJECT_0) {
    if (_receive_message(data->dplobby, data->app_id, callback) == FALSE) {
      break;
    }
  }

  return DP_OK;
}
