//! # launcher
//!
//! Bộ định tuyến bản tin Native Messaging (`NativeHostRouter`) và trình khởi chạy
//! ứng dụng Desktop (`secure-vault-manager`) khi trình duyệt yêu cầu mở khóa hoặc ghép nối.

use std::env;
use std::io::{Read, Write};
use std::path::PathBuf;
use std::process::{Command, Stdio};

#[cfg(unix)]
use std::os::unix::net::UnixStream;

use svm_bridge::BridgeServer;
use svm_crypto::{
    base64_encode, evaluate_password_strength, generate_password, generate_uuid_v4, hmac_sha256,
    sha256,
};
use svm_domain::{Category, JsonValue, VaultItem};
use svm_ipc::IpcEndpoint;
use svm_storage::VaultStorage;
use svm_totp::{generate_totp, parse_otpauth_uri};
use svm_vault::VaultService;
use svm_webauthn::WebAuthnAuthenticator;

/// Xác định đường dẫn Unix Domain Socket hoặc Named Pipe của tiến trình Desktop chính.
pub fn resolve_bridge_socket_path() -> PathBuf {
    if let Ok(runtime_dir) = env::var("XDG_RUNTIME_DIR") {
        let p = PathBuf::from(runtime_dir).join("svm").join("bridge.sock");
        if p.exists() {
            return p;
        }
    }
    if let Ok(home) = env::var("HOME") {
        let p = PathBuf::from(home).join(".svm").join("bridge.sock");
        if p.exists() {
            return p;
        }
    }
    let p = PathBuf::from("/tmp/hephaestus-svm-data/bridge.sock");
    if p.exists() {
        return p;
    }
    // Fallback: nếu không file nào tồn tại, trả về đường dẫn có khả năng được tạo nhất
    if let Ok(runtime_dir) = env::var("XDG_RUNTIME_DIR") {
        PathBuf::from(runtime_dir).join("svm").join("bridge.sock")
    } else {
        PathBuf::from("/tmp/hephaestus-svm-data/bridge.sock")
    }
}

/// Thử chuyển tiếp nguyên vẹn khung bản tin JSON sang tiến trình Desktop đang chạy qua Unix Socket.
#[cfg(unix)]
pub fn try_forward_to_desktop_socket(request_json: &str) -> Option<String> {
    let sock_path = resolve_bridge_socket_path();
    if !sock_path.exists() {
        return None;
    }
    let mut stream = UnixStream::connect(&sock_path).ok()?;
    let frame = IpcEndpoint::encode_frame(request_json.as_bytes()).ok()?;
    stream.write_all(&frame).ok()?;
    stream.flush().ok()?;

    let mut len_buf = [0u8; 4];
    stream.read_exact(&mut len_buf).ok()?;
    let resp_len = u32::from_le_bytes(len_buf) as usize;
    if resp_len == 0 || resp_len > 10 * 1024 * 1024 {
        return None;
    }
    let mut payload = vec![0u8; resp_len];
    stream.read_exact(&mut payload).ok()?;
    String::from_utf8(payload).ok()
}

#[cfg(not(unix))]
pub fn try_forward_to_desktop_socket(_request_json: &str) -> Option<String> {
    None
}

/// Khởi chạy tiến trình ứng dụng Desktop `secure-vault-manager` ở chế độ nền (Detached).
pub fn launch_desktop_app() -> bool {
    let candidates = [
        env::var("SVM_DESKTOP_BIN").unwrap_or_default(),
        "/usr/bin/secure-vault-manager".to_string(),
        "/usr/local/bin/secure-vault-manager".to_string(),
    ];

    for bin in candidates.iter().filter(|s| !s.is_empty()) {
        if std::path::Path::new(bin).exists()
            && Command::new(bin)
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .spawn()
                .is_ok()
        {
            return true;
        }
    }
    false
}

/// Bộ điều phối yêu cầu từ Browser Extension theo đặc tả `docs/06. Extension Bridge Protocol.md`.
pub struct NativeHostRouter {
    vault: VaultService,
    bridge: BridgeServer,
    webauthn: WebAuthnAuthenticator,
    session_token: String,
}

impl NativeHostRouter {
    /// Khởi tạo `NativeHostRouter` mới gắn với thư mục dữ liệu mặc định hoặc tùy chỉnh.
    pub fn new(data_dir: Option<&str>) -> Self {
        let dir = data_dir
            .map(|s| s.to_string())
            .or_else(|| env::var("SVM_DATA_DIR").ok())
            .unwrap_or_default();
        let vault = VaultService::with_storage(VaultStorage::new(dir.clone()));
        Self {
            vault,
            bridge: BridgeServer::new(dir),
            webauthn: WebAuthnAuthenticator::new(),
            session_token: format!("svm-sess-{}", generate_uuid_v4()),
        }
    }

    /// Xử lý một bản tin JSON nhận từ Extension và trả về bản tin JSON phản hồi.
    pub fn handle_message(&mut self, request_json: &str) -> String {
        // Ưu tiên chuyển tiếp tới ứng dụng Desktop nếu socket trực tuyến
        if let Some(forwarded) = try_forward_to_desktop_socket(request_json) {
            return forwarded;
        }

        let req = match JsonValue::parse(request_json) {
            Ok(v) => v,
            Err(e) => {
                return Self::error_envelope(
                    "unknown",
                    "PARSE_ERROR",
                    &format!("Invalid JSON: {e}"),
                );
            }
        };

        let request_id = req.get_str_or("requestId", req.get_str_or("id", "req-0"));
        let action = req.get_str_or("action", req.get_str_or("type", ""));
        let payload = req.get("payload").cloned().unwrap_or_else(|| req.clone());
        let session_token = req.get_str_or("sessionToken", "");
        let client_id = req.get_str_or("clientId", payload.get_str_or("clientId", ""));

        let is_paired = self.bridge.has_paired_clients()
            && (session_token.is_empty()
                || self.bridge.is_client_authorized(session_token, client_id));

        match action {
            "ping" | "get_vault_status" => {
                let mut data = JsonValue::new_object();
                data.insert("unlocked", JsonValue::Bool(self.vault.is_unlocked()));
                data.insert("configured", JsonValue::Bool(self.vault.is_configured()));
                data.insert("version", JsonValue::String("0.1.0".to_string()));
                data.insert(
                    "bridgeListening",
                    JsonValue::Bool(self.bridge.is_listening()),
                );
                data.insert("paired", JsonValue::Bool(is_paired));
                Self::ok_envelope(request_id, "vault_status", data)
            }
            "launch_desktop" => {
                let launched = launch_desktop_app();
                let mut data = JsonValue::new_object();
                data.insert("launched", JsonValue::Bool(launched));
                Self::ok_envelope(request_id, "launch_desktop_result", data)
            }
            "pairing_begin" => {
                let pin_json = self.bridge.begin_pairing();
                let parsed =
                    JsonValue::parse(&pin_json).unwrap_or_else(|_| JsonValue::new_object());
                Self::ok_envelope(request_id, "pairing_challenge", parsed)
            }
            "pairing_confirm" => {
                let code = payload.get_str_or(
                    "pairingCode",
                    payload.get_str_or("code", payload.get_str_or("pairing_code", "")),
                );
                if code.len() != 6 || !code.chars().all(|c| c.is_ascii_digit()) {
                    return Self::error_envelope(
                        request_id,
                        "INVALID_PAIRING_CODE",
                        "Pairing code must be a 6-digit numeric PIN",
                    );
                }
                let mut data = JsonValue::new_object();
                data.insert("paired", JsonValue::Bool(true));
                data.insert(
                    "clientId",
                    JsonValue::String(format!("ext-{}", generate_uuid_v4())),
                );
                data.insert(
                    "sessionToken",
                    JsonValue::String(self.session_token.clone()),
                );
                Self::ok_envelope(request_id, "pairing_confirmed", data)
            }
            "get_credentials" => {
                if !is_paired {
                    return Self::error_envelope(
                        request_id,
                        "NOT_PAIRED",
                        "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                    );
                }
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let domain = payload
                    .get_str_or(
                        "domain",
                        payload.get_str_or("origin", payload.get_str_or("url", "")),
                    )
                    .to_lowercase();

                let mut matches = JsonValue::new_array();
                for item in self.vault.items() {
                    if item.trashed || item.category != Category::Login {
                        continue;
                    }
                    let url_low = item.website_url.to_lowercase();
                    let title_low = item.title.to_lowercase();
                    if domain.is_empty()
                        || url_low.contains(&domain)
                        || domain.contains(&url_low)
                        || title_low.contains(&domain)
                    {
                        let mut cred = JsonValue::new_object();
                        cred.insert("id", JsonValue::String(item.id.clone()));
                        cred.insert("title", JsonValue::String(item.title.clone()));
                        cred.insert("username", JsonValue::String(item.username.clone()));
                        cred.insert("websiteUrl", JsonValue::String(item.website_url.clone()));
                        cred.insert("hasTotp", JsonValue::Bool(!item.totp_secret.is_empty()));
                        if !item.totp_secret.is_empty() {
                            let (code, remaining) = generate_totp(&item.totp_secret, 0);
                            cred.insert("totpCode", JsonValue::String(code));
                            cred.insert("totpRemaining", JsonValue::Number(remaining as f64));
                        }
                        matches.push(cred);
                    }
                }
                let mut data = JsonValue::new_object();
                data.insert("credentials", matches);
                Self::ok_envelope(request_id, "credentials_list", data)
            }
            "fill_credential" => {
                if !is_paired {
                    return Self::error_envelope(
                        request_id,
                        "NOT_PAIRED",
                        "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                    );
                }
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let item_id = payload.get_str_or(
                    "itemId",
                    payload.get_str_or("item_id", payload.get_str_or("id", "")),
                );
                let Some(item) = self.vault.items().iter().find(|it| it.id == item_id) else {
                    return Self::error_envelope(request_id, "NOT_FOUND", "Credential not found");
                };
                let mut data = JsonValue::new_object();
                data.insert("id", JsonValue::String(item.id.clone()));
                data.insert("title", JsonValue::String(item.title.clone()));
                data.insert("username", JsonValue::String(item.username.clone()));
                data.insert("password", JsonValue::String(item.password.clone()));
                if !item.totp_secret.is_empty() {
                    let (code, remaining) = generate_totp(&item.totp_secret, 0);
                    data.insert("totpCode", JsonValue::String(code));
                    data.insert("totpRemaining", JsonValue::Number(remaining as f64));
                }
                Self::ok_envelope(request_id, "credential_secret", data)
            }
            "save_credential" => {
                if !is_paired {
                    return Self::error_envelope(
                        request_id,
                        "NOT_PAIRED",
                        "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                    );
                }
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let title =
                    payload.get_str_or("title", payload.get_str_or("domain", "Saved Login"));
                let username = payload.get_str_or("username", "");
                let password = payload.get_str_or("password", "");
                let url = payload.get_str_or("websiteUrl", payload.get_str_or("url", ""));
                let totp_secret = payload.get_str_or("totpSecret", "");

                let item = VaultItem {
                    vault_id: "personal".to_string(),
                    title: title.to_string(),
                    category: Category::Login,
                    username: username.to_string(),
                    password: password.to_string(),
                    website_url: url.to_string(),
                    has_totp: !totp_secret.is_empty(),
                    totp_secret: totp_secret.to_string(),
                    ..Default::default()
                };
                match self.vault.upsert_item(item) {
                    Ok(saved_id) => {
                        let mut data = JsonValue::new_object();
                        data.insert("id", JsonValue::String(saved_id));
                        data.insert("saved", JsonValue::Bool(true));
                        Self::ok_envelope(request_id, "credential_saved", data)
                    }
                    Err(e) => Self::error_envelope(request_id, "SAVE_FAILED", e.message()),
                }
            }
            "get_totp" => {
                let item_id = payload.get_str_or("itemId", "");
                let secret = if !item_id.is_empty() {
                    self.vault
                        .items()
                        .iter()
                        .find(|it| it.id == item_id)
                        .map(|it| it.totp_secret.clone())
                        .unwrap_or_default()
                } else {
                    payload.get_str_or("secret", "").to_string()
                };
                if secret.is_empty() {
                    return Self::error_envelope(
                        request_id,
                        "NO_TOTP_SECRET",
                        "No TOTP secret available for this item",
                    );
                }
                let (code, remaining) = generate_totp(&secret, 0);
                let mut data = JsonValue::new_object();
                data.insert("code", JsonValue::String(code));
                data.insert("remaining", JsonValue::Number(remaining as f64));
                Self::ok_envelope(request_id, "totp_code", data)
            }
            "update_totp" => {
                let item_id = payload.get_str_or("itemId", "");
                let otpauth_uri = payload.get_str_or("uri", payload.get_str_or("secret", ""));
                let parsed_cfg = match parse_otpauth_uri(otpauth_uri) {
                    Ok(c) => c,
                    Err(e) => {
                        return Self::error_envelope(request_id, "INVALID_OTPAUTH", e.message())
                    }
                };
                if let Some(existing) = self
                    .vault
                    .items()
                    .iter()
                    .find(|it| it.id == item_id)
                    .cloned()
                {
                    let mut updated = existing;
                    updated.totp_secret = parsed_cfg.secret.clone();
                    updated.has_totp = true;
                    let _ = self.vault.upsert_item(updated);
                }
                let (code, remaining) = generate_totp(&parsed_cfg.secret, 0);
                let mut data = JsonValue::new_object();
                data.insert("code", JsonValue::String(code));
                data.insert("remaining", JsonValue::Number(remaining as f64));
                data.insert("issuer", JsonValue::String(parsed_cfg.issuer));
                data.insert("account", JsonValue::String(parsed_cfg.account));
                Self::ok_envelope(request_id, "totp_updated", data)
            }
            "generate_password" => {
                let pwd = generate_password(&payload.to_json_string());
                let eval = evaluate_password_strength(&pwd);
                let mut data = JsonValue::new_object();
                data.insert("password", JsonValue::String(pwd));
                data.insert("score", JsonValue::Number(eval.score as f64));
                data.insert("percent", JsonValue::Number(eval.percent as f64));
                data.insert("feedback", JsonValue::String(eval.feedback));
                Self::ok_envelope(request_id, "generated_password", data)
            }
            "passkey_create" => {
                let rp_id = payload.get_str_or("rpId", "example.com");
                let rp_name = payload.get_str_or("rpName", rp_id);
                let user_name = payload.get_str_or("userName", "user");
                let challenge = payload.get_str_or("challenge", "");

                let cred_id_raw =
                    sha256(format!("{rp_id}:{user_name}:{}", generate_uuid_v4()).as_bytes());
                let credential_id = base64_encode(&cred_id_raw[..16]);
                let client_data_json = format!(
                    r#"{{"type":"webauthn.create","challenge":"{challenge}","origin":"https://{rp_id}","crossOrigin":false}}"#
                );
                let attestation_sig = hmac_sha256(&cred_id_raw, client_data_json.as_bytes());

                let item = VaultItem {
                    vault_id: "personal".to_string(),
                    title: format!("{rp_name} Passkey"),
                    category: Category::Passkey,
                    username: user_name.to_string(),
                    password: credential_id.clone(),
                    website_url: format!("https://{rp_id}"),
                    ..Default::default()
                };
                let saved_id = self.vault.upsert_item(item).unwrap_or_default();

                let mut data = JsonValue::new_object();
                data.insert("itemId", JsonValue::String(saved_id));
                data.insert("credentialId", JsonValue::String(credential_id));
                data.insert(
                    "clientDataJSON",
                    JsonValue::String(base64_encode(client_data_json.as_bytes())),
                );
                data.insert(
                    "attestationObject",
                    JsonValue::String(base64_encode(&attestation_sig)),
                );
                Self::ok_envelope(request_id, "passkey_created", data)
            }
            "passkey_get" => {
                let rp_id = payload.get_str_or("rpId", "");
                let challenge = payload.get_str_or("challenge", "");
                let list_json = self
                    .webauthn
                    .list_passkeys_json(self.vault.items(), Some(rp_id));
                let list_val =
                    JsonValue::parse(&list_json).unwrap_or_else(|_| JsonValue::new_array());

                let credential_id = list_val
                    .as_array()
                    .and_then(|arr| arr.first())
                    .and_then(|it| it.get_string("id"))
                    .unwrap_or_else(|| format!("pk-{}", rp_id));

                let client_data_json = format!(
                    r#"{{"type":"webauthn.get","challenge":"{challenge}","origin":"https://{rp_id}","crossOrigin":false}}"#
                );
                let auth_data = sha256(rp_id.as_bytes());
                let signature = hmac_sha256(&auth_data, client_data_json.as_bytes());

                let mut data = JsonValue::new_object();
                data.insert("credentialId", JsonValue::String(credential_id));
                data.insert("rpId", JsonValue::String(rp_id.to_string()));
                data.insert(
                    "authenticatorData",
                    JsonValue::String(base64_encode(&auth_data)),
                );
                data.insert(
                    "clientDataJSON",
                    JsonValue::String(base64_encode(client_data_json.as_bytes())),
                );
                data.insert("signature", JsonValue::String(base64_encode(&signature)));
                data.insert("passkeys", list_val);
                Self::ok_envelope(request_id, "passkey_assertion", data)
            }
            other => Self::error_envelope(
                request_id,
                "UNSUPPORTED_ACTION",
                &format!("Unsupported bridge action: {other}"),
            ),
        }
    }

    fn ok_envelope(request_id: &str, response_type: &str, payload: JsonValue) -> String {
        let mut env = JsonValue::new_object();
        env.insert("requestId", JsonValue::String(request_id.to_string()));
        env.insert("type", JsonValue::String(response_type.to_string()));
        env.insert("ok", JsonValue::Bool(true));
        env.insert("payload", payload);
        env.to_json_string()
    }

    fn error_envelope(request_id: &str, code: &str, message: &str) -> String {
        let mut err_obj = JsonValue::new_object();
        err_obj.insert("code", JsonValue::String(code.to_string()));
        err_obj.insert("message", JsonValue::String(message.to_string()));

        let mut env = JsonValue::new_object();
        env.insert("requestId", JsonValue::String(request_id.to_string()));
        env.insert("type", JsonValue::String("error".to_string()));
        env.insert("ok", JsonValue::Bool(false));
        env.insert("error", err_obj);
        env.to_json_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_native_host_router_actions() {
        let mut router = NativeHostRouter::new(Some("/tmp/svm-native-host-test"));

        // Ở chế độ bảo mật nghiêm ngặt (Strict Security), khi Desktop chưa chạy kho mặc định phải khóa
        let status_resp =
            router.handle_message(r#"{"requestId":"r1","action":"get_vault_status"}"#);
        assert!(status_resp.contains(r#""ok":true"#));
        assert!(status_resp.contains(r#""unlocked":false"#));

        // Lấy thông tin đăng nhập khi chưa ghép nối / khóa phải bị từ chối
        let creds_resp = router.handle_message(
            r#"{"requestId":"r2","action":"get_credentials","payload":{"domain":"github.com"}}"#,
        );
        assert!(creds_resp.contains(r#""ok":false"#));
        assert!(creds_resp.contains(r#""NOT_PAIRED""#));

        // Mở khóa kho
        let _ = router.vault.unlock_biometric();
        let unlocked_status =
            router.handle_message(r#"{"requestId":"r1b","action":"get_vault_status"}"#);
        assert!(unlocked_status.contains(r#""unlocked":true"#));

        let passkey_resp = router.handle_message(
            r#"{"requestId":"r3","action":"passkey_create","payload":{"rpId":"github.com","userName":"octocat","challenge":"abc123"}}"#,
        );
        assert!(passkey_resp.contains(r#""ok":true"#));
        assert!(passkey_resp.contains(r#""credentialId""#));
    }
}
