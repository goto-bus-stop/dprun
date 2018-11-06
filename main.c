#include <stdio.h>
#include "host_session.h"
#include "debug.h"

static GUID GUID_TEST_APP = {0x5bfdb060, 0x6a4, 0x11d0, {0x9c, 0x4f, 0x0, 0xa0, 0xc9, 0x5, 0x42, 0x5e}};

BOOL onmessage(struct dplobby_message* message) {
  printf("%d\n", message->data_size);
  dplobby_message_free(message);
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
