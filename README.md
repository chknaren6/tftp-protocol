TFTP Protocol (Trivial File Transfer Protocol) - C Implementation
A complete TFTP (RFC 1350) client and server implementation in C, supporting basic file transfer operations with both octet (binary) and netascii (text) modes.

Features
TFTP Client

Supports Read Requests (RRQ) — GET operation

Supports Write Requests (WRQ) — PUT operation

TFTP Server

Handles incoming RRQ and WRQ requests

Supports netascii and octet transfer modes

Configurable server port and root directory

Error Handling

Standard TFTP error codes (e.g., File not found, Access violation, Disk full)

Protocol Compliance

Standard 512-byte block size as per RFC 1350

Requirements
C compiler (e.g., GCC)

make (optional, for easier build management)

Standard C Libraries

Building the Project
1. Clone the Repository
bash
Copy
Edit
git clone https://github.com/chknaren6/tftp-protocol.git
cd tftp-protocol
2. Build using make
bash
Copy
Edit
make
This compiles the source files and generates:

tftp_client

tftp_server

3. Manual Compilation (alternative)
bash
Copy
Edit
# Build client
gcc tftp_client.c common.c -o tftp_client

# Build server
gcc tftp_server.c common.c -o tftp_server
Adjust commands if your file structure is different.

Usage
TFTP Client
bash
Copy
Edit
./tftp_client <server_ip> <GET|PUT> <remote_filename> [local_filename] [mode]
Arguments:

<server_ip>: IP address or hostname of the TFTP server

<GET|PUT>: Operation (case-insensitive)

<remote_filename>: File name on the server

[local_filename]: (Optional) Local file name. Defaults to remote_filename if omitted in GET

[mode]: (Optional) Transfer mode (octet or netascii). Defaults to octet

Examples:

Download a file:

bash
Copy
Edit
./tftp_client 192.168.1.10 GET document.txt
./tftp_client server.example.com GET image.jpg local_image.jpg
Upload a file:

bash
Copy
Edit
./tftp_client 192.168.1.10 PUT local_config.bin remote_config.bin
./tftp_client server.example.com PUT report.txt
TFTP Server
bash
Copy
Edit
./tftp_server [port] [root_directory]
Arguments:

[port]: (Optional) UDP port to listen on (default: 69)

[root_directory]: (Optional) Directory to serve files from (default: current directory .)

Examples:

Serve files from /srv/tftp on default port 69:

bash
Copy
Edit
./tftp_server 69 /srv/tftp
Serve files from the current directory on port 6969:

bash
Copy
Edit
./tftp_server 6969 .
Security Note: Ensure proper permissions are set on the root_directory.

Protocol Details
Follows RFC 1350

Uses 512-byte block size for file transfer

Limitations:

No support for TFTP extensions like block size negotiation (RFC 2348) or transfer size options (RFC 2349)

Error Handling
Standard TFTP error codes are implemented:


Code	Meaning
0	Not defined (see message)
1	File not found
2	Access violation
3	Disk full or allocation exceeded
4	Illegal TFTP operation
5	Unknown transfer ID
6	File already exists
7	No such user
Errors are logged to stderr on both client and server sides.

Resources
RFC 1350: The TFTP Protocol (Revision 2)

License
This project is released under the MIT License.

