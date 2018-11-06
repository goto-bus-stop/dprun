#pragma once
#include <dplobby.h>

struct host_desc {
  char* player_name;
  GUID application;
  GUID service_provider;
};

HRESULT host_session(struct host_desc desc);
