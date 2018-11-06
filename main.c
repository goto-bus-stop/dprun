#include <stdio.h>
#include "host_session.h"
#include "debug.h"

static GUID GUID_TEST_APP = {0x5bfdb060, 0x6a4, 0x11d0, {0x9c, 0x4f, 0x0, 0xa0, 0xc9, 0x5, 0x42, 0x5e}};

int main() {
  struct host_desc desc = {
    .player_name = "Test",
    .application = GUID_TEST_APP,
    .service_provider = DPSPGUID_TCPIP,
  };

  HRESULT result = host_session(desc);
  if (result == DP_OK) {
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
