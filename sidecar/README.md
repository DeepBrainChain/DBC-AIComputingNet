# dbc-chain-sidecar

Lightweight Rust daemon that bridges between the C++ miner client
(this repository's `src/`) and the DBC mainchain substrate node.

Lives in-tree at `DBC-AIComputingNet/sidecar/` so the miner and its
chain adapter ship in one repo / one release tarball / one CI
pipeline. The binary still runs as a separate process on the miner
host (own systemd unit, own keypair, own log stream).

## Why

- The miner client (`dbc-node`) is ~30k lines of C++17 with no native substrate
  RPC support. Direct subxt usage in C++ is impractical.
- DDN (the Go health-monitor) already speaks substrate via `gsrpc`; the miner
  side needs its own signer.
- Keeping substrate metadata + SCALE codec + sr25519 signing in Rust lets us
  upgrade the chain protocol independently of the C++ binary.

## Architecture

```
[ DBC-AIComputingNet (C++17) ]
            │
            │ Unix-socket JSON-RPC (SO_PEERCRED-gated)
            ▼
[ dbc-chain-sidecar (Rust, this crate) ]
            │
            │ subxt 0.27  (matches polkadot-v0.9.43 / DBC spec 412)
            ▼
[ DBC mainnet WS endpoint ]
```

## MVP-1 scope

- Submit `containerMode.enable_container_mode` and `remove_container_mode`.
- Read `containerMode.containerModeMachines` storage.
- One sr25519 keypair, loaded from a 0600 seed file at startup.
- sqlite-backed nonce + last-seen-block persistence.

Deferred to MVP-2/3:

- TPM-sealed credentials via `systemd-creds`.
- Event subscription / push to dbc-node.
- Multi-account / key rotation.
- Hot reload of config.

## Build

```bash
cargo build --release
```

## Run

```bash
sudo install -m 0700 -d /etc/dbc-chain-sidecar
sudo cp config.example.toml /etc/dbc-chain-sidecar/config.toml
sudo chmod 0600 /etc/dbc-chain-sidecar/config.toml
# generate a fresh sr25519 seed
sudo bash -c "head -c 32 /dev/urandom > /etc/dbc-chain-sidecar/signer.seed && chmod 0600 /etc/dbc-chain-sidecar/signer.seed"

./target/release/dbc-chain-sidecar --config /etc/dbc-chain-sidecar/config.toml
```

## See also

- `DBCDEVOPS/release/PRD_container_mode_MVP1.md` — full MVP-1 product spec.
- `DBCDEVOPS/release/DBC_NODE_CONTAINER_MODE_DESIGN.md` — dbc-node C++
  integration design (§5 sidecar protocol).
