//! # launcher
//!
//! Bộ định tuyến bản tin Native Messaging (`NativeHostRouter`) và trình khởi chạy
//! ứng dụng Desktop (`secure-vault-manager`) khi trình duyệt yêu cầu mở khóa hoặc đồng bộ.

use std::env;
use std::io::{Read, Write};
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::time::Duration;

#[cfg(unix)]
use std::os::unix::net::UnixStream;

use svm_crypto::{evaluate_password_strength, generate_password};
use svm_domain::{Category, JsonValue, VaultItem};
use svm_ipc::IpcEndpoint;
use svm_storage::VaultStorage;
use svm_totp::generate_totp;
use svm_vault::VaultService;
use svm_webauthn::WebAuthnAuthenticator;

/// Danh sách các đường dẫn Unix Domain Socket ứng viên của tiến trình Desktop chính.
pub fn candidate_bridge_socket_paths() -> Vec<PathBuf> {
    let mut paths = Vec::new();
    if let Ok(runtime_dir) = env::var("XDG_RUNTIME_DIR") {
        paths.push(PathBuf::from(runtime_dir).join("svm").join("bridge.sock"));
    }
    if let Ok(home) = env::var("HOME") {
        paths.push(PathBuf::from(home).join(".svm").join("bridge.sock"));
    }
    #[cfg(unix)]
    {
        let user_suffix = env::var("USER")
            .or_else(|_| env::var("LOGNAME"))
            .unwrap_or_else(|_| "user".to_string());
        paths.push(PathBuf::from(format!(
            "/tmp/hephaestus-svm-data-{user_suffix}/bridge.sock"
        )));
    }
    #[cfg(not(unix))]
    {
        paths.push(PathBuf::from("/tmp/hephaestus-svm-data/bridge.sock"));
    }
    paths
}

/// Xác định đường dẫn Unix Domain Socket hoặc Named Pipe của tiến trình Desktop chính.
pub fn resolve_bridge_socket_path() -> PathBuf {
    let candidates = candidate_bridge_socket_paths();
    for p in &candidates {
        if p.exists() {
            return p.clone();
        }
    }
    candidates.into_iter().next().unwrap_or_else(|| {
        #[cfg(unix)]
        {
            let user_suffix = env::var("USER")
                .or_else(|_| env::var("LOGNAME"))
                .unwrap_or_else(|_| "user".to_string());
            PathBuf::from(format!(
                "/tmp/hephaestus-svm-data-{user_suffix}/bridge.sock"
            ))
        }
        #[cfg(not(unix))]
        {
            PathBuf::from("/tmp/hephaestus-svm-data/bridge.sock")
        }
    })
}

/// Thử chuyển tiếp nguyên vẹn khung bản tin JSON sang tiến trình Desktop qua Unix Socket hoặc Named Pipe.
pub fn try_forward_to_desktop_socket(request_json: &str) -> Option<String> {
    let frame = IpcEndpoint::encode_frame(request_json.as_bytes()).ok()?;

    #[cfg(unix)]
    {
        for sock_path in candidate_bridge_socket_paths() {
            if !sock_path.exists() {
                continue;
            }
            let Ok(mut stream) = UnixStream::connect(&sock_path) else {
                continue;
            };
            if let Some(resp) = roundtrip_ipc_frame(&mut stream, &frame) {
                return Some(resp);
            }
        }
        None
    }

    #[cfg(windows)]
    {
        let pipe_name = svm_bridge::server::resolve_windows_pipe_name();
        let mut pipe = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(pipe_name)
            .ok()?;
        roundtrip_ipc_frame(&mut pipe, &frame)
    }

    #[cfg(not(any(unix, windows)))]
    {
        let _ = frame;
        None
    }
}

fn roundtrip_ipc_frame<S: Read + Write>(stream: &mut S, frame: &[u8]) -> Option<String> {
    if stream.write_all(frame).is_err() || stream.flush().is_err() {
        return None;
    }
    let mut len_buf = [0u8; 4];
    if stream.read_exact(&mut len_buf).is_err() {
        return None;
    }
    let resp_len = u32::from_le_bytes(len_buf) as usize;
    if resp_len == 0 || resp_len > 10 * 1024 * 1024 {
        return None;
    }
    let mut payload = vec![0u8; resp_len];
    if stream.read_exact(&mut payload).is_err() {
        return None;
    }
    String::from_utf8(payload).ok()
}

/// Khởi chạy tiến trình ứng dụng Desktop `secure-vault-manager` / `svm_ui` ở chế độ nền (Detached).
pub fn launch_desktop_app() -> bool {
    let ui_bin = if cfg!(windows) {
        "svm_ui.exe"
    } else {
        "svm_ui"
    };
    let app_bin = if cfg!(windows) {
        "secure-vault-manager.exe"
    } else {
        "secure-vault-manager"
    };

    let mut candidates = vec![env::var("SVM_DESKTOP_BIN").unwrap_or_default()];

    #[cfg(not(windows))]
    {
        candidates.push("/usr/bin/svm_ui".to_string());
        candidates.push("/usr/bin/secure-vault-manager".to_string());
        candidates.push("/usr/local/bin/svm_ui".to_string());
        candidates.push("/usr/local/bin/secure-vault-manager".to_string());
    }
    #[cfg(target_os = "macos")]
    {
        candidates.push("/Applications/Secure Vault Manager.app/Contents/MacOS/svm_ui".to_string());
        candidates.push(
            "/Applications/Secure Vault Manager.app/Contents/MacOS/secure-vault-manager"
                .to_string(),
        );
    }
    #[cfg(windows)]
    {
        if let Ok(local_app) = env::var("LOCALAPPDATA") {
            candidates.push(format!(
                "{local_app}\\Hephaestus\\SecureVaultManager\\{ui_bin}"
            ));
            candidates.push(format!(
                "{local_app}\\Hephaestus\\SecureVaultManager\\{app_bin}"
            ));
        }
    }

    if let Ok(exe) = env::current_exe() {
        if let Some(parent) = exe.parent() {
            candidates.push(parent.join(ui_bin).to_string_lossy().to_string());
            candidates.push(parent.join(app_bin).to_string_lossy().to_string());
            if let Some(grandparent) = parent.parent() {
                candidates.push(
                    grandparent
                        .join("ui")
                        .join(ui_bin)
                        .to_string_lossy()
                        .to_string(),
                );
                candidates.push(
                    grandparent
                        .join("ui")
                        .join(app_bin)
                        .to_string_lossy()
                        .to_string(),
                );
                if let Some(great) = grandparent.parent() {
                    candidates.push(
                        great
                            .join("build")
                            .join("debug")
                            .join("ui")
                            .join(ui_bin)
                            .to_string_lossy()
                            .to_string(),
                    );
                    candidates.push(
                        great
                            .join("build")
                            .join("ui")
                            .join(ui_bin)
                            .to_string_lossy()
                            .to_string(),
                    );
                }
            }
        }
    }

    // Dự phòng đường dẫn tương đối theo thư mục làm việc hiện tại (khi dev cục bộ)
    if let Ok(cwd) = env::current_dir() {
        candidates.push(
            cwd.join("build")
                .join("debug")
                .join("ui")
                .join(ui_bin)
                .to_string_lossy()
                .to_string(),
        );
    }

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
    #[allow(dead_code)]
    webauthn: WebAuthnAuthenticator,
}

impl NativeHostRouter {
    /// Khởi tạo `NativeHostRouter` mới gắn với thư mục dữ liệu mặc định hoặc tùy chỉnh.
    pub fn new(data_dir: Option<&str>) -> Self {
        let dir = data_dir
            .map(|s| s.to_string())
            .or_else(|| env::var("SVM_DATA_DIR").ok())
            .unwrap_or_default();
        let vault = VaultService::with_storage(VaultStorage::new(dir));
        Self {
            vault,
            webauthn: WebAuthnAuthenticator::new(),
        }
    }

    /// Xử lý một bản tin JSON nhận từ Extension và trả về bản tin JSON phản hồi.
    pub fn handle_message(&mut self, request_json: &str) -> String {
        // Ưu tiên chuyển tiếp tới ứng dụng Desktop nếu socket trực tuyến (Desktop là Single Source of Truth)
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

        match action {
            "ping" | "get_vault_status" => {
                let mut data = JsonValue::new_object();
                // Khi tiến trình Desktop App chưa chạy hoặc chưa kết nối socket,
                // trạng thái mở khóa bắt buộc phải là false để đảm bảo Desktop là Single Source of Truth.
                data.insert("unlocked", JsonValue::Bool(false));
                data.insert("desktopRunning", JsonValue::Bool(false));
                data.insert("configured", JsonValue::Bool(self.vault.is_configured()));
                data.insert("version", JsonValue::String("0.1.0".to_string()));
                data.insert("bridgeListening", JsonValue::Bool(false));
                data.insert("biometricAvailable", JsonValue::Bool(false));
                Self::ok_envelope(request_id, "vault_status", data)
            }
            "unlock_vault" | "unlock_vault_biometric" => {
                // Tự động khởi chạy Desktop App ngầm khi người dùng thực hiện mở khóa
                if launch_desktop_app() {
                    for _ in 0..10 {
                        std::thread::sleep(Duration::from_millis(150));
                        if let Some(forwarded) = try_forward_to_desktop_socket(request_json) {
                            return forwarded;
                        }
                    }
                }
                Self::error_envelope(
                    request_id,
                    "DESKTOP_NOT_RUNNING",
                    "Ứng dụng Desktop chưa khởi động. Vui lòng mở ứng dụng Secure Vault Manager trên máy tính.",
                )
            }
            "lock_vault" => {
                self.vault.lock();
                let mut data = JsonValue::new_object();
                data.insert("unlocked", JsonValue::Bool(false));
                Self::ok_envelope(request_id, "vault_locked", data)
            }
            "launch_desktop" => {
                let launched = launch_desktop_app();
                let mut data = JsonValue::new_object();
                data.insert("launched", JsonValue::Bool(launched));
                Self::ok_envelope(request_id, "launch_desktop_result", data)
            }
            "get_credentials" => {
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
                    let matches_domain = if domain.is_empty() {
                        false
                    } else if svm_webauthn::is_matching_login_domain(&item.website_url, &domain) {
                        true
                    } else if item.website_url.trim().is_empty() {
                        let title_low = item.title.to_lowercase();
                        domain.len() >= 4
                            && (title_low == domain || title_low.ends_with(&format!(".{domain}")))
                    } else {
                        false
                    };
                    if matches_domain {
                        let mut cred = JsonValue::new_object();
                        cred.insert("id", JsonValue::String(item.id.clone()));
                        cred.insert("title", JsonValue::String(item.title.clone()));
                        cred.insert("username", JsonValue::String(item.username.clone()));
                        cred.insert("websiteUrl", JsonValue::String(item.website_url.clone()));
                        cred.insert("hasTotp", JsonValue::Bool(!item.totp_secret.is_empty()));
                        cred.insert("vaultId", JsonValue::String(item.vault_id.clone()));
                        if let Some(v) = self.vault.vaults().iter().find(|v| v.id == item.vault_id)
                        {
                            cred.insert("vaultName", JsonValue::String(v.name.clone()));
                        }
                        matches.push(cred);
                    }
                }
                let mut data = JsonValue::new_object();
                data.insert("credentials", matches);
                Self::ok_envelope(request_id, "credentials_list", data)
            }
            "fill_credential" => {
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
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let item_id = payload.get_str_or("itemId", payload.get_str_or("id", ""));
                let is_update = payload.get_bool_or("isUpdate", false);
                let title =
                    payload.get_str_or("title", payload.get_str_or("domain", "Saved Login"));
                let username = payload.get_str_or("username", "");
                let password = payload.get_str_or("password", "");
                let url = payload.get_str_or("websiteUrl", payload.get_str_or("url", ""));
                let totp_secret = payload.get_str_or("totpSecret", "");

                let existing = if !item_id.is_empty() {
                    self.vault
                        .items()
                        .iter()
                        .find(|it| !it.trashed && it.id == item_id)
                        .cloned()
                } else if is_update {
                    self.vault
                        .items()
                        .iter()
                        .find(|it| {
                            !it.trashed
                                && it.category == Category::Login
                                && it.username.eq_ignore_ascii_case(username)
                                && (!url.trim().is_empty()
                                    && svm_webauthn::is_matching_login_domain(&it.website_url, url))
                        })
                        .cloned()
                } else {
                    None
                };

                let chosen_vault_id = payload.get_str_or("vaultId", "");
                let target_vault_id = if !chosen_vault_id.is_empty() {
                    chosen_vault_id.to_string()
                } else {
                    self.vault.default_vault_id()
                };

                let item = if let Some(mut old) = existing {
                    old.password = password.to_string();
                    if !chosen_vault_id.is_empty() {
                        old.vault_id = chosen_vault_id.to_string();
                    }
                    if !totp_secret.is_empty() {
                        old.totp_secret = totp_secret.to_string();
                        old.has_totp = true;
                    }
                    old
                } else {
                    VaultItem {
                        vault_id: target_vault_id,
                        title: title.to_string(),
                        category: Category::Login,
                        username: username.to_string(),
                        password: password.to_string(),
                        website_url: url.to_string(),
                        has_totp: !totp_secret.is_empty(),
                        totp_secret: totp_secret.to_string(),
                        ..Default::default()
                    }
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
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
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
            "decode_qr" => {
                match svm_bridge::handle_decode_qr_action(self.vault.is_unlocked(), &payload) {
                    Ok(data) => Self::ok_envelope(request_id, "qr_decoded", data),
                    Err((code, msg)) => Self::error_envelope(request_id, code, &msg),
                }
            }
            "update_totp" => {
                match svm_bridge::handle_update_totp_action(&mut self.vault, &payload) {
                    Ok(data) => Self::ok_envelope(request_id, "totp_updated", data),
                    Err((code, msg)) => Self::error_envelope(request_id, code, &msg),
                }
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
            "passkey_query" => {
                let data = svm_webauthn::query_passkeys_payload(
                    self.vault.is_unlocked(),
                    self.vault.items(),
                    &payload,
                );
                Self::ok_envelope(request_id, "passkey_query_result", data)
            }
            "passkey_create" => {
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let rp_id = payload.get_str_or("rpId", "");
                if rp_id.trim().is_empty() {
                    return Self::error_envelope(request_id, "INVALID_ARG", "rpId is required");
                }
                let rp_name = payload.get_str_or("rpName", rp_id);
                let user_name = payload.get_str_or("userName", "User");
                let user_display = payload.get_str_or("userDisplayName", user_name);
                let challenge = payload.get_str_or("challenge", "");
                let default_origin = format!("https://{rp_id}");
                let origin = payload.get_str_or("origin", &default_origin);
                let user_handle_b64 = payload.get_str_or("userHandle", "");
                let exclude_creds = payload
                    .get("excludeCredentials")
                    .and_then(|v| v.as_array())
                    .map(|a| a.to_vec())
                    .unwrap_or_default();
                if !exclude_creds.is_empty() {
                    let exists = self.vault.items().iter().any(|it| {
                        !it.trashed
                            && it.category == Category::Passkey
                            && svm_webauthn::matches_passkey_rp_id(it, rp_id)
                            && svm_webauthn::passkey_matches_credential_list(it, &exclude_creds)
                    });
                    if exists {
                        return Self::error_envelope(
                            request_id,
                            "CREDENTIAL_EXISTS",
                            "Passkey already registered for this account",
                        );
                    }
                }

                match svm_webauthn::create_passkey_credential(rp_id, challenge, origin) {
                    Ok(reg) => {
                        let cred_id_b64 = svm_webauthn::encode_base64url(&reg.credential_id);
                        let att_b64 = svm_webauthn::encode_base64url(&reg.attestation_object);
                        let client_json_b64 =
                            svm_webauthn::encode_base64url(reg.client_data_json.as_bytes());
                        let chosen_vid = payload.get_str_or("vaultId", "");
                        let default_vid = self.vault.default_vault_id();
                        let target_vid = if !chosen_vid.is_empty() {
                            chosen_vid
                        } else {
                            &default_vid
                        };
                        let params = svm_webauthn::PasskeyItemParams {
                            vault_id: target_vid,
                            rp_id,
                            rp_name,
                            user_name,
                            user_display,
                            origin,
                            user_handle_b64,
                        };
                        let item = svm_webauthn::build_passkey_item(&params, &reg);
                        let saved_id = match self.vault.upsert_item(item) {
                            Ok(id) => id,
                            Err(e) => {
                                return Self::error_envelope(
                                    request_id,
                                    "PASSKEY_SAVE_FAILED",
                                    e.message(),
                                )
                            }
                        };

                        let mut data = JsonValue::new_object();
                        data.insert("itemId", JsonValue::String(saved_id));
                        data.insert("credentialId", JsonValue::String(cred_id_b64));
                        data.insert("clientDataJSON", JsonValue::String(client_json_b64));
                        data.insert("attestationObject", JsonValue::String(att_b64));
                        Self::ok_envelope(request_id, "passkey_created", data)
                    }
                    Err(e) => Self::error_envelope(request_id, "PASSKEY_CREATE_FAILED", &e),
                }
            }
            "passkey_get" => {
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let rp_id = payload.get_str_or("rpId", "");
                let challenge = payload.get_str_or("challenge", "");
                let default_origin = format!("https://{rp_id}");
                let origin = payload.get_str_or("origin", &default_origin);
                let allow_creds = payload
                    .get("allowCredentials")
                    .and_then(|v| v.as_array())
                    .map(|a| a.to_vec())
                    .unwrap_or_default();

                let target_item = self
                    .vault
                    .items()
                    .iter()
                    .find(|it| {
                        !it.trashed
                            && it.category == Category::Passkey
                            && svm_webauthn::matches_passkey_rp_id(it, rp_id)
                            && (allow_creds.is_empty()
                                || svm_webauthn::passkey_matches_credential_list(it, &allow_creds))
                    })
                    .cloned();

                let Some(mut item) = target_item else {
                    return Self::error_envelope(
                        request_id,
                        "NOT_FOUND",
                        "Không tìm thấy Passkey cho trang web này",
                    );
                };

                let cred_id_str = svm_webauthn::extract_passkey_credential_id(&item);
                match svm_webauthn::execute_passkey_assertion(&mut item, rp_id, challenge, origin) {
                    Ok(assertion) => {
                        let _ = self.vault.upsert_item(item);
                        let cred_id_b64 = if !cred_id_str.is_empty() {
                            cred_id_str
                        } else {
                            svm_webauthn::encode_base64url(&assertion.credential_id)
                        };
                        let auth_data_b64 =
                            svm_webauthn::encode_base64url(&assertion.authenticator_data);
                        let sig_b64 = svm_webauthn::encode_base64url(&assertion.signature);
                        let client_json_b64 =
                            svm_webauthn::encode_base64url(assertion.client_data_json.as_bytes());

                        let mut data = JsonValue::new_object();
                        data.insert("credentialId", JsonValue::String(cred_id_b64));
                        data.insert("clientDataJSON", JsonValue::String(client_json_b64));
                        data.insert("authenticatorData", JsonValue::String(auth_data_b64));
                        data.insert("signature", JsonValue::String(sig_b64));
                        if let Some(uh) = assertion.user_handle {
                            data.insert(
                                "userHandle",
                                JsonValue::String(svm_webauthn::encode_base64url(&uh)),
                            );
                        }
                        Self::ok_envelope(request_id, "passkey_assertion", data)
                    }
                    Err(e) => Self::error_envelope(request_id, "PASSKEY_ASSERTION_FAILED", &e),
                }
            }
            "get_vaults" => {
                if !self.vault.is_unlocked() {
                    return Self::error_envelope(
                        request_id,
                        "VAULT_LOCKED",
                        "Vault is currently locked",
                    );
                }
                let raw_json = self.vault.list_vaults_json();
                let vaults = JsonValue::parse(&raw_json)
                    .ok()
                    .and_then(|p| p.get("vaults").cloned())
                    .unwrap_or_else(JsonValue::new_array);
                let mut data = JsonValue::new_object();
                data.insert("vaults", vaults);
                Self::ok_envelope(request_id, "vaults_list", data)
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

        // Lấy thông tin đăng nhập khi Desktop chưa chạy / kho đang khóa phải bị từ chối
        let creds_resp = router.handle_message(
            r#"{"requestId":"r2","action":"get_credentials","payload":{"domain":"github.com"}}"#,
        );
        assert!(creds_resp.contains(r#""ok":false"#));
        assert!(creds_resp.contains(r#""VAULT_LOCKED""#));

        // Khi Desktop App chưa chạy (không có socket), get_vault_status luôn phản hồi unlocked: false
        let unlocked_status =
            router.handle_message(r#"{"requestId":"r1b","action":"get_vault_status"}"#);
        assert!(unlocked_status.contains(r#""unlocked":false"#));
        assert!(unlocked_status.contains(r#""desktopRunning":false"#));

        // Yêu cầu tạo passkey khi kho đang khóa phải bị từ chối với VAULT_LOCKED
        let passkey_resp = router.handle_message(
            r#"{"requestId":"r3","action":"passkey_create","payload":{"rpId":"github.com","userName":"octocat","challenge":"abc123"}}"#,
        );
        assert!(passkey_resp.contains(r#""ok":false"#));
        assert!(passkey_resp.contains(r#""VAULT_LOCKED""#));
    }
}
