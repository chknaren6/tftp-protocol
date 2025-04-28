# TFTP(Trivial File Transfer Protocol) Implementation in C

## Description

This project implements the Trivial File Transfer Protocol (TFTP) client and/or server written in the C programming language. It aims to provide basic file transfer capabilities (read and write) based on RFC 1350.

## Features

* **TFTP Client:**
    * Supports Read Requests (RRQ) - `GET` operation.
    * Supports Write Requests (WRQ) - `PUT` operation.
* **TFTP Server:**
    * Handles incoming RRQ and WRQ requests.
    * **Transfer Modes:**
    * Supports `netascii` mode.
    * Supports `octet` (binary) mode.
* **Error Handling:** This implements TFTP error packet reporting for common issues (e.g., File not found, Access violation, Disk full).

## Requirements

* A C compiler (e.g., GCC)
* Make (optional, for easy building)
* Standard C libraries

## Building

1.  **Clone the repository (if applicable):**
    ```bash
    git clone https://github.com/chknaren6/tftp-protocol.git
    cd tftp-protocol
    ```
2.  **Compile using Make:**
    ```bash
    make
    ```
    This will typically compile the source files (e.g., `tftp_client.c`, `tftp_server.c`) and create executables (e.g., `tftp_client`, `tftp_server`).

3.  **Compile manually (example):**
    ```bash
    # Compile client
    gcc tftp_client.c

    # Compile server
    gcc tftp_server.c common.c -o tftp_server    ```
    

## Usage

### Client

```bash
./tftp_client <server_ip> <GET|PUT> <remote_filename> [local_filename] [mode]
```

Arguments:
<server_ip>: IP address or hostname of the TFTP server.
<GET|PUT>: Operation type (case-insensitive).
GET: Download remote_filename from the server.
PUT: Upload local_filename to the server as remote_filename.
<remote_filename>: The name of the file on the server.
[local_filename]: (Required for PUT, optional for GET) The name of the file on the local machine. If omitted for GET, it defaults to remote_filename.
Examples:
Get a file:
```bash
./tftp_client 192.168.1.10 GET document.txt local_doc.txt
./tftp_client server.example.com GET image.jpg
```

Put a file:
```bash
./tftp_client 192.168.1.10 PUT config.bin remote_config.bin
./tftp_client server.example.com PUT report.txt
```

Server 
```bash
./tftp_server [port] [root_directory]
```

Arguments:
[port]: (Optional) The UDP port number to listen on. Defaults to the standard TFTP port 69.
[root_directory]: (Optional) The directory from which the server will serve files. Defaults to the current directory (.). Security Note: Ensure this directory has appropriate permissions set.
Example:
# Run server on default port 69, serving from /srv/tftp
```bash
./tftp_server 69 /srv/tftp
```

# Run server on port 6969, serving from the current directory
```bash
./tftp_server 6969.
```

##Protocol Details
This implementation follows the specifications outlined in RFC 1350.
The standard TFTP block size of 512 bytes is used.

###Error Handling
The implementation handles standard TFTP error codes as defined in RFC 1350:
0: Not defined, see error message (if any).
1: File not found.
2: Access violation.
3: Disk full or allocation exceeded.
4: Illegal TFTP operation.
5: Unknown transfer ID.
6: File already exists.
7: No such user.
Error messages are printed to standard error (stderr) on both the client and server side, where applicable.

[Visit RFC 1350 Specification](https://datatracker.ietf.org/doc/html/rfc1350)


