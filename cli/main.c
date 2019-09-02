#include "../shared.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include "session.h"
#include "../debug.h"
#include "dpsp.h"

static struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"host", optional_argument, NULL, 'H'},
  {"join", required_argument, NULL, 'J'},
  {"player", required_argument, NULL, 'p'},
  {"address", required_argument, NULL, 'a'},
  {"application", required_argument, NULL, 'A'},
  {"session-name", required_argument, NULL, 'n'},
  {"session-password", required_argument, NULL, 'q'},
  {"service-provider", required_argument, NULL, 's'},
  {0, 0, 0, 0},
};

static const char* help_text =
  "dprun <--host|--join> [options]\n"
  "\n"
  "-H, --host [session]\n"
  "    Host a DirectPlay session.\n"
  "    [session] is optional, and can contain a GUID that will be used as the session instance ID.\n"
  "    If omitted, a random GUID is generated.\n"
  "-J, --join [session]\n"
  "    Join a DirectPlay session.\n"
  "    [session] is the GUID for the session.\n"
  "\n"
  "Options:\n"
  "  -p, --player [name]\n"
  "      The name of the local player (required).\n"
  "  -s, --service-provider [guid]\n"
  "      The GUID of the service provider to use (required).\n"
  "      This field also supports constant values: TCPIP, IPX, SERIAL, MODEM, DPRUN\n"
  "  -A, --application [guid]\n"
  "      The GUID of the application to start (required).\n"
  "\n"
  "  -a, --address [key]=[value]\n"
  "      Add an address part. This flag can appear more than once.\n"
  "      The [value] is the string value of the address part.\n"
  "      To specify a numeric value, use \"i:12345\".\n"
  "      To specify a binary value, use \"b:[hex encoded value]\", for example \"b:DEADBEEF\".\n"
  "      The [key] field is the GUID for the address data type. It also supports constant values:\n"
  "          TotalSize, ServiceProvider, LobbyProvider, Phone, PhoneW,\n"
  "          Modem, ModemW, INet, INetW, INetPort, ComPort, SelfID\n"
  "  -n, --session-name [name]\n"
  "      The name of the session to host or join (optional).\n"
  "  -q, --session-password [password]\n"
  "      The password for the session to host or join (optional).\n"
  "\n"
  "GUIDs passed to dprun must be formatted like below, including braces and dashes:\n"
  "    {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n"
  "    {685BC400-9D2C-11cf-A9CD-00AA006886E3}\n";

static void print_address(dpaddress* addr) {
  printf("[print_address] address:\n");
  for (int i = 0; i < addr->num_elements; i++) {
    DPCOMPOUNDADDRESSELEMENT el = addr->elements[i];
    char guid[GUID_STR_LEN];
    char data[100];
    guid_stringify(&el.guidDataType, guid);
    if (el.dwDataSize < 100) {
      memcpy(data, el.lpData, el.dwDataSize);
      data[el.dwDataSize] = '\0';
    } else {
      memcpy(data, el.lpData, 99);
      data[99] = '\0';
    }
    printf("                  %s - %s\n", guid, data);
  }
}

static HRESULT parse_address_chunk(char* input, DPCOMPOUNDADDRESSELEMENT** out_address) {
  HRESULT result = DP_OK;
  char* eq = strchr(input, '=');
  char guid_str[60];
  if (eq - input > 55) return DPERR_OUTOFMEMORY;
  memcpy(guid_str, input, eq - input);
  guid_str[eq - input] = '\0';

  GUID data_type;
  void* data;
  DWORD data_size;

  if (strcmp(guid_str, "TotalSize") == 0) data_type = DPAID_TotalSize;
  else if (strcmp(guid_str, "ServiceProvider") == 0) data_type = DPAID_ServiceProvider;
  else if (strcmp(guid_str, "LobbyProvider") == 0) data_type = DPAID_LobbyProvider;
  else if (strcmp(guid_str, "Phone") == 0) data_type = DPAID_Phone;
  else if (strcmp(guid_str, "PhoneW") == 0) data_type = DPAID_PhoneW;
  else if (strcmp(guid_str, "Modem") == 0) data_type = DPAID_Modem;
  else if (strcmp(guid_str, "ModemW") == 0) data_type = DPAID_ModemW;
  else if (strcmp(guid_str, "INet") == 0) data_type = DPAID_INet;
  else if (strcmp(guid_str, "INetW") == 0) data_type = DPAID_INetW;
  else if (strcmp(guid_str, "INetPort") == 0) data_type = DPAID_INetPort;
  else if (strcmp(guid_str, "ComPort") == 0) data_type = DPAID_ComPort;
  else if (strcmp(guid_str, "SelfID") == 0) data_type = DPAID_SelfID;
  else result = guid_parse(guid_str, &data_type);

  // --address {685BC400-9D2C-11cf-A9CD-00AA006886E3}=i:8000
  // --address {685BC400-9D2C-11cf-A9CD-00AA006886E3}=b:DEADBEEF
  // --address {685BC400-9D2C-11cf-A9CD-00AA006886E3}=127.0.0.1

  if (eq[1] == 'i' && eq[2] == ':') {
    // integer
    data_size = sizeof(DWORD);
    data = calloc(1, sizeof(DWORD));
    DWORD num = atoi(&eq[3]);
    memcpy(data, &num, sizeof(DWORD));
  } else if (eq[1] == 'b' && eq[2] == ':') {
    // binary data
    char* hex_str = &eq[3];
    int str_size = strlen(hex_str);
    data_size = str_size / 2;
    data = calloc(1, data_size);
    for (int i = 0; i < str_size; i++) {
      char current_pair[3] = {
        hex_str[i * 2],
        hex_str[i * 2 + 1],
        '\0',
      };
      ((char*)data)[i] = strtol(current_pair, NULL, 16);
    }
  } else {
    data_size = strlen(eq);
    data = calloc(1, data_size);
    memcpy(data, &eq[1], data_size - 1);
  }

  if (SUCCEEDED(result)) {
    result = dpaddrelement_create(data_type, data, data_size, out_address);
  }

  return result;
}

static HRESULT parse_cli_args(int argc, char** argv, session_desc* desc) {
  int opt_index = 0;
  DPCOMPOUNDADDRESSELEMENT* addr_element = NULL;
  while (TRUE) {
    if (addr_element != NULL) {
      free(addr_element);
      addr_element = NULL;
    }
    switch (getopt_long(argc, argv, "HJ:p:A:n:q:s:", long_options, &opt_index)) {
      case -1:
        // Done.
        return DP_OK;
      case 'J': case 'H':
        printf("--join and --host may only appear as the first argument\n");
        return 1;
      case 'p':
        if (optarg == NULL) return 1;
        desc->player_name = optarg;
        break;
      case 'A':
        if (optarg == NULL) return 1;
        guid_parse(optarg, &desc->application);
        break;
      case 's':
        if (optarg == NULL) return 1;
        if (strcmp(optarg, "IPX") == 0) desc->service_provider = DPSPGUID_IPX;
        else if (strcmp(optarg, "TCPIP") == 0) desc->service_provider = DPSPGUID_TCPIP;
        else if (strcmp(optarg, "SERIAL") == 0) desc->service_provider = DPSPGUID_SERIAL;
        else if (strcmp(optarg, "MODEM") == 0) desc->service_provider = DPSPGUID_MODEM;
        else if (strcmp(optarg, "DPRUN") == 0) desc->service_provider = DPSPGUID_DPRUN;
        else guid_parse(optarg, &desc->service_provider);
        dpaddress_create_element(desc->address, DPAID_ServiceProvider, &desc->service_provider, sizeof(GUID));
        break;
      case 'a': {
        if (optarg == NULL) return 1;
        HRESULT result = parse_address_chunk(optarg, &addr_element);
        if (FAILED(result)) {
          printf("Could not parse address chunk '%s': %s\n", optarg, get_error_message(result));
          return result;
        }
        dpaddress_add(desc->address, addr_element);
        break;
      }
      default:
        printf("Unknown argument '%s'\n", long_options[opt_index].name);
        return 1;
    }
  }
}

static BOOL onmessage(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobbymsg* message) {
  printf("[onmessage] Receiving message... %ld\n", message->flags);
  for (int i = 0; i < message->data_size; i++) {
    printf("%02x", ((unsigned char*) message->data)[i]);
  }
  printf("\n");
  if (message->flags == DPLMSG_STANDARD) {
    dplobbymsg_free(message);
    return TRUE;
  }

  DPLMSG_SYSTEMMESSAGE* system_message = (DPLMSG_SYSTEMMESSAGE*) message->data;
  switch (system_message->dwType) {
    case DPLSYS_APPTERMINATED:
      printf("[onmessage] received APPTERMINATED message\n");
      dplobbymsg_free(message);
      return FALSE;
    case DPLSYS_NEWSESSIONHOST:
      printf("[onmessage] received NEWSESSIONHOST message\n");
      break;
    case DPLSYS_CONNECTIONSETTINGSREAD:
      printf("[onmessage] received CONNECTIONSETTINGSREAD message\n");
      break;
    case DPLSYS_DPLAYCONNECTFAILED:
      printf("[onmessage] received CONNECTFAILED message\n");
      break;
    case DPLSYS_DPLAYCONNECTSUCCEEDED:
      printf("[onmessage] received CONNECTSUCCEEDED message!\n");
      break;
    default:
      printf("[onmessage] received unknown message: %ld\n", system_message->dwType);
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

  dplobbymsg_free(message);
  return TRUE;
}

int main(int argc, char** argv) {
  session_desc desc = session_create();
  int opt_index = 0;
  switch (getopt_long(argc, argv, "hHJ:p:A:n:q:s:", long_options, &opt_index)) {
    case 'J': {
      GUID guid = GUID_NULL;
      if (strlen(optarg) != 38 || guid_parse(optarg, &guid) != S_OK) {
        printf("--join got invalid GUID. required format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n");
        return 1;
      }
      desc.is_host = FALSE;
      desc.session_id = guid;
      break;
    }
    case 'H':
      desc.is_host = TRUE;
      if (optind < argc && argv[optind] != NULL && argv[optind][0] != '\0' && argv[optind][0] != '-') {
        printf("--host guid: %s\n", argv[optind]);
        if (guid_parse(argv[optind], &desc.session_id) != S_OK) {
          printf("--host got invalid GUID. required format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n");
          return 1;
        }
        optind++;
      } else {
        CoCreateGuid(&desc.session_id);
      }
      break;
    case 'h':
      printf(help_text);
      return 0;
    default:
      printf("must provide --join or --host as the first argument\n");
      return 1;
  }

  HRESULT result = parse_cli_args(argc, argv, &desc);
  if (FAILED(result)) {
    return 1;
  }

  if (desc.player_name == NULL) {
    printf("Missing --player\n");
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

  char use_dprun_sp = IsEqualGUID(&desc.service_provider, &DPSPGUID_DPRUN);

  if (use_dprun_sp) {
    result = dpsp_register();
    if (FAILED(result)) {
      printf("Could not register DPRun service provider: %s\n", get_error_message(result));
      return 1;
    }
  }

  print_address(desc.address);

  result = session_launch(&desc);
  if (FAILED(result)) {
    printf("Fail: %ld\n", result);
    char* message = get_error_message(result);
    if (message != NULL) {
      printf("%s\n", message);
    }
    free(message);

    if (use_dprun_sp) {
      result = dpsp_unregister();
      if (FAILED(result)) {
        printf("Could not unregister DPRun service provider: %s\n", get_error_message(result));
      }
    }

    return 1;
  }

  char session_id[GUID_STR_LEN];
  guid_stringify(&desc.session_id, session_id);

  printf("[main] launched session %s\n", session_id);
  FILE* dbg_sessid = fopen("dbg_sessid.txt", "w");
  fwrite((void*)session_id, 38, 1, dbg_sessid);
  fclose(dbg_sessid);

  session_process_messages(&desc, onmessage);

  printf("[main] Success!\n");

  if (use_dprun_sp) {
    result = dpsp_unregister();
    if (FAILED(result)) {
      printf("Could not unregister DPRun service provider: %s\n", get_error_message(result));
    }
  }

  return 0;
}
