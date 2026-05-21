//! Substrate client + signing layer.
//!
//! Owns the long-lived WS connection to DBC mainnet, holds the sr25519 keypair,
//! and exposes high-level methods (`enable_container_mode`,
//! `remove_container_mode`, `query_container_info`) that the RPC layer calls.
//!
//! Subxt 0.34 is our floor: it has the standalone `subxt-signer` crate which
//! avoids the sp-core/schnorrkel/substrate-bip39 diamond present in 0.27.

use anyhow::{Context, Result};
use parity_scale_codec::Decode;
use std::path::Path;
use std::sync::Arc;
use subxt::dynamic::{storage as dyn_storage, Value};
use subxt::tx::dynamic as dyn_tx;
use subxt::{OnlineClient, PolkadotConfig};
use subxt_signer::sr25519::Keypair;

use crate::state::State;

pub struct ChainHandle {
    client: OnlineClient<PolkadotConfig>,
    signer: Keypair,
    state: Arc<State>,
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
        // subxt_signer 0.34 takes a 32-byte secret/seed via `from_seed`.
        let signer = Keypair::from_seed(seed)
            .map_err(|e| anyhow::anyhow!("invalid sr25519 seed: {e:?}"))?;

        let account_id = signer.public_key().to_account_id();
        let signer_ss58 = account_id.to_string();

        Ok(Self { client, signer, state, signer_ss58 })
    }

    pub fn signer_ss58(&self) -> &str {
        &self.signer_ss58
    }

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

    pub async fn remove_container_mode(&self, machine_id: Vec<u8>) -> Result<TxOutcome> {
        let call = dyn_tx(
            "ContainerMode",
            "remove_container_mode",
            vec![Value::from_bytes(machine_id)],
        );
        self.submit_and_wait(call).await
    }

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
        let bytes = raw.encoded();
        let mut slice = bytes;
        let view = ContainerInfoView::decode(&mut slice)
            .context("decode ContainerModeInfo")?;
        Ok(Some(view))
    }

    async fn submit_and_wait<C>(&self, call: C) -> Result<TxOutcome>
    where
        C: subxt::tx::TxPayload,
    {
        // subxt 0.34 auto-fetches the nonce inside `_default`; this is fine
        // for MVP-1 (single sidecar instance per signer). The sqlite-backed
        // nonce reconciliation we used in 0.27 is restored when MVP-2 needs
        // to support concurrent submitters.

        let mut progress = self
            .client
            .tx()
            .sign_and_submit_then_watch_default(&call, &self.signer)
            .await
            .context("sign + submit")?;

        let events = progress
            .wait_for_finalized_success()
            .await
            .context("wait for finalized success")?;

        let block_hash = events.block_hash();
        let block_number = self
            .client
            .blocks()
            .at(block_hash)
            .await
            .context("fetch block header")?
            .number();

        Ok(TxOutcome {
            tx_hash: format!("{:?}", events.extrinsic_hash()),
            block_hash: format!("{block_hash:?}"),
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
