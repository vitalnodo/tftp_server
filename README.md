# tftp_server

A TFTP server (RFC 1350) written in C++17.

## Features

- RRQ (download) and WRQ (upload)
- Concurrent sessions — each transfer runs in its own thread
- RFC-compliant TIDs — server responds from a unique ephemeral port per session
- Dual-stack — accepts both IPv4 and IPv6 clients on a single socket
- Path traversal protection — rejects `..` and absolute paths
- 5-second receive timeout per session to prevent hung transfers

## Build

Requires a C++17 compiler and CMake 3.10+.

```sh
cmake -B build      # configure (run once)
cmake --build build # compile
```

## Usage

```
./build/tftp_server [root_dir] [port]

  root_dir  directory to serve files from (default: .)
  port      UDP port to listen on       (default: 69, requires root)
```

Examples:

```sh
sudo ./build/tftp_server                  # serve . on port 69
sudo ./build/tftp_server /srv/tftp        # serve /srv/tftp on port 69
     ./build/tftp_server ./files 6969     # serve ./files on port 6969
```

## Tests

```sh
./build/tftp_tests
```
