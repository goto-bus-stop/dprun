#include "../shared.h"
#include <stdio.h>
#include <dplobby.h>
#include <getopt.h>
#include "dpwrap.h"
#include "../debug.h"

enum outputfmt {
    OUTPUT_FMT_DEFAULT,
    OUTPUT_FMT_CSV
};

static struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"csv", no_argument, NULL, 'c'},
  {0, 0, 0, 0},
};

static const char* help_text =
  "dpenumerate [options]\n"
  "\n"
  "Options:\n"
  "  -c, --csv\n"
  "      CSV output.\n";


BOOL FAR PASCAL application_print_callback(LPCDPLAPPINFO lpAppInfo,  LPVOID lpContext,  DWORD dwFlags) {
  enum outputfmt format = * (enum outputfmt*) lpContext;
  char guid[GUID_STR_LEN];
  guid_stringify(&lpAppInfo->guidApplication, guid);

  switch (format) {
    case OUTPUT_FMT_CSV:
      printf("%s,%s\n", lpAppInfo->lpszAppNameA, guid);
      break;

    default:
      printf("  - %s: %s\n", lpAppInfo->lpszAppNameA, guid);
      break;
  }

  return TRUE;
}

int main(int argc, char** argv) {
  int opt_index = 0;
  enum outputfmt format = OUTPUT_FMT_DEFAULT;

  switch (getopt_long(argc, argv, "hc", long_options, &opt_index)) {
    case 'c':
      format = OUTPUT_FMT_CSV;
      break;

    case 'h':
      printf(help_text);
      return 0;

    default:
      break;
  }

  LPDIRECTPLAYLOBBY3A lobby = NULL;

  HRESULT result = dplobby_create(&lobby);
  CHECK("dplobby_create", result);

  switch (format) {
    case OUTPUT_FMT_CSV:
      printf("ApplicationName,GUID\n");
      break;

    default:
      printf("Applications registered with DirectPlay:\n");
      break;
  }

  result = lobby->lpVtbl->EnumLocalApplications(lobby, application_print_callback, (LPVOID) &format, 0);

  if (FAILED(result)) {
    printf("Could not enumerate local applications: %s\n", get_error_message(result));
    return 1;
  }

  return 0;
}
