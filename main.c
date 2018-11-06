#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include "host_session.h"
#include "debug.h"

/* static GUID GUID_TEST_APP = {0x5bfdb060, 0x6a4, 0x11d0, {0x9c, 0x4f, 0x0, 0xa0, 0xc9, 0x5, 0x42, 0x5e}}; */

static struct option long_options[] = {
  {"host", no_argument, NULL, 'h'},
  {"join", required_argument, NULL, 'j'},
  {"player", required_argument, NULL, 'p'},
  {"application", required_argument, NULL, 'A'},
  {"session-name", required_argument, NULL, 'n'},
  {"session-password", required_argument, NULL, 'q'},
  {"service-provider", required_argument, NULL, 's'},
  {0, 0, 0, 0},
};

HRESULT parse_guid(char* input, GUID* out_guid) {
  wchar_t str[39];
  MultiByteToWideChar(CP_ACP, 0, input, -1, str, 39);
  str[38] = L'\0';
  return IIDFromString(str, out_guid);
}

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

HRESULT parse_cli_args(int argc, char** argv, struct session_init* desc) {
  int opt_index = 0;
  while (TRUE) {
    switch (getopt_long(argc, argv, "hj:p:A:n:q:s:", long_options, &opt_index)) {
      case -1:
        // Done.
        return DP_OK;
      case 'j': case 'h':
        printf("--join and --host may only appear as the first argument\n");
        return 1;
      case 'p':
        desc->player_name = optarg;
        break;
      case 'A':
        parse_guid(optarg, &desc->application);
        break;
      case 's':
        parse_guid(optarg, &desc->service_provider);
        break;
      default:
        printf("Unknown argument '%s'\n", long_options[opt_index].name);
        return 1;
    }
  }
}

int main(int argc, char** argv) {
  struct session_init desc = {0};
  int opt_index = 0;
  switch (getopt_long(argc, argv, "hj:p:A:n:q:s:", long_options, &opt_index)) {
    case 'j': {
      GUID guid = GUID_NULL;
      if (strlen(optarg) != 38 || parse_guid(optarg, &guid) != S_OK) {
        printf("--join got invalid GUID. required format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n");
        return 1;
      }
      desc = create_join_session(guid);
      break;
    }
    case 'h':
      desc = create_host_session();
      break;
    default:
      printf("must provide --join or --host as the first argument\n");
      return 1;
  }

  if (parse_cli_args(argc, argv, &desc) != DP_OK) {
    printf("Could not parse CLI args\n");
    return 1;
  }

  if (desc.player_name == NULL) {
    printf("Missing --player-name\n");
    return 1;
  }
  if (IsEqualGUID(&desc.application, &GUID_NULL)) {
    printf("Missing --application\n");
    return 1;
  }
  if (IsEqualGUID(&desc.service_provider, &GUID_NULL)) {
    printf("Missing --service-provider\n");
    return 1;
  }

  HRESULT result = launch_session(&desc);
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
