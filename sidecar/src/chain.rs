//! Substrate client + signing layer.
//!
//! Owns the long-lived WS connection to DBC mainnet, holds the sr25519 keypair,
//! and exposes high-level methods (`enable_container_mode`,
//! `remove_container_mode`, `query_container_info`) that the RPC layer calls.
//!
//! Subxt API note: this file uses the **dynamic** API (`dyn_tx`, `dyn_storage`).
//! Once spec 412 stabilises on mainnet we will switch to the typed
//! `#[subxt::subxt(runtime_metadata_path = "metadata-412.scale")]` form for
//! compile-time-checked extrinsics. The dynamic API is sufficient for MVP-1
//! and survives future field additions to ContainerModeInfo without recompile.

use anyhow::{Context, Result};
use parity_scale_codec::Decode;
use std::path::Path;
use std::sync::Arc;
use subxt::dynamic::{storage as dyn_storage, tx as dyn_tx, Value};
use subxt::utils::AccountId32;
use subxt::{OnlineClient, PolkadotConfig};
use subxt_signer::sr25519::Keypair;

use crate::state::State;

pub struct ChainHandle {
    client: OnlineClient<PolkadotConfig>,
    signer: Keypair,
    state: Arc<State>,
    /// SS58 of the signer (cached for logging + RPC responses).
    signer_ss58: String,
}

impl ChainHandle {
    pub async fn connect(
        ws_endpoint: &str,
        key_path: &Path,
        state: Arc<State>,
    ) -> Result<Self> {
        let client = OnlineClient::<PolkadotConfig>::from_url(ws_endpoint)
            .await
            .with_context(|| format!("connect ws {ws_endpoint}"))?;

        let seed_bytes = std::fs::read(key_path)
            .with_context(|| format!("read key file {}", key_path.display()))?;
        anyhow::ensure!(
            seed_bytes.len() == 32 || seed_bytes.len() == 64,
            "key file must contain 32-byte seed or 64-byte secret"
        );
        let seed: [u8; 32] = seed_bytes[..32].try_into()?;
        let signer = Keypair::from_seed(seed)
            .map_err(|e| anyhow::anyhow!("invalid sr25519 seed: {e}"))?;

        let account_id = AccountId32::from(signer.public_key().0);
        let signer_ss58 = account_id.to_string();

        Ok(Self { client, signer, state, signer_ss58 })
    }

    pub fn signer_ss58(&self) -> &str {
        &self.signer_ss58
    }

    // ----- High-level methods called from rpc.rs -----

    /// Submit `containerMode.enable_container_mode(machine_id, start, end)`,
    /// wait for inclusion in a block, return the resulting hash + block info.
    pub async fn enable_container_mode(
        &self,
        machine_id: Vec<u8>,
        port_range_start: u16,
        port_range_end: u16,
    ) -> Result<TxOutcome> {
        let call = dyn_tx(
            "ContainerMode",
            "enable_container_mode",
            vec![
                Value::from_bytes(machine_id),
                Value::u128(port_range_start as u128),
                Value::u128(port_range_end as u128),
            ],
        );
        self.submit_and_wait(call).await
    }

    /// Submit `containerMode.remove_container_mode(machine_id)` (stash path).
    pub async fn remove_container_mode(&self, machine_id: Vec<u8>) -> Result<TxOutcome> {
        let call = dyn_tx(
            "ContainerMode",
            "remove_container_mode",
            vec![Value::from_bytes(machine_id)],
        );
        self.submit_and_wait(call).await
    }

    /// Read `ContainerMode.ContainerModeMachines(machine_id)` from chain state.
    /// Returns `Ok(None)` if the machine is not in container mode.
    pub async fn query_container_info(
        &self,
        machine_id: Vec<u8>,
    ) -> Result<Option<ContainerInfoView>> {
        let address = dyn_storage(
            "ContainerMode",
            "ContainerModeMachines",
            vec![Value::from_bytes(machine_id)],
        );
        let raw = self
            .client
            .storage()
            .at_latest()
            .await
            .context("storage at_latest")?
            .fetch(&address)
            .await
            .context("storage fetch")?;
        let Some(raw) = raw else { return Ok(None); };

        // ContainerModeInfo<BlockNumber> is { bonded_at_block: u32,
        //   spec_proof_committee_verified: bool,
        //   port_range_start: u16, port_range_end: u16 }
        // Subxt returns the SCALE-encoded leaf; we decode into our local mirror.
        let bytes = raw.encoded();
        let mut slice = bytes;
        let view = ContainerInfoView::decode(&mut slice)
            .context("decode ContainerModeInfo")?;
        Ok(Some(view))
    }

    // ----- Internal -----

    async fn submit_and_wait<C>(&self, call: C) -> Result<TxOutcome>
    where
        C: subxt::tx::Payload,
    {
        let account_id = AccountId32::from(self.signer.public_key().0);

        // Nonce reconciliation: take the larger of (chain.system.account.nonce,
        // last persisted local + 1). Persist the chosen nonce so a restart
        // mid-flight doesn't double-spend.
        let chain_nonce: u64 = self
            .client
            .tx()
            .account_nonce(&account_id)
            .await
            .context("fetch system.account.nonce")?;
        let local_nonce = self
            .state
            .get_nonce(&self.signer_ss58)?
            .map(|n| n.saturating_add(1))
            .unwrap_or(chain_nonce);
        let nonce = std::cmp::max(chain_nonce, local_nonce);
        self.state.set_nonce(&self.signer_ss58, nonce)?;

        let params = subxt::config::polkadot::PolkadotExtrinsicParamsBuilder::new()
            .nonce(nonce)
            .build();

        let in_block = self
            .client
            .tx()
            .sign_and_submit_then_watch(&call, &self.signer, params)
            .await
            .context("sign + submit")?
            .wait_for_in_block()
            .await
            .context("wait in_block")?;

        let block_hash = in_block.block_hash();
        let block_number = self
            .client
            .blocks()
            .at(block_hash)
            .await
            .context("fetch block header")?
            .number();

        Ok(TxOutcome {
            tx_hash: format!("{:?}", in_block.extrinsic_hash()),
            block_hash: format!("{:?}", block_hash),
            block_number,
        })
    }
}

#[derive(Debug, serde::Serialize)]
pub struct TxOutcome {
    pub tx_hash: String,
    pub block_hash: String,
    pub block_number: u32,
}

#[derive(Debug, serde::Serialize, Decode)]
pub struct ContainerInfoView {
    pub bonded_at_block: u32,
    pub spec_proof_committee_verified: bool,
    pub port_range_start: u16,
    pub port_range_end: u16,
}
