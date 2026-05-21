//! dbc-chain-sidecar
//!
//! Local Rust process that owns the substrate WS connection + sr25519 signing
//! for dbc-node (C++). dbc-node talks to this sidecar via Unix-socket JSON-RPC.
//!
//! See `release/DBC_NODE_CONTAINER_MODE_DESIGN.md` §5 for the protocol design.
//!
//! MVP-1 scope (≤300 lines of real logic):
//!   * Connect to DBC mainnet WS endpoint (configurable)
//!   * Maintain one signed keypair from file (0600); persist nonce in sqlite
//!   * Expose `enable_container_mode`, `remove_container_mode`,
//!     `query_container_info`, `subscribe_events`, `get_machine_status`
//!   * Unix-socket JSON-RPC server with `SO_PEERCRED` uid check (MVP-2 adds TPM)
//!
//! Out of scope for MVP-1 (deferred):
//!   * HSM / TPM-sealed keys
//!   * DDN keypair (DDN is a separate Go process with its own substrate client)
//!   * Multi-account support
//!   * Hot reload

use anyhow::Result;
use clap::Parser;
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use tracing::{info, warn};

mod chain;
mod rpc;
mod state;

#[derive(Parser, Debug)]
#[command(name = "dbc-chain-sidecar", version)]
struct Args {
    /// TOML config file
    #[arg(short, long, default_value = "/etc/dbc-chain-sidecar/config.toml")]
    config: PathBuf,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Config {
    /// Substrate WS endpoint (e.g. "wss://rpc.dbcwallet.io")
    pub ws_endpoint: String,
    /// Path to sr25519 keystore file (0600). Format: 32-byte raw seed.
    pub key_path: PathBuf,
    /// Unix socket path for the JSON-RPC server (e.g. "/var/run/dbc-node/sidecar.sock")
    pub socket_path: PathBuf,
    /// sqlite path for nonce + last-seen-block persistence
    pub state_db: PathBuf,
    /// Allowed peer UIDs (only these can call the JSON-RPC). Defaults to current uid.
    #[serde(default)]
    pub allowed_uids: Vec<u32>,
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .with_target(false)
        .init();

    let args = Args::parse();
    let cfg_text = std::fs::read_to_string(&args.config)?;
    let cfg: Config = toml::from_str(&cfg_text)?;
    info!("loaded config from {}", args.config.display());

    // Open state DB (sqlite) and chain handle.
    let state = state::open(&cfg.state_db)?;
    let chain = chain::ChainHandle::connect(&cfg.ws_endpoint, &cfg.key_path, state.clone()).await?;
    info!("connected to {} as {}", cfg.ws_endpoint, chain.signer_ss58());

    // Spawn the JSON-RPC server; race it against SIGTERM / SIGINT.
    let server = tokio::spawn(rpc::serve(cfg.clone(), chain));

    tokio::select! {
        _ = tokio::signal::ctrl_c() => {
            warn!("ctrl-c received; shutting down");
        }
        res = server => match res {
            Ok(Ok(())) => warn!("rpc server exited cleanly"),
            Ok(Err(e)) => warn!("rpc server returned error: {e}"),
            Err(e) => warn!("rpc server panicked: {e}"),
        },
    }
    Ok(())
}
