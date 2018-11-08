#include "dpsp.h"

BOOL WINAPI DllMain (HINSTANCE dll, DWORD reason, void* reserved) {
  return TRUE;
}

__declspec(dllexport) HRESULT SPInit(SPINITDATA* data) {
  return dpsp_init(data);
}
