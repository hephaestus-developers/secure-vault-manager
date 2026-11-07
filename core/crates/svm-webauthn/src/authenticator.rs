//! Trình xác thực ảo FIDO2 / WebAuthn (`WebAuthnAuthenticator`).

use svm_domain::{Category, JsonValue, VaultItem};

/// Bộ xử lý trình xác thực ảo FIDO2 / WebAuthn.
pub struct WebAuthnAuthenticator;

impl WebAuthnAuthenticator {
    /// Khởi tạo một thực thể `WebAuthnAuthenticator` mới.
    pub fn new() -> Self {
        Self
    }

    /// Lọc danh sách các mục thuộc danh mục `Category::Passkey` theo tên miền `rp_id`.
    pub fn list_passkeys_json(&self, items: &[VaultItem], rp_id: Option<&str>) -> String {
        let filter_rp = rp_id
            .map(|s| s.trim().to_lowercase())
            .filter(|s| !s.is_empty());

        let mut arr = JsonValue::new_array();
        for item in items {
            if item.trashed || item.category != Category::Passkey {
                continue;
            }
            if let Some(ref rp) = filter_rp {
                let url_low = item.website_url.to_lowercase();
                let title_low = item.title.to_lowercase();
                if !url_low.contains(rp) && !title_low.contains(rp) {
                    continue;
                }
            }
            arr.push(item.to_json());
        }
        arr.to_json_string()
    }
}

impl Default for WebAuthnAuthenticator {
    fn default() -> Self {
        Self::new()
    }
}
