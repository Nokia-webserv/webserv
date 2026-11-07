# IPv4 validation tests

Run from the project root:

```sh
make
bash tests/config/test_config.sh
```

The 20 `ipv4_*.conf` files run with the existing config tests:

- 4 valid configs: loopback, wildcard, default host, octet/port boundaries.
- 16 invalid configs: missing host, wrong number of parts, misplaced dots,
  empty parts, values above 255, too many digits, letters, signs, leading zeros,
  whitespace, hostname, wildcard `*`, and IPv6.

These tests check configuration parsing only; they do not open sockets.
An address ending in `.255` is not automatically invalid: its network role
depends on the subnet prefix. No subnet is specified in this config format.
