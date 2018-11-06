#include <stdio.h>
#include "host_session.h"
#include "debug.h"

static GUID GUID_TEST_APP = {0x5bfdb060, 0x6a4, 0x11d0, {0x9c, 0x4f, 0x0, 0xa0, 0xc9, 0x5, 0x42, 0x5e}};

BOOL onmessage(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobby_message* message) {
  if (message->flags != DPLMSG_SYSTEM) {
    dplobby_message_free(message);
    return TRUE;
  }

  DPLMSG_SYSTEMMESSAGE* system_message = (DPLMSG_SYSTEMMESSAGE*) message->data;
  switch (system_message->dwType) {
    case DPLSYS_APPTERMINATED:
      printf("received APPTERMINATED message\n");
      dplobby_message_free(message);
      return FALSE;
    case DPLSYS_NEWSESSIONHOST:
      printf("received NEWSESSIONHOST message\n");
      break;
    case DPLSYS_CONNECTIONSETTINGSREAD:
      printf("received CONNECTIONSETTINGSREAD message\n");
      break;
    case DPLSYS_DPLAYCONNECTFAILED:
      printf("received CONNECTFAILED message\n");
      break;
    case DPLSYS_DPLAYCONNECTSUCCEEDED:
      printf("received CONNECTSUCCEEDED message!\n");
      break;
    default:
      printf("received unknown message: %ld\n", system_message->dwType);
      break;
    case DPLSYS_GETPROPERTY: {
      DPLMSG_GETPROPERTY* get_prop_message = (DPLMSG_GETPROPERTY*) message->data;
      DPLMSG_GETPROPERTYRESPONSE* get_prop_response = calloc(1, sizeof(DPLMSG_GETPROPERTYRESPONSE));
      get_prop_response->dwType = DPLSYS_GETPROPERTYRESPONSE;
      get_prop_response->dwRequestID = get_prop_message->dwRequestID;
      get_prop_response->guidPlayer = get_prop_message->guidPlayer;
      get_prop_response->guidPropertyTag = get_prop_message->guidPropertyTag;
      get_prop_response->hr = DPERR_UNKNOWNMESSAGE;
      get_prop_response->dwDataSize = 0;
      get_prop_response->dwPropertyData[0] = 0;
      IDirectPlayLobby_SendLobbyMessage(lobby, 0, app_id, get_prop_response, sizeof(get_prop_response));
      break;
    }
  }

  dplobby_message_free(message);
  return TRUE;
}

int main() {
  struct session_init desc = create_host_session();
  desc.player_name = "Test";
  desc.application = GUID_TEST_APP;
  desc.service_provider = DPSPGUID_TCPIP;

  HRESULT result = host_session(&desc);
  if (result == DP_OK) {
    dplobby_process_messages(&desc, onmessage);

    printf("Success!\n");
    return 0;
  }

  printf("Fail: %ld\n", result);

  char* message = get_error_message(result);
  if (message != NULL) {
    printf("%s\n", message);
  }
  free(message);

  return 1;
}
