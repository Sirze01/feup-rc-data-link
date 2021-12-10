# Data Link Protocol

A performance-driven implementation of a logical link protocol to operate over the `RS-232` device driver and ensure an error free transmission between two peers. For implementation details, refer to the report on `docs/report.pdf`.

A simple application for transferring a single file and collecting transfer statistics is provided to demonstrate a usage case scenario of the protocol.

## How to run

### Compilation

Run `make` at the project root. Protocol settings may be changed through make arguments for testing purposes:

- BAUDRATE
- CONNECTION_TIMEOUT_TS
- CONNECTION_MAX_TRIES
- INDUCED_FER
- INDUCED_DELAY_US

### Application usage

./application [-v] -p port -s filepath -r outdirectory [-n filename] [-b dataperpacket]