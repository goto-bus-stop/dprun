# DPRun

DPRun is a standalone C Windows/Wine utility for starting DirectPlay lobbyable applications.

The idea is to have a small application that can handle the lobby setup, and exposes an API that a real lobby application can use to start games. This way a lobby application can run natively on eg. Linux and macOS, and only run the absolutely necessary bits in Wine.

By "small" I mean I hope this can be done well in something like 1000 lines.

## Usage

You can launch a DirectPlay session using the command line.

```
dprun <--host|--join> [options]

-H, --host [session]
    Host a DirectPlay session.
    [session] is optional, and can contain a GUID that will be used as the session instance ID.
    If omitted, a random GUID is generated.
-j, --join [session]
    Join a DirectPlay session.
    [session] is the GUID for the session.

Options:
  -p, --player [name]
      The name of the local player.
  -s, --service-provider [guid]
      The GUID of the service provider to use.
      This field also supports constant values: TCPIP, IPX, SERIAL, MODEM, DPRUN
  -A, --application [guid]
      The GUID of the application to start.

  --address [key]=[value]
      Add an address part. This flag can appear more than once.
      The [value] is the string value of the address part.
      To specify a numeric value, use "i:12345".
      To specify a binary value, use "b:[hex encoded value]", for example "b:DEADBEEF".
      The [key] field is the GUID for the address data type. It also supports constant values:
          TotalSize, ServiceProvider, LobbyProvider, Phone, PhoneW,
          Modem, ModemW, INet, INetW, INetPort, ComPort
  --session-name [name]
      The name of the session to host or join (optional).
  --session-password [password]
      The password for the session to host or join (optional).

GUIDs passed to dprun must be formatted like below, including braces and dashes:
    {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    {685BC400-9D2C-11cf-A9CD-00AA006886E3}
```

## License

[GPL-3.0](./LICENSE.md)
