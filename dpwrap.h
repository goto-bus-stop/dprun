#pragma once

/**
 * DirectPlay wrappers
 */

typedef struct dpaddress {
  // Array of elements in this address.
  DPCOMPOUNDADDRESSELEMENT* elements;
  // Amount of elements in this address.
  DWORD num_elements;
} dpaddress;

/**
 * Create an address element.
 *
 * @param data_type The address element data type.
 * @param data The address element value.
 * @param data_size Size of the address element value.
 * @param out_element Pointer to a DPCOMPOUNDADDRESSELEMENT pointer, which will be filled with the created address element. The user is responsible for freeing this memory.
 */
HRESULT dpaddrelement_create(GUID data_type, void* data, DWORD data_size, DPCOMPOUNDADDRESSELEMENT** out_element);

/**
 * Create an empty DirectPlay address.
 *
 * @param out_address Pointer to a dpaddress pointer, which will be filled with the created address. The user is responsible for freeing this memory.
 */
HRESULT dpaddress_create(dpaddress** out_address);

/**
 * Add an element to a DirectPlay address.
 *
 * @param address The dpaddress.
 * @param element The element to add. This element is copied into the address; the element memory can be freed if you are no longer using it.
 */
HRESULT dpaddress_add(dpaddress* address, DPCOMPOUNDADDRESSELEMENT* element);

/**
 * Create and add an element to a DirectPlay address.
 * Convenience wrapper around dpaddrelement_create and dpaddress_add that saves some memory juggling.
 *
 * @param address The dpaddress.
 * @param data_type The address element data type.
 * @param data The address element value.
 * @param data_size Size of the address element value.
 */
HRESULT dpaddress_create_element(dpaddress* address, GUID data_type, void* data, DWORD data_size);

/**
 * Finish the DirectPlay address and return values that can be assigned to the DirectPlay API directly.
 *
 * @param address The dpaddress.
 * @param lobby
 * @param out_elements Pointer to a memory pointer that will be filled with the address data.
 * @param out_size Pointer to a DWORD that will be filled with the data size.
 */
HRESULT dpaddress_finish(dpaddress* address, LPDIRECTPLAYLOBBY3A lobby, void** out_elements, DWORD* out_size);

typedef struct dplobbymsg {
  DWORD flags;
  void* data;
  DWORD data_size;
} dplobbymsg;

void dplobbymsg_free(dplobbymsg* message);

HRESULT dplobby_create(LPDIRECTPLAYLOBBY3A* out_lobby);
HRESULT dplobby_run_application(LPDIRECTPLAYLOBBY3A lobby, DWORD* app_id, LPDPLCONNECTION connection, HANDLE event);
HRESULT dplobby_receive_message(LPDIRECTPLAYLOBBY3A lobby, DWORD app_id, dplobbymsg** out_message);

HRESULT dpname_create(char* name, LPDPNAME* out_name);

HRESULT dpsess_create(LPDPSESSIONDESC2* out_session_desc);
HRESULT dpsess_create_host(GUID application, LPDPSESSIONDESC2* out_session_desc);
void dpsess_generate_id(LPDPSESSIONDESC2 session_desc);
void dpsess_set_id(LPDPSESSIONDESC2 session_desc, GUID session_id);
void dpsess_set_application(LPDPSESSIONDESC2 session_desc, GUID application);

HRESULT dpconn_create(LPDPSESSIONDESC2 session_desc, LPDPNAME player_name, LPDPLCONNECTION* out_connection);
void dpconn_set_host(LPDPLCONNECTION connection, char is_host);
void dpconn_set_service_provider(LPDPLCONNECTION connection, GUID service_provider);
