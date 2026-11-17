//! Dữ liệu hồ sơ Vault mặc định cho kho dữ liệu mới khởi tạo.

use std::time::{SystemTime, UNIX_EPOCH};

use svm_domain::{VaultItem, VaultProfile};

/// Lấy dấu thời gian Unix Epoch hiện tại tính bằng mili-giây.
pub fn current_timestamp_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .unwrap_or(1_700_000_000_000)
}

/// Tạo hồ sơ Vault mặc định (`personal`) hoàn toàn trống.
pub fn build_default_vaults(primary_name: &str) -> Vec<VaultProfile> {
    let now = current_timestamp_ms();
    let name = if primary_name.trim().is_empty() {
        "Personal Vault"
    } else {
        primary_name.trim()
    };
    vec![VaultProfile {
        id: "personal".to_string(),
        name: name.to_string(),
        icon_name: "vault_personal".to_string(),
        description: "Primary personal credentials & logins".to_string(),
        item_count: 0,
        is_default: true,
        created_at: now,
    }]
}

/// Danh sách mục mẫu ban đầu (hoàn toàn trống).
pub fn build_default_sample_items() -> Vec<VaultItem> {
    Vec::new()
}
