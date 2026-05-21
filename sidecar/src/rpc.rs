//! Unix-socket line-delimited JSON-RPC server.
//!
//! dbc-node (C++) connects to the socket and issues JSON-RPC calls. Each
//! request is a single line of JSON terminated by `\n`; the response is
//! likewise one line. We use the JSON-RPC 2.0 envelope shape:
//!
//!   request:  `{ "jsonrpc": "2.0", "id": <n>, "method": "...", "params": {...} }\n`
//!   response: `{ "jsonrpc": "2.0", "id": <n>, "result": {...} }\n`
//!         or  `{ "jsonrpc": "2.0", "id": <n>, "error": {code, message} }\n`
//!
//! Authn: SO_PEERCRED is checked at accept time; only callers whose effective
//! uid matches the configured allowlist (or the sidecar's own euid by default)
//! may issue calls. This pins the IPC to processes the host operator
//! explicitly co-located with the sidecar.
//!
//! We do NOT depend on jsonrpsee or hyper here: the wire format is small
//! enough that handling it directly is simpler than wedging an HTTP server
//! onto AF_UNIX.

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::os::unix::fs::PermissionsExt;
use std::os::unix::io::AsRawFd;
use std::sync::Arc;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{UnixListener, UnixStream};
use tracing::{debug, info, warn};

use crate::chain::ChainHandle;
use crate::Config;

/// JSON-RPC request envelope.
#[derive(Debug, Deserialize)]
struct Request {
    #[serde(default)]
    jsonrpc: Option<String>,
    id: Value,
    method: String,
    #[serde(default)]
    params: Value,
}

/// JSON-RPC response envelope.
#[derive(Debug, Serialize)]
struct ResponseEnvelope<'a> {
    jsonrpc: &'static str,
    id: &'a Value,
    #[serde(skip_serializing_if = "Option::is_none")]
    result: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<RpcError>,
}

#[derive(Debug, Serialize)]
struct RpcError {
    code: i32,
    message: String,
}

/// Bind the AF_UNIX socket and run forever.
pub async fn serve(cfg: Config, chain: ChainHandle) -> Result<()> {
    if cfg.socket_path.exists() {
        std::fs::remove_file(&cfg.socket_path)
            .with_context(|| format!("remove stale socket {}", cfg.socket_path.display()))?;
    }
    if let Some(parent) = cfg.socket_path.parent() {
        std::fs::create_dir_all(parent)?;
    }

    let listener = UnixListener::bind(&cfg.socket_path)
        .with_context(|| format!("bind {}", cfg.socket_path.display()))?;

    // Defence-in-depth: even though SO_PEERCRED is the real authn, tighten the
    // socket's filesystem permission so non-uid processes can't even connect.
    std::fs::set_permissions(&cfg.socket_path, std::fs::Permissions::from_mode(0o600))?;

    let chain = Arc::new(chain);
    let allowed_uids = if cfg.allowed_uids.is_empty() {
        vec![unsafe { libc::geteuid() }]
    } else {
        cfg.allowed_uids.clone()
    };

    info!(
        "sidecar listening on {} (allowed uids: {:?})",
        cfg.socket_path.display(),
        allowed_uids
    );

    loop {
        let (stream, _addr) = listener.accept().await.context("accept")?;
        let chain = chain.clone();
        let allowed_uids = allowed_uids.clone();
        tokio::spawn(async move {
            if let Err(e) = handle_connection(stream, chain, &allowed_uids).await {
                warn!("connection ended with error: {e}");
            }
        });
    }
}

async fn handle_connection(
    stream: UnixStream,
    chain: Arc<ChainHandle>,
    allowed_uids: &[u32],
) -> Result<()> {
    let peer_uid = peer_uid(&stream)?;
    if !allowed_uids.contains(&peer_uid) {
        warn!("rejecting connection from uid {peer_uid} (not in allowlist)");
        return Ok(());
    }
    debug!("accepted connection from uid {peer_uid}");

    let (reader, mut writer) = stream.into_split();
    let mut buf = BufReader::new(reader);
    let mut line = String::new();
    loop {
        line.clear();
        let n = buf.read_line(&mut line).await?;
        if n == 0 {
            return Ok(());
        }
        let trimmed = line.trim_end_matches(|c| c == '\n' || c == '\r');
        if trimmed.is_empty() {
            continue;
        }

        let resp_line = handle_request_line(trimmed, &chain).await;
        writer.write_all(resp_line.as_bytes()).await?;
        writer.write_all(b"\n").await?;
        writer.flush().await?;
    }
}

async fn handle_request_line(line: &str, chain: &Arc<ChainHandle>) -> String {
    let req: Request = match serde_json::from_str(line) {
        Ok(r) => r,
        Err(e) => {
            return error_response(&Value::Null, -32700, &format!("parse error: {e}"));
        }
    };

    match dispatch(&req, chain).await {
        Ok(result) => {
            let resp = ResponseEnvelope {
                jsonrpc: "2.0",
                id: &req.id,
                result: Some(result),
                error: None,
            };
            serde_json::to_string(&resp).unwrap_or_else(|e| format!(r#"{{"error":"{e}"}}"#))
        }
        Err((code, msg)) => error_response(&req.id, code, &msg),
    }
}

fn error_response(id: &Value, code: i32, message: &str) -> String {
    let resp = ResponseEnvelope {
        jsonrpc: "2.0",
        id,
        result: None,
        error: Some(RpcError { code, message: message.to_string() }),
    };
    serde_json::to_string(&resp).unwrap_or_else(|_| String::from(r#"{"error":"format"}"#))
}

async fn dispatch(req: &Request, chain: &Arc<ChainHandle>) -> Result<Value, (i32, String)> {
    match req.method.as_str() {
        "enable_container_mode" => {
            #[derive(Deserialize)]
            struct P {
                machine_id_hex: String,
                port_range_start: u16,
                port_range_end: u16,
            }
            let p: P = serde_json::from_value(req.params.clone())
                .map_err(|e| (-32602, format!("bad params: {e}")))?;
            let machine_id = parse_hex(&p.machine_id_hex)
                .map_err(|e| (-32602, format!("bad machine_id_hex: {e}")))?;
            chain
                .enable_container_mode(machine_id, p.port_range_start, p.port_range_end)
                .await
                .map(|o| serde_json::to_value(o).unwrap())
                .map_err(|e| (-32000, e.to_string()))
        }
        "remove_container_mode" => {
            #[derive(Deserialize)]
            struct P {
                machine_id_hex: String,
            }
            let p: P = serde_json::from_value(req.params.clone())
                .map_err(|e| (-32602, format!("bad params: {e}")))?;
            let machine_id = parse_hex(&p.machine_id_hex)
                .map_err(|e| (-32602, format!("bad machine_id_hex: {e}")))?;
            chain
                .remove_container_mode(machine_id)
                .await
                .map(|o| serde_json::to_value(o).unwrap())
                .map_err(|e| (-32000, e.to_string()))
        }
        "query_container_info" => {
            #[derive(Deserialize)]
            struct P {
                machine_id_hex: String,
            }
            let p: P = serde_json::from_value(req.params.clone())
                .map_err(|e| (-32602, format!("bad params: {e}")))?;
            let machine_id = parse_hex(&p.machine_id_hex)
                .map_err(|e| (-32602, format!("bad machine_id_hex: {e}")))?;
            chain
                .query_container_info(machine_id)
                .await
                .map(|opt| match opt {
                    Some(v) => serde_json::to_value(v).unwrap(),
                    None => json!(null),
                })
                .map_err(|e| (-32000, e.to_string()))
        }
        "ping" => Ok(json!({ "alive": true, "signer": chain.signer_ss58() })),
        _ => Err((-32601, format!("method not found: {}", req.method))),
    }
}

fn parse_hex(s: &str) -> Result<Vec<u8>, String> {
    let s = s.strip_prefix("0x").unwrap_or(s);
    hex::decode(s).map_err(|e| e.to_string())
}

/// Read SO_PEERCRED on a Linux Unix socket — returns the effective uid of the
/// process at the other end of the socket.
fn peer_uid(stream: &UnixStream) -> Result<u32> {
    #[repr(C)]
    struct Ucred {
        pid: i32,
        uid: u32,
        gid: u32,
    }
    let fd = stream.as_raw_fd();
    let mut cred = Ucred { pid: 0, uid: 0, gid: 0 };
    let mut len = std::mem::size_of::<Ucred>() as libc::socklen_t;
    let rc = unsafe {
        libc::getsockopt(
            fd,
            libc::SOL_SOCKET,
            libc::SO_PEERCRED,
            &mut cred as *mut _ as *mut libc::c_void,
            &mut len,
        )
    };
    if rc != 0 {
        return Err(anyhow::anyhow!(
            "getsockopt(SO_PEERCRED) failed: {}",
            std::io::Error::last_os_error()
        ));
    }
    Ok(cred.uid)
}
