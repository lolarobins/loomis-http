# loomis-http
Simple static webpage HTTP server for POSIX-compliant operating systems.
Created for educational purposes, works as a simple lightweight & fast http server.

## Install
1. Clone the repository.
2. Run `make`.
3. Upload any public files into `web/`.
4. Launch the server with `loomis-http [flags]`.

## Launch Flags
* `-6` Use IPV6
* `-v` Verbose Output
* `-a <address>` Server Address (default 127.0.0.1)
* `-p <port>` Server Port (default 80)
* `-- <directory` Directory to host
