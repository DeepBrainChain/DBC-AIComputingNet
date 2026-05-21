//! Unix-socket JSON-RPC server.
//!
//! dbc-node (C++) connects to the socket and issues JSON-RPC calls. Connections
//! are authenticated by `SO_PEERCRED`: only the configured uid(s) can send calls.
//!
//! Methods exposed (MVP-1):
//!   * enable_container_mode(machine_id, port_range_start, port_range_end) -> TxOutcome
//!   * remove_container_mode(machine_id) -> TxOutcome
//!   * query_container_info(machine_id) -> Option<ContainerInfoView>
//!   * subscribe_events(filter) -> push channel (MVP-2)
//!   * get_machine_status(machine_id) -> aggregated state (MVP-2)

use anyhow::Result;
use jsonrpsee::core::async_trait;
use jsonrpsee::proc_macros::rpc;
use jsonrpsee::server::Server;
use std::sync::Arc;

use crate::chain::{ChainHandle, ContainerInfoView, TxOutcome};
use crate::Config;

#[rpc(server)]
pub trait SidecarApi {
    /// Submit containerMode.enable_container_mode.
    #[method(name = "enable_container_mode")]
    async fn enable_container_mode(
        &self,
        machine_id_hex: String,
        port_range_start: u16,
        port_range_end: u16,
    ) -> Result<TxOutcome, jsonrpsee::core::Error>;

    /// Submit containerMode.remove_container_mode.
    #[method(name = "remove_container_mode")]
    async fn remove_container_mode(
        &self,
        machine_id_hex: String,
    ) -> Result<TxOutcome, jsonrpsee::core::Error>;

    /// Read containerMode.containerModeMachines storage.
    #[method(name = "query_container_info")]
    async fn query_container_info(
        &self,
        machine_id_hex: String,
    ) -> Result<Option<ContainerInfoView>, jsonrpsee::core::Error>;
}

struct SidecarImpl {
    chain: Arc<ChainHandle>,
}

#[async_trait]
impl SidecarApiServer for SidecarImpl {
    async fn enable_container_mode(
        &self,
        machine_id_hex: String,
        port_range_start: u16,
        port_range_end: u16,
    ) -> Result<TxOutcome, jsonrpsee::core::Error> {
        let machine_id = hex::decode(machine_id_hex.trim_start_matches("0x"))
            .map_err(|e| jsonrpsee::core::Error::Custom(format!("bad machine_id hex: {e}")))?;
        self.chain
            .enable_container_mode(machine_id, port_range_start, port_range_end)
            .await
            .map_err(|e| jsonrpsee::core::Error::Custom(e.to_string()))
    }

    async fn remove_container_mode(
        &self,
        machine_id_hex: String,
    ) -> Result<TxOutcome, jsonrpsee::core::Error> {
        let machine_id = hex::decode(machine_id_hex.trim_start_matches("0x"))
            .map_err(|e| jsonrpsee::core::Error::Custom(format!("bad machine_id hex: {e}")))?;
        self.chain
            .remove_container_mode(machine_id)
            .await
            .map_err(|e| jsonrpsee::core::Error::Custom(e.to_string()))
    }

    async fn query_container_info(
        &self,
        machine_id_hex: String,
    ) -> Result<Option<ContainerInfoView>, jsonrpsee::core::Error> {
        let machine_id = hex::decode(machine_id_hex.trim_start_matches("0x"))
            .map_err(|e| jsonrpsee::core::Error::Custom(format!("bad machine_id hex: {e}")))?;
        self.chain
            .query_container_info(machine_id)
            .await
            .map_err(|e| jsonrpsee::core::Error::Custom(e.to_string()))
    }
}

pub async fn serve(cfg: Config, chain: ChainHandle) -> Result<Server> {
    // TODO(MVP-1): bind to Unix socket + SO_PEERCRED filter.
    // jsonrpsee's stock Server only supports TCP/WS by default. For MVP-1 we may
    // use a TCP listener on 127.0.0.1:<random> with shared-secret auth, then
    // switch to AF_UNIX with peer-cred check before mainnet.
    let _ = cfg;
    let chain = Arc::new(chain);
    let server = Server::builder().build("127.0.0.1:0").await?;
    let _ = SidecarImpl { chain }.into_rpc(); // wire up later
    Ok(server)
}
