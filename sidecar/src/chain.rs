//! Substrate client + signing layer.
//!
//! Owns the long-lived WS connection to DBC mainnet, holds the sr25519 keypair,
//! and exposes high-level methods (`enable_container_mode`, `remove_container_mode`,
//! `query_container_info`) that the RPC layer calls.

use anyhow::{Context, Result};
use std::path::Path;
use std::sync::Arc;

use crate::state::State;

// TODO(MVP-1): generate the typed runtime API from a `subxt-cli metadata` snapshot
// of spec 412. For now we use the dynamic API (untyped storage + extrinsic by name).
// Once the chain stabilises, switch to `#[subxt::subxt(runtime_metadata_path = "...")]`
// for compile-time-checked extrinsics.

pub struct ChainHandle {
    client: subxt::OnlineClient<subxt::PolkadotConfig>,
    signer: subxt_signer::sr25519::Keypair,
    state: Arc<State>,
}

impl ChainHandle {
    pub async fn connect(
        ws_endpoint: &str,
        key_path: &Path,
        state: Arc<State>,
    ) -> Result<Self> {
        let client = subxt::OnlineClient::<subxt::PolkadotConfig>::from_url(ws_endpoint)
            .await
            .with_context(|| format!("connect ws {}", ws_endpoint))?;

        let seed_bytes = std::fs::read(key_path)
            .with_context(|| format!("read key file {}", key_path.display()))?;
        anyhow::ensure!(
            seed_bytes.len() == 32 || seed_bytes.len() == 64,
            "key file must contain 32-byte seed or 64-byte secret"
        );
        let seed: [u8; 32] = seed_bytes[..32].try_into()?;
        let signer = subxt_signer::sr25519::Keypair::from_seed(seed)
            .map_err(|e| anyhow::anyhow!("invalid sr25519 seed: {e}"))?;

        Ok(Self { client, signer, state })
    }

    pub fn signer_ss58(&self) -> String {
        // Stub: real impl returns ss58 of self.signer.public_key()
        String::from("<signer-ss58>")
    }

    // ----- High-level methods called from RPC -----

    /// Submit `containerMode.enable_container_mode(machine_id, start, end)` and
    /// return the tx hash + included block.
    pub async fn enable_container_mode(
        &self,
        _machine_id: Vec<u8>,
        _port_range_start: u16,
        _port_range_end: u16,
    ) -> Result<TxOutcome> {
        // TODO(MVP-1): build dynamic call via
        //   subxt::dynamic::tx("ContainerMode", "enable_container_mode", vec![...])
        // sign+submit with self.signer, await Finalized, return hash + block.
        anyhow::bail!("not implemented yet (MVP-1 in progress)")
    }

    pub async fn remove_container_mode(&self, _machine_id: Vec<u8>) -> Result<TxOutcome> {
        anyhow::bail!("not implemented yet (MVP-1 in progress)")
    }

    pub async fn query_container_info(
        &self,
        _machine_id: Vec<u8>,
    ) -> Result<Option<ContainerInfoView>> {
        // Read storage: containerMode.containerModeMachines(machine_id)
        anyhow::bail!("not implemented yet (MVP-1 in progress)")
    }
}

#[derive(Debug, serde::Serialize)]
pub struct TxOutcome {
    pub tx_hash: String,
    pub block_hash: String,
    pub block_number: u32,
}

#[derive(Debug, serde::Serialize)]
pub struct ContainerInfoView {
    pub bonded_at_block: u32,
    pub spec_proof_committee_verified: bool,
    pub port_range_start: u16,
    pub port_range_end: u16,
}
