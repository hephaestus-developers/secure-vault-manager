//! Trình xác thực ảo FIDO2 / WebAuthn (`WebAuthnAuthenticator`).

use svm_domain::{Category, JsonValue, VaultItem};

use crate::fido2::{authenticate_passkey_assertion, PasskeyAssertion, PasskeyRegistration};

/// Mã hóa lát cắt byte sang chuỗi Base64URL không padding (`-`, `_`).
pub fn encode_base64url(bytes: &[u8]) -> String {
    svm_crypto::base64_encode(bytes)
        .replace('+', "-")
        .replace('/', "_")
        .trim_end_matches('=')
        .to_string()
}

/// Giải mã chuỗi Base64URL (có hoặc không có padding) sang `Vec<u8>`.
pub fn decode_base64url(input: &str) -> Vec<u8> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return Vec::new();
    }
    let mut standard = trimmed.replace('-', "+").replace('_', "/");
    while !standard.len().is_multiple_of(4) {
        standard.push('=');
    }
    svm_crypto::base64_decode(&standard).unwrap_or_default()
}

/// Siêu dữ liệu cần thiết để khởi tạo `VaultItem` thuộc danh mục `Category::Passkey`.
pub struct PasskeyItemParams<'a> {
    pub vault_id: &'a str,
    pub rp_id: &'a str,
    pub rp_name: &'a str,
    pub user_name: &'a str,
    pub user_display: &'a str,
    pub origin: &'a str,
    pub user_handle_b64: &'a str,
}

/// Tạo thực thể `VaultItem` chuẩn hóa với đầy đủ trường Passkey chuyên biệt.
pub fn build_passkey_item(params: &PasskeyItemParams<'_>, reg: &PasskeyRegistration) -> VaultItem {
    let cred_id_b64 = encode_base64url(&reg.credential_id);
    let rp_label = if params.rp_name.trim().is_empty() {
        params.rp_id
    } else {
        params.rp_name.trim()
    };
    VaultItem {
        vault_id: params.vault_id.to_string(),
        title: format!("{rp_label} Passkey"),
        category: Category::Passkey,
        username: params.user_name.to_string(),
        website_url: format!("https://{}", params.rp_id),
        notes: format!("Passkey for {} on {}", params.user_display, params.origin),
        has_passkey: true,
        password: svm_crypto::base64_encode(&reg.private_key_pkcs8),
        passkey_credential_id: cred_id_b64,
        passkey_rp_id: params.rp_id.trim().to_string(),
        passkey_algorithm: reg.algorithm,
        passkey_user_handle: params.user_handle_b64.trim().to_string(),
        passkey_sign_count: 0,
        ..Default::default()
    }
}

/// Thực thi ký xác thực WebAuthn cho `item` và tự động tăng `passkey_sign_count`.
pub fn execute_passkey_assertion(
    item: &mut VaultItem,
    rp_id: &str,
    challenge: &str,
    origin: &str,
) -> Result<PasskeyAssertion, String> {
    let priv_bytes = svm_crypto::base64_decode(&item.password).unwrap_or_default();
    if priv_bytes.is_empty() {
        return Err("Khóa riêng tư của Passkey không hợp lệ".to_string());
    }
    let cred_id_bytes = decode_base64url(&item.passkey_credential_id);
    let user_handle_bytes = if item.passkey_user_handle.trim().is_empty() {
        None
    } else {
        Some(decode_base64url(&item.passkey_user_handle))
    };
    let next_sign_count = bump_passkey_sign_count(item);

    authenticate_passkey_assertion(
        rp_id,
        challenge,
        origin,
        &cred_id_bytes,
        &priv_bytes,
        next_sign_count,
        user_handle_bytes,
    )
}

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

    if !item.passkey_rp_id.trim().is_empty() {
        let item_rp = item.passkey_rp_id.trim().to_ascii_lowercase();
        let item_rp = item_rp.trim_end_matches('.');
        return !item_rp.is_empty() && item_rp == rp;
    }

    if let Some(host) = extract_normalized_host(&item.website_url) {
        return host == rp
            || host.ends_with(&format!(".{rp}"))
            || rp.ends_with(&format!(".{host}"));
    }

    false
}

/// Trích xuất `passkey_credential_id` (Base64URL) của một mục Passkey.
pub fn extract_passkey_credential_id(item: &VaultItem) -> String {
    item.passkey_credential_id.clone()
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

/// Đọc và tăng `passkey_sign_count` cho mục Passkey (không ghi ra ngoài cho tới khi caller gọi `upsert_item`).
pub fn bump_passkey_sign_count(item: &mut VaultItem) -> u32 {
    item.passkey_sign_count = item.passkey_sign_count.saturating_add(1);
    item.passkey_sign_count
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

/// Truy vấn danh sách Passkey khớp `rpId` và `allowCredentials` cho Bridge/Extension `passkey_query`.
pub fn query_passkeys_payload(
    unlocked: bool,
    items: &[VaultItem],
    payload: &JsonValue,
) -> JsonValue {
    let mut data = JsonValue::new_object();
    if !unlocked {
        data.insert("unlocked", JsonValue::Bool(false));
        data.insert("passkeys", JsonValue::Array(Vec::new()));
        return data;
    }

    let rp_id = payload.get_str_or("rpId", "");
    let allow_creds = payload
        .get("allowCredentials")
        .and_then(|v| v.as_array())
        .map(|a| a.to_vec())
        .unwrap_or_default();

    let matched: Vec<JsonValue> = items
        .iter()
        .filter(|it| {
            !it.trashed
                && it.category == Category::Passkey
                && matches_passkey_rp_id(it, rp_id)
                && (allow_creds.is_empty() || passkey_matches_credential_list(it, &allow_creds))
        })
        .map(|it| {
            let mut obj = JsonValue::new_object();
            obj.insert("id", JsonValue::String(it.id.clone()));
            obj.insert("title", JsonValue::String(it.title.clone()));
            obj.insert("username", JsonValue::String(it.username.clone()));
            obj.insert(
                "credentialId",
                JsonValue::String(extract_passkey_credential_id(it)),
            );
            obj
        })
        .collect();

    data.insert("unlocked", JsonValue::Bool(true));
    data.insert("passkeys", JsonValue::Array(matched));
    data
}

impl Default for WebAuthnAuthenticator {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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

        item.passkey_rp_id = "auth.corp.internal".to_string();
        assert!(matches_passkey_rp_id(&item, "auth.corp.internal"));
        assert!(!matches_passkey_rp_id(&item, "example.com"));
    }

    #[test]
    fn test_build_passkey_item_and_sign_count() {
        let reg = crate::fido2::create_passkey_credential(
            "webauthn.io",
            "dGVzdGNoYWxsZW5nZQ",
            "https://webauthn.io",
        )
        .expect("create passkey");
        let params = PasskeyItemParams {
            vault_id: "personal",
            rp_id: "webauthn.io",
            rp_name: "WebAuthn.io",
            user_name: "alice@example.com",
            user_display: "Alice",
            origin: "https://webauthn.io",
            user_handle_b64: "YWxpY2U",
        };
        let mut item = build_passkey_item(&params, &reg);
        assert_eq!(item.passkey_rp_id, "webauthn.io");
        assert_eq!(item.passkey_algorithm, -7);
        assert_eq!(item.passkey_user_handle, "YWxpY2U");
        assert_eq!(item.passkey_sign_count, 0);
        assert!(!item.passkey_credential_id.is_empty());
        assert!(item.custom_fields.is_empty());
        assert_eq!(bump_passkey_sign_count(&mut item), 1);
        assert_eq!(item.passkey_sign_count, 1);
    }
}
