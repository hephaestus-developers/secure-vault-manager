//! Trình xác thực ảo FIDO2 / WebAuthn (`WebAuthnAuthenticator`).

use svm_domain::{Category, JsonValue, VaultItem};

/// Trích xuất hostname từ URL và chuẩn hóa chữ thường.
fn extract_normalized_host(raw: &str) -> Option<String> {
    let trimmed = raw.trim().to_ascii_lowercase();
    if trimmed.is_empty() {
        return None;
    }
    let after_scheme = trimmed.split("://").nth(1).unwrap_or(&trimmed);
    let host_port = after_scheme
        .split(['/', '?', '#'])
        .next()
        .unwrap_or("")
        .trim_end_matches('.');
    let host = host_port.split(':').next().unwrap_or("");
    if host.is_empty() {
        None
    } else {
        Some(host.to_string())
    }
}

/// Kiểm tra xem một mục Passkey có khớp chuẩn Relying Party ID (`rp_id`) hay không.
/// Từ chối `rp_id` rỗng và chỉ khớp theo ranh giới tên miền đầy đủ (exact hoặc valid subdomain).
pub fn matches_passkey_rp_id(item: &VaultItem, rp_id: &str) -> bool {
    let rp = rp_id.trim().to_ascii_lowercase();
    let rp = rp.trim_end_matches('.');
    if rp.is_empty() {
        return false;
    }

    if let Some(cf) = item.custom_fields.iter().find(|cf| cf.label == "rpId") {
        let cf_rp = cf.value.trim().to_ascii_lowercase();
        let cf_rp = cf_rp.trim_end_matches('.');
        return !cf_rp.is_empty() && cf_rp == rp;
    }

    if let Some(host) = extract_normalized_host(&item.website_url) {
        return host == rp
            || host.ends_with(&format!(".{rp}"))
            || rp.ends_with(&format!(".{host}"));
    }

    false
}

/// Trích xuất `credentialId` (Base64URL) từ custom fields của một mục Passkey.
pub fn extract_passkey_credential_id(item: &VaultItem) -> String {
    item.custom_fields
        .iter()
        .find(|cf| cf.label == "credentialId")
        .map(|cf| cf.value.clone())
        .unwrap_or_default()
}

/// Kiểm tra xem `item` có `credentialId` nằm trong danh sách `allowCredentials` / `excludeCredentials` JSON hay không.
pub fn passkey_matches_credential_list(item: &VaultItem, creds: &[JsonValue]) -> bool {
    if creds.is_empty() {
        return false;
    }
    let cid = extract_passkey_credential_id(item);
    if cid.is_empty() {
        return false;
    }
    creds.iter().any(|entry| {
        if let Some(s) = entry.as_str() {
            s == cid
        } else if let Some(s) = entry.get_string("id") {
            s == cid
        } else {
            false
        }
    })
}

/// Đọc và tăng `signCount` cho mục Passkey (không ghi ra ngoài cho tới khi caller gọi `upsert_item`).
pub fn bump_passkey_sign_count(item: &mut VaultItem) -> u32 {
    if let Some(cf) = item
        .custom_fields
        .iter_mut()
        .find(|cf| cf.label == "signCount")
    {
        let current = cf.value.trim().parse::<u32>().unwrap_or(0);
        let next = current.saturating_add(1);
        cf.value = next.to_string();
        next
    } else {
        item.custom_fields.push(svm_domain::CustomField {
            id: svm_crypto::generate_uuid_v4(),
            label: "signCount".to_string(),
            value: "1".to_string(),
            kind: svm_domain::FieldKind::Text,
        });
        1
    }
}

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
                if !matches_passkey_rp_id(item, rp) {
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

#[cfg(test)]
mod tests {
    use super::*;
    use svm_domain::CustomField;

    #[test]
    fn test_matches_passkey_rp_id_boundaries() {
        let mut item = VaultItem {
            category: Category::Passkey,
            website_url: "https://login.example.com:8443/auth".to_string(),
            ..VaultItem::default()
        };

        assert!(!matches_passkey_rp_id(&item, ""));
        assert!(!matches_passkey_rp_id(&item, "   "));
        assert!(matches_passkey_rp_id(&item, "login.example.com"));
        assert!(matches_passkey_rp_id(&item, "example.com"));
        assert!(!matches_passkey_rp_id(&item, "evil-example.com"));
        assert!(!matches_passkey_rp_id(&item, "ple.com"));

        item.custom_fields.push(CustomField {
            id: "cf-1".to_string(),
            label: "rpId".to_string(),
            value: "auth.corp.internal".to_string(),
            kind: svm_domain::FieldKind::Text,
        });
        assert!(matches_passkey_rp_id(&item, "auth.corp.internal"));
        assert!(!matches_passkey_rp_id(&item, "example.com"));
    }
}
