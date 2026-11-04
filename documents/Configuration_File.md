# Webserv Configuration Architecture

## What is a Config File and Why Do We Need It?

The configuration file stores important values that control how the server runs. It allows us to change the server’s behavior without modifying the source code. For example, we can change the port the server listens on by simply updating the configuration file instead of changing the code and recompiling the project. The server reads the configuration file at startup and runs according to the specified settings.

---

## Config File Example

    server {
        listen 8080;

        location / {
            root /var/www;
        }

        location /images {
            root /var/www/images;
        }
    }

---

## Config File Format

The configuration file format is free to choose, as there are no strict rules defining how it should look. We can decide how we want the format to look and define the rules for parsing it. However, it is recommended to follow a widely used configuration style. Thus, we will take inspiration from the NGINX configuration format.

---

## Core Syntax Rules

### - Blocks `{}`
The config uses nested blocks enclosed in curly braces (`{}`).

There are 2 types of blocks: 
* `server` — defines the configuration for a virtual server.
* `location` — defines configuration for a specific URI path.

#### How blocks work

A `server` block defines an independent virtual server that listens on specific `interface:port` pairs. 

`location` blocks can only be defined inside a `server` block. Each `location` block applies its settings to a specific URI path or prefix (e.g., /images or /upload). This allows to define different settings for different parts of the server while keeping general settings at the server level.

### - Semocolons `;`
Every non-block directive must end with a semicolon (`;`). 

*\*Structural blocks (`server` and `location`) do not use semicolons after their closing braces (`}`).*

### - Comments `#`
The `#` symbol is used for comments. Anything from the `#` to the end of the line is ignored.

### - Quote Escaping `\`
Backslash (`\`) is used for placing quotes inside same-style quotes (e.g., `"smth\"smth"`).

---

## Supported Directives

Below is the initial set of directives that is highly recommended to support by our parser.

* `server` — Defines a virtual server block.
* `listen` — Sets the `interface:port` pairs on which the server listens.
* `server_name` — Specifies hostnames for virtual hosting.
* `client_max_body_size` — Restricts the maximum body size of client requests (protects against large payloads).
* `client_max_header_size` — Restricts the maximum head size of client requests (request line + headers).
* `client_max_response_size` — Restricts the maximum body size of client responses.
* `connections_max_number` — Restricts the maximum number of client connections (for a virtual server).
* `error_page` — Configures custom fallback pages for specific HTTP error codes.
* `location` — Scopes configuration rules to a specific URL path prefix.
* `root` — Defines the root directory on disk where requested files are located.
* `index` — Sets the default file to serve when a directory is requested (e.g., `index.html`).
* `autoindex` — Enables (`on`) or disables (`off`) directory listing when no index file is found.
* `methods` — Restricts allowed HTTP methods (e.g., allowing `GET` and `POST` while blocking others).
* `return` — Configures HTTP redirections with a status code and target URL.
* `cgi_pass` — Binds file extensions to external CGI interpreters (e.g., `.php` to `/usr/bin/php-cgi`).

### Supported `listen` format

```nginx
listen 8080;           # Uses 0.0.0.0:8080
listen 127.0.0.1:8080; # Explicit IPv4 address and port
```

Multiple `listen` directives are allowed in each server block. If none is
specified, the parser adds a default listener at `0.0.0.0:80` and prints an
informational message. Explicit `listen` directives are preserved; no extra
default listener is added when at least one is present.

Ports must contain decimal digits and have a value from 1 through 65535;
leading zeros in ports are accepted.

Explicit addresses use IPv4 only: exactly four dot-separated decimal octets,
each containing 1-3 ASCII digits with a value from 0 through 255. A multi-digit
octet must not start with zero. Hostnames, `*`, IPv6, abbreviated addresses,
hexadecimal forms, and subnet suffixes are not supported.

Address validation checks syntax. Whether the address is configured locally or
the port is available is checked when the networking code binds the socket.
Special-purpose ranges are not rejected by the IPv4 syntax validator:
`0.0.0.0` is the wildcard bind address, and loopback/private addresses are valid
configuration choices. Broadcast or multicast suitability belongs to the TCP
listener/networking layer. Do not reject every address ending in `.0` or `.255`;
their role depends on the subnet prefix, which is not part of this format.

---

## Config File Parsing

The config parsing pipeline is split into three robust phases to catch errors early and convert raw text into a clean hierarchical object tree:

### 1. Read Config File

* Loads the raw text from the specified file path into memory safely, handling file-reading exceptions or permission errors.

### 2. Parse

The parsing stage processes the text through several sub-steps:

* **Check File Format**: Validates structural integrity, including paired braces (`{}`), paired quotes (`"` / `'`), and the presence of semicolons on non-block directives.
* **Tokenize Content**: Splits the file content into tokens.
  * *Sanitation*: Removes comments, normalizes white spaces, and strips BOM headers.
  * *Tokenization*: Breaks the stream into meaningful tokens while verifying directive names and their valid scopes (whitelist approach).
  * *Post-Processing*: Cleans up tokens by removing external quotes and resolving escape characters (`\`).

### 3. Validate Content

Performs deep semantic checks on parsed values (e.g., verifying that port numbers fall within the valid range `1–65535`, `autoindex` only receives `on` or `off`, and paths are syntactically sound).
