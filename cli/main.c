#include "../shared.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#define CJSON_HIDE_SYMBOLS
#include <cjson/cJSON.h>
#include "session.h"
#include "../debug.h"
#include "dpsp.h"

static BOOL json_rpc = FALSE;

#define log(...) fprintf(stderr, __VA_ARGS__)

static char tmp_err_buffer[2048] = {0};

/**
 * Emit a notification.
 *
 * Takes ownership of the JSON object in `params`.
 */
void jsonrpc_emit(char* method, cJSON* params) {
  if (!json_rpc) {
    if (params != NULL) cJSON_Delete(params);
    return;
  }
  if (params == NULL) {
    params = cJSON_CreateNull();
  }
  cJSON* message = cJSON_CreateObject();
  cJSON_AddStringToObject(message, "jsonrpc", "2.0");
  cJSON_AddStringToObject(message, "method", method);
  cJSON_AddItemToObject(message, "params", params);
  char* string = cJSON_PrintUnformatted(message);
  printf("%s\n", string);
  cJSON_Delete(message);
}

/**
 * Call a JSON-RPC method.
 *
 * Takes ownership of `params`.
 */
void jsonrpc_call(char* method, cJSON* params, DWORD id) {
  if (!json_rpc) {
    if (params != NULL) cJSON_Delete(params);
    return;
  }
  if (params == NULL) {
    params = cJSON_CreateNull();
  }
  cJSON* message = cJSON_CreateObject();
  cJSON_AddStringToObject(message, "jsonrpc", "2.0");
  cJSON_AddStringToObject(message, "method", method);
  cJSON_AddItemToObject(message, "params", params);
  cJSON_AddNumberToObject(message, "id", id);
  char* string = cJSON_PrintUnformatted(message);
  printf("%s\n", string);
  cJSON_Delete(message);
}

void jsonrpc_handle_notification() {
}

void jsonrpc_handle_call() {
}

void jsonrpc_handle_result() {
}

void jsonrpc_process(char* message) {
  cJSON* json = cJSON_Parse(message);
  if (json == NULL) {
    return;
  }
  cJSON* version = cJSON_GetObjectItemCaseSensitive(json, "jsonrpc");
  cJSON* id = cJSON_GetObjectItemCaseSensitive(json, "id");
  cJSON* params = cJSON_GetObjectItemCaseSensitive(json, "params");

  if (params != NULL) {
    cJSON* method = cJSON_GetObjectItemCaseSensitive(json, "method");
    if (id == NULL) {
      // Notification
    } else {
      // Call
    }
  } else if (id != NULL && params == NULL) {
    // Response
    cJSON* result = cJSON_GetObjectItemCaseSensitive(json, "result");
    cJSON* error = cJSON_GetObjectItemCaseSensitive(json, "error");
  }

  cJSON_Delete(json);
}

static char stdin_buffer[2048];
void jsonrpc_poll() {
  log("jsonrpc_poll\n");
  DWORD bytes_read = 0;
  ReadFile(GetStdHandle(STD_INPUT_HANDLE), stdin_buffer, 2048, &bytes_read, NULL);
  /* char* message = read_line(); */
  /* log("[jsonrpc_poll] received: %ld %s", bytes_read, stdin_buffer); */
}

void error(HRESULT hr, const char* format, ...) {
  va_list va;
  va_start(va, format);

  if (json_rpc) {
    vsprintf(tmp_err_buffer, format, va);
    cJSON* json = cJSON_CreateObject();
    if (hr != DP_OK)
      cJSON_AddNumberToObject(json, "code", hr);
    cJSON_AddStringToObject(json, "message", tmp_err_buffer);
    jsonrpc_emit("error", json);
  } else {
    fprintf(stderr, format, va);
  }

  va_end(va);
}

static struct option long_options[] = {
  {"rpc", no_argument, NULL, 'r'},
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
  "  -r, --rpc\n"
  "      Use stdin/stdout for JSON-RPC.\n"
  "      Events are sent on stdout, control messages are taken from stdin.\n"
  "\n"
  "GUIDs passed to dprun must be formatted like below, including braces and dashes:\n"
  "    {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n"
  "    {685BC400-9D2C-11cf-A9CD-00AA006886E3}\n";

static void print_address(dpaddress* addr) {
  log("[print_address] address:\n");
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
    log("                  %s - %s\n", guid, data);
  }
}

static HRESULT parse_address_chunk(char* input, DPCOMPOUNDADDRESSELEMENT** out_address) {
  HRESULT result = DP_OK;
  char* eq = strchr(input, '=');
  char guid_str[60];
  if (eq - input > 55)
    return DPERR_OUTOFMEMORY;
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

static HRESULT parse_service_provider_guid(char* str, GUID* sp) {
  if (strcmp(str, "IPX") == 0) *sp = DPSPGUID_IPX;
  else if (strcmp(str, "TCPIP") == 0) *sp = DPSPGUID_TCPIP;
  else if (strcmp(str, "SERIAL") == 0) *sp = DPSPGUID_SERIAL;
  else if (strcmp(str, "MODEM") == 0) *sp = DPSPGUID_MODEM;
  else if (strcmp(str, "DPRUN") == 0) *sp = DPSPGUID_DPRUN;
  else {
    return guid_parse(str, sp);
  }
  return DP_OK;
}

static HRESULT parse_cli_args(int argc, char** argv, session_desc* desc) {
  int opt_index = 0;
  DPCOMPOUNDADDRESSELEMENT* addr_element = NULL;
  while (TRUE) {
    if (addr_element != NULL) {
      free(addr_element);
      addr_element = NULL;
    }
    switch (getopt_long(argc, argv, "HJ:p:A:n:q:s:r", long_options, &opt_index)) {
      case -1:
        // Done.
        return DP_OK;
      case 'r':
        json_rpc = TRUE;
        break;
      case 'J': case 'H':
        log("--join and --host may only appear as the first argument\n");
        return 1;
      case 'p':
        if (optarg == NULL) return 1;
        desc->player_name = optarg;
        break;
      case 'A':
        if (optarg == NULL) return 1;
        guid_parse(optarg, &desc->application);
        break;
      case 's': {
        if (optarg == NULL) return 1;
        HRESULT result = parse_service_provider_guid(optarg, &desc->service_provider);
        if (FAILED(result)) {
          log("Could not parse service provider '%s': %s\n", optarg, get_error_message(result));
          return result;
        }
        dpaddress_create_element(desc->address, DPAID_ServiceProvider, &desc->service_provider, sizeof(GUID));
        break;
      }
      case 'a': {
        if (optarg == NULL) return 1;
        HRESULT result = parse_address_chunk(optarg, &addr_element);
        if (FAILED(result)) {
          log("Could not parse address chunk '%s': %s\n", optarg, get_error_message(result));
          return result;
        }
        dpaddress_add(desc->address, addr_element);
        break;
      }
      default:
        log("Unknown argument '%s'\n", long_options[opt_index].name);
        return 1;
    }
  }
}

static BOOL onmessage(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobbymsg* message) {
  log("[onmessage] Receiving message... %ld\n", message->flags);
  for (int i = 0; i < message->data_size; i++) {
    log("%02x", ((unsigned char*) message->data)[i]);
  }
  log("\n");
  if (message->flags == DPLMSG_STANDARD) {
    dplobbymsg_free(message);
    return TRUE;
  }

  DPLMSG_SYSTEMMESSAGE* system_message = (DPLMSG_SYSTEMMESSAGE*) message->data;
  switch (system_message->dwType) {
    case DPLSYS_APPTERMINATED:
      log("[onmessage] received APPTERMINATED message\n");
      jsonrpc_emit("appTerminated", NULL);
      dplobbymsg_free(message);
      return FALSE;
    case DPLSYS_NEWSESSIONHOST:
      log("[onmessage] received NEWSESSIONHOST message\n");
      jsonrpc_emit("newSessionHost", NULL);
      break;
    case DPLSYS_CONNECTIONSETTINGSREAD:
      log("[onmessage] received CONNECTIONSETTINGSREAD message\n");
      jsonrpc_emit("connectionSettingsRead", NULL);
      break;
    case DPLSYS_DPLAYCONNECTFAILED:
      log("[onmessage] received CONNECTFAILED message\n");
      jsonrpc_emit("connectFailed", NULL);
      break;
    case DPLSYS_DPLAYCONNECTSUCCEEDED:
      log("[onmessage] received CONNECTSUCCEEDED message!\n");
      jsonrpc_emit("connectSucceeded", NULL);
      break;
    default:
      log("[onmessage] received unknown message: %ld\n", system_message->dwType);
      break;
    case DPLSYS_GETPROPERTY: {
      DPLMSG_GETPROPERTY* get_prop_message = (DPLMSG_GETPROPERTY*) message->data;
      char player[GUID_STR_LEN];
      char property[GUID_STR_LEN];
      guid_stringify(&get_prop_message->guidPlayer, player);
      guid_stringify(&get_prop_message->guidPropertyTag, property);
      cJSON* json = cJSON_CreateObject();
      cJSON_AddStringToObject(json, "player", player);
      cJSON_AddStringToObject(json, "property", property);
      jsonrpc_call("getProperty", json, get_prop_message->dwRequestID);

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
  switch (getopt_long(argc, argv, "hHJ:p:A:n:q:s:r", long_options, &opt_index)) {
    case 'J': {
      GUID guid = GUID_NULL;
      if (strlen(optarg) != 38 || guid_parse(optarg, &guid) != S_OK) {
        error(DPERR_INVALIDPARAM, "--join got invalid GUID. required format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n");
        return 1;
      }
      desc.is_host = FALSE;
      desc.session_id = guid;
      break;
    }
    case 'H':
      desc.is_host = TRUE;
      if (optind < argc && argv[optind] != NULL && argv[optind][0] != '\0' && argv[optind][0] != '-') {
        log("--host guid: %s\n", argv[optind]);
        if (guid_parse(argv[optind], &desc.session_id) != S_OK) {
          error(DPERR_INVALIDPARAM, "--host got invalid GUID. required format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n");
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
      error(DPERR_INVALIDPARAM, "must provide --join or --host as the first argument\n");
      return 1;
  }

  HRESULT result = parse_cli_args(argc, argv, &desc);
  if (FAILED(result)) {
    return 1;
  }

  if (desc.player_name == NULL) {
    error(DPERR_INVALIDPARAM, "Missing --player\n");
    return 1;
  }
  if (IsEqualGUID(&desc.application, &GUID_NULL)) {
    error(DPERR_INVALIDPARAM, "Missing --application\n");
    return 1;
  }
  if (IsEqualGUID(&desc.service_provider, &GUID_NULL)) {
    error(DPERR_INVALIDPARAM, "Missing --service-provider\n");
    return 1;
  }

  char use_dprun_sp = IsEqualGUID(&desc.service_provider, &DPSPGUID_DPRUN);

  if (use_dprun_sp) {
    result = dpsp_register();
    if (FAILED(result)) {
      error(result, "Could not register DPRun service provider: %s\n", get_error_message(result));
      return 1;
    }
  }

  print_address(desc.address);

  result = session_launch(&desc);
  if (FAILED(result)) {
    char* message = get_error_message(result);
    if (message != NULL) {
      error(result, "%s\n", message);
    } else {
      error(result, "Fail: %ld\n", result);
    }
    free(message);

    if (use_dprun_sp) {
      result = dpsp_unregister();
      if (FAILED(result)) {
        log("Could not unregister DPRun service provider: %s\n", get_error_message(result));
      }
    }

    return 1;
  }

  char session_id[GUID_STR_LEN];
  guid_stringify(&desc.session_id, session_id);

  log("[main] launched session %s\n", session_id);
  FILE* dbg_sessid = fopen("dbg_sessid.txt", "w");
  fwrite((void*)session_id, 38, 1, dbg_sessid);
  fclose(dbg_sessid);

  cJSON* app_launched_json = cJSON_CreateObject();
  cJSON_AddStringToObject(app_launched_json, "guid", session_id);
  cJSON_AddNumberToObject(app_launched_json, "pid", session_get_pid(&desc));
  jsonrpc_emit("appLaunched", app_launched_json);

  HANDLE events[] = {
    session_get_event(&desc),
    GetStdHandle(STD_INPUT_HANDLE)
  };
  const DWORD num_events = sizeof(events) / sizeof(events[0]);
  BOOL keep_going = TRUE;
  while (keep_going) {
    DWORD wait = WaitForMultipleObjects(num_events, events, FALSE, 100);
    switch (wait) {
      case WAIT_OBJECT_0:
        if (session_process_message(&desc, onmessage) == FALSE) {
          keep_going = FALSE;
        }
        break;
      case WAIT_OBJECT_0 + 1:
        jsonrpc_poll();
        break;
      case WAIT_FAILED:
      case WAIT_ABANDONED:
        keep_going = FALSE;
        break;
    }
  }

  log("[main] Success!\n");

  if (use_dprun_sp) {
    result = dpsp_unregister();
    if (FAILED(result)) {
      log("Could not unregister DPRun service provider: %s\n", get_error_message(result));
    }
  }

  return 0;
}
