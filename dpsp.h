#pragma once
#include <dplaysp.h>

static const GUID DPSPGUID_DPRUN = { 0xb1ed2367, 0x609b, 0x4c5c, { 0x87, 0x55, 0xd2, 0xa2, 0x9b, 0xb9, 0xa5, 0x54 } };

HRESULT dpsp_register();
HRESULT dpsp_unregister();
HRESULT dpsp_init(SPINITDATA* init_data);
