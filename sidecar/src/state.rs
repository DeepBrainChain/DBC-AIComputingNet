//! Local state persistence (sqlite). Currently tracks the substrate nonce so
//! we don't double-spend across restarts.

use anyhow::Result;
use rusqlite::Connection;
use std::path::Path;
use std::sync::{Arc, Mutex};

pub struct State {
    conn: Mutex<Connection>,
}

pub fn open(path: &Path) -> Result<Arc<State>> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let conn = Connection::open(path)?;
    conn.execute_batch(
        r#"
        CREATE TABLE IF NOT EXISTS nonce (
            account TEXT PRIMARY KEY,
            value INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS last_seen (
            key TEXT PRIMARY KEY,
            value INTEGER NOT NULL
        );
        "#,
    )?;
    Ok(Arc::new(State { conn: Mutex::new(conn) }))
}

impl State {
    pub fn get_nonce(&self, account: &str) -> Result<Option<u64>> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare("SELECT value FROM nonce WHERE account = ?1")?;
        let mut rows = stmt.query([account])?;
        if let Some(row) = rows.next()? {
            Ok(Some(row.get::<_, i64>(0)? as u64))
        } else {
            Ok(None)
        }
    }

    pub fn set_nonce(&self, account: &str, value: u64) -> Result<()> {
        let conn = self.conn.lock().unwrap();
        conn.execute(
            "INSERT OR REPLACE INTO nonce(account, value) VALUES (?1, ?2)",
            (account, value as i64),
        )?;
        Ok(())
    }
}
