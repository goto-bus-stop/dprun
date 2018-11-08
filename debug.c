#include "shared.h"
#include "debug.h"
#include <stdio.h>
#include <dplay.h>

#ifdef DEBUG

static char* get_dp_message (HRESULT hr) {
  switch (hr) {
    case CLASS_E_NOAGGREGATION: return "A non-NULL value was passed for the pUnkOuter parameter in DirectPlayCreate, DirectPlayLobbyCreate, or IDirectPlayLobby2::Connect.";
    case DPERR_ACCESSDENIED: return "The session is full or an incorrect password was supplied.";
    case DPERR_ACTIVEPLAYERS: return "The requested operation cannot be performed because there are existing active players.";
    case DPERR_ALREADYINITIALIZED: return "This object is already initialized.";
    case DPERR_APPNOTSTARTED: return "The application has not been started yet.";
    case DPERR_AUTHENTICATIONFAILED: return "The password or credentials supplied could not be authenticated.";
    case DPERR_BUFFERTOOLARGE: return "The data buffer is too large to store.";
    case DPERR_BUSY: return "A message cannot be sent because the transmission medium is busy.";
    case DPERR_BUFFERTOOSMALL: return "The supplied buffer is not large enough to contain the requested data.";
    case DPERR_CANTADDPLAYER: return "The player cannot be added to the session.";
    case DPERR_CANTCREATEGROUP: return "A new group cannot be created.";
    case DPERR_CANTCREATEPLAYER: return "A new player cannot be created.";
    case DPERR_CANTCREATEPROCESS: return "Cannot start the application.";
    case DPERR_CANTCREATESESSION: return "A new session cannot be created.";
    case DPERR_CANTLOADCAPI: return "No credentials were supplied and the CryptoAPI package (CAPI) to use for cryptography services cannot be loaded.";
    case DPERR_CANTLOADSECURITYPACKAGE: return "The software security package cannot be loaded.";
    case DPERR_CANTLOADSSPI: return "No credentials were supplied and the software security package (SSPI) that will prompt for credentials cannot be loaded.";
    case DPERR_CAPSNOTAVAILABLEYET: return "The capabilities of the DirectPlay object have not been determined yet. This error will occur if the DirectPlay object is implemented on a connectivity solution that requires polling to determine available bandwidth and latency.";
    case DPERR_CONNECTING: return "The method is in the process of connecting to the network. The application should keep calling the method until it returns DP_OK, indicating successful completion, or it returns a different error.";
    case DPERR_ENCRYPTIONFAILED: return "The requested information could not be digitally encrypted. Encryption is used for message privacy. This error is only relevant in a secure session.";
    case DPERR_EXCEPTION: return "An exception occurred when processing the request.";
    case DPERR_GENERIC: return "An undefined error condition occurred.";
    case DPERR_INVALIDFLAGS: return "The flags passed to this method are invalid.";
    case DPERR_INVALIDGROUP: return "The group ID is not recognized as a valid group ID for this game session.";
    case DPERR_INVALIDINTERFACE: return "The interface parameter is invalid.";
    case DPERR_INVALIDOBJECT: return "The DirectPlay object pointer is invalid.";
    case DPERR_INVALIDPARAMS: return "One or more of the parameters passed to the method are invalid.";
    case DPERR_INVALIDPASSWORD: return "An invalid password was supplied when attempting to join a session that requires a password.";
    case DPERR_INVALIDPLAYER: return "The player ID is not recognized as a valid player ID for this game session.";
    case DPERR_LOGONDENIED: return "The session could not be opened because credentials are required and either no credentials were supplied or the credentials were invalid.";
    case DPERR_NOCAPS: return "The communication link that DirectPlay is attempting to use is not capable of this function.";
    case DPERR_NOCONNECTION: return "No communication link was established.";
    case DPERR_NOINTERFACE: return "The interface is not supported.";
    case DPERR_NOMESSAGES: return "There are no messages in the receive queue.";
    case DPERR_NONAMESERVERFOUND: return "No name server (host) could be found or created. A host must exist to create a player.";
    case DPERR_NONEWPLAYERS: return "The session is not accepting any new players.";
    case DPERR_NOPLAYERS: return "There are no active players in the session.";
    case DPERR_NOSESSIONS: return "There are no existing sessions for this game.";
    case DPERR_NOTLOBBIED: return "Returned by the IDirectPlayLobby2::Connect method if the application was not started by using the IDirectPlayLobby2::RunApplication method or if there is no DPLCONNECTION structure currently initialized for this DirectPlayLobby object.";
    case DPERR_NOTLOGGEDIN: return "An action cannot be performed because a player or client application is not logged in. Returned by the IDirectPlay3::Send method when the client application tries to send a secure message without being logged in.";
    case DPERR_OUTOFMEMORY: return "There is insufficient memory to perform the requested operation.";
    case DPERR_PLAYERLOST: return "A player has lost the connection to the session.";
    case DPERR_SENDTOOBIG: return "The message being sent by the IDirectPlay3::Send method is too large.";
    case DPERR_SESSIONLOST: return "The connection to the session has been lost.";
    case DPERR_SIGNFAILED: return "The requested information could not be digitally signed. Digital signatures are used to establish the authenticity of messages.";
    case DPERR_TIMEOUT: return "The operation could not be completed in the specified time.";
    case DPERR_UNAVAILABLE: return "The requested function is not available at this time.";
    case DPERR_UNINITIALIZED: return "The requested object has not been initialized.";
    case DPERR_UNKNOWNAPPLICATION: return "An unknown application was specified.";
    case DPERR_UNSUPPORTED: return "The function is not available in this implementation. Returned from IDirectPlay3::GetGroupConnectionSettings and IDirectPlay3::SetGroupConnectionSettings if they are called from a session that is not a lobby session.";
    case DPERR_USERCANCEL: return "Can be returned in two ways. 1) The user canceled the connection process during a call to the IDirectPlay3::Open method. 2) The user clicked Cancel in one of the DirectPlay service provider dialog boxes during a call to IDirectPlay3::EnumSessions.";
    default: return NULL;
  }
}

char* get_error_message(HRESULT hr) {
  LPTSTR message = NULL;
  FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    hr,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&message,
    0,
    NULL
  );

  if (message == NULL) {
    return get_dp_message(hr);
  }

  return message;
}

#else

char* get_error_message(HRESULT hr) {
  char message[200];
  sprintf(message, "Error ID #%ld", hr);

  int len = strlen(message);
  char* mem = calloc(len + 1, sizeof(char));
  memcpy(mem, message, len + 1);
  return mem;
}

#endif
