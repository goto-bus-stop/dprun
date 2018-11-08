#include <stdio.h>
#include <dplobby.h>
#include "session.h"
#include "dpwrap.h"
#include "debug.h"

struct session_priv* session_get_private(session_desc* desc) {
  return (struct session_priv*) desc->_private;
}

static void session_init(session_desc* desc) {
  dpaddress_create(&desc->address);
  session_get_private(desc)->dplobby = NULL;
  session_get_private(desc)->app_id = 0;
  session_get_private(desc)->message_event = CreateEvent(NULL, FALSE, FALSE, NULL);
}

session_desc session_create() {
  session_desc desc = {
    .player_name = NULL,
    .session_id = GUID_NULL,
    .application = GUID_NULL,
    .service_provider = GUID_NULL,
  };

  session_init(&desc);

  return desc;
}

HRESULT session_launch(session_desc* desc) {
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

  result = dpsess_create(&dp_session_desc);
  CHECK("dpsess_create", result);
  dpsess_set_application(dp_session_desc, desc->application);
  dpsess_set_id(dp_session_desc, desc->session_id);

  result = dpconn_create(dp_session_desc, dp_player_name, &dp_connection);
  CHECK("dpconn_create", result);
  dpconn_set_host(dp_connection, TRUE);
  dpconn_set_service_provider(dp_connection, desc->service_provider);

  result = dpaddress_finish(desc->address, &dp_connection->lpAddress, &dp_connection->dwAddressSize);
  CHECK("dpaddress_finish", result);

  result = dplobby_run_application(lobby, &app_id, dp_connection, event);
  CHECK("RunApplication", result);

  desc->session_id = dp_session_desc->guidInstance;

  session_get_private(desc)->dplobby = lobby;
  session_get_private(desc)->app_id = app_id;
  session_get_private(desc)->message_event = event;

  // TODO also clean up when errors happen
  if (dp_connection != NULL) free(dp_connection);
  if (dp_session_desc != NULL) free(dp_session_desc);
  if (dp_player_name != NULL) free(dp_player_name);
  // TODO free dpaddress from session_desc maybe

  return result;
}

BOOL _handle_message(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, session_onmessage callback) {
  dplobbymsg* message = NULL;
  HRESULT result = dplobby_receive_message(lobby, app_id, &message);

  if (result != DP_OK) {
    return FALSE;
  }
  if (message == NULL) {
    return FALSE;
  }

  return callback(lobby, app_id, message);
}

HRESULT session_process_messages(session_desc* desc, session_onmessage callback) {
  struct session_priv* data = session_get_private(desc);

  printf("App: %ld\n", data->app_id);

  while (WaitForSingleObject(data->message_event, INFINITE) == WAIT_OBJECT_0) {
    if (_handle_message(data->dplobby, data->app_id, callback) == FALSE) {
      break;
    }
  }

  return DP_OK;
}
