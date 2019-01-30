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

## DPRun Service Provider

This app ships with a DirectPlay service provider dprun.dll. Service Providers are the things that actually handle network communication, so this can be used to replace the default TCP/IP network stack. When starting a game using the `DPRUN` service provider (`--service-provider=DPRUN`), DPRun temporarily registers this .dll file with the DirectPlay Service Providers list, and uses it to start the game.

The DPRun service provider is different from most service providers in that it doesn't do any networking. Instead, it sends all game communication packets to a local socket of your choosing, so you can implement your own networking stack inside a host application. This host application can live on Windows, inside Wine, or even in Linux/macOS outside Wine.

To configure the socket address, use the INet and INetPort address types:

```
--service-provider DPRUN
--address INet=127.0.0.1
--address INetPort=i:3456
```

The DPRun Service Provider needs some metadata to identify the players that send and receive messages. You should pass in a third address type, SelfID. SelfID is a GUID (16 random bytes), encoded as a hexadecimal string:

```
--address SelfID=b:1e1bf813fd161cf55e3e2a3d8d3e2a48
```

Further documentation on the socket API may follow :)

## dpenumerate

List applications registered with directplay.

```
dpenumerate [options]

Options:
  -c, --csv
      CSV output.
```

## Building

### Linux

This project uses the MinGW GCC compiler suite.

Run `make` to build both the Service Provider and the DPRun app in debug mode. The built files will be placed in `bin/debug`.

Run `make release` to build both the Service Provider and the DPRun app in release mode. The built files will be placed in `bin/release`.

Run `make lint` to run the CLang analyzer.

### Windows

This project can be compiled using [MSYS2](http://www.msys2.org/) with [mingw-w64](https://mingw-w64.org/doku.php).

1. Download and instal [MSYS2](http://www.msys2.org/)
2. On MSYS2 terminal install `mingw-w64-i686-gcc`, `mingw-w64-i686-gdb` and `make` packages:

```
pacman -Syu
pacman -S mingw-w64-i686-gcc mingw-w64-i686-gdb make
```

3. Configure `PATH` environmental variables (suposing you've install msys64 on `C:\msys64`):
    - `C:\msys64\usr\bin`
    - `C:\msys64\mingw32\bin`
    - `C:\msys64\mingw64\bin`

Run `make` to build both the Service Provider and the DPRun app in debug mode. The built files will be placed in `bin/debug`.

Run `make release` to build both the Service Provider and the DPRun app in release mode. The built files will be placed in `bin/release`.

Run `make lint` to run the CLang analyzer.

## License

[GPL-3.0](./LICENSE.md)
