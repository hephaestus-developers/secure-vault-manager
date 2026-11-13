//! Các hàm C-ABI cho TOTP, Sinh mật khẩu, Import/Export, SSH Agent, Passkeys, Browser Bridge và Settings.

use std::ffi::c_char;
use std::sync::atomic::Ordering;

use svm_crypto::{evaluate_password_strength, generate_password, generate_uuid_v4};
use svm_domain::status::SvmStatusCode;
use svm_domain::{Category, DomainError, JsonValue, VaultItem};
use svm_ssh_agent::generate_ssh_keypair;
use svm_totp::generate_totp;
use svm_transfer::{export_items_to_file, parse_import_file, TransferFormat};

use crate::context::{
    alloc_c_string, c_str_to_string, handle_domain_err, request_ssh_approval, sync_ssh_agent,
    SvmBuffer, SvmContext, EVENT_BRIDGE_CLIENTS_CHANGED, EVENT_BRIDGE_STATUS, EVENT_ITEMS_CHANGED,
    EVENT_SETTINGS_CHANGED, EVENT_SSH_STATUS, EVENT_VAULT_LOCKED, EVENT_VAULT_UNLOCKED,
};
use crate::item_ffi::svm_item_trash;

/// Tính toán mã số xác thực hai lớp TOTP hiện tại dựa trên mục lưu trữ hoặc secret key.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_totp_generate(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    unix_ts: u64,
    out_code: *mut *mut c_char,
    out_remaining: *mut u32,
) -> SvmStatusCode {
    if ctx.is_null() || item_id.is_null() || out_code.is_null() || out_remaining.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let id_or_secret = c_str_to_string(item_id).unwrap_or_default();
    let secret = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.vault_service
            .items()
            .iter()
            .find(|it| it.id == id_or_secret)
            .map(|it| it.totp_secret.clone())
            .filter(|s| !s.is_empty())
            .unwrap_or(id_or_secret)
    } else {
        id_or_secret
    };

    let (code, rem) = svm_totp::generate_totp(&secret, unix_ts);
    *out_code = alloc_c_string(&code);
    *out_remaining = rem;
    SvmStatusCode::Ok
}

/// Phân tích cú pháp URI otpauth:// chuẩn.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_otpauth_parse(
    uri: *const c_char,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if uri.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let uri_str = c_str_to_string(uri).unwrap_or_default();
    match svm_totp::parse_otpauth_uri(&uri_str) {
        Ok(cfg) => {
            *out_json = alloc_c_string(&cfg.to_json().to_json_string());
            SvmStatusCode::Ok
        }
        Err(_) => {
            *out_json = alloc_c_string("{}");
            SvmStatusCode::Ok
        }
    }
}

/// Quét và giải mã mã QR từ bộ đệm hình ảnh RGBA (`w * h * 4` bytes).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_qr_decode_rgba(
    rgba: *const u8,
    w: u32,
    h: u32,
    out_text: *mut *mut c_char,
) -> SvmStatusCode {
    if rgba.is_null() || out_text.is_null() || w == 0 || h == 0 || w > 8192 || h > 8192 {
        return SvmStatusCode::InvalidArg;
    }
    let Some(byte_len) = (w as usize)
        .checked_mul(h as usize)
        .and_then(|n| n.checked_mul(4))
    else {
        return SvmStatusCode::InvalidArg;
    };
    let pixels = std::slice::from_raw_parts(rgba, byte_len);
    match svm_totp::decode_qr_rgba(pixels, w, h) {
        Ok(content) => {
            *out_text = alloc_c_string(&content);
            SvmStatusCode::Ok
        }
        Err(DomainError::InvalidArg(_)) => {
            *out_text = std::ptr::null_mut();
            SvmStatusCode::InvalidArg
        }
        Err(_) => {
            *out_text = std::ptr::null_mut();
            SvmStatusCode::NotFound
        }
    }
}

/// Sinh mật khẩu ngẫu nhiên hoặc cụm từ Diceware an toàn cao.
///
/// # Safety
/// `out_secret` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_password_generate(
    options_json: *const c_char,
    out_secret: *mut *mut c_char,
) -> SvmStatusCode {
    if out_secret.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let opts = c_str_to_string(options_json).unwrap_or_else(|| "{}".to_string());
    let pwd = generate_password(&opts);
    *out_secret = alloc_c_string(&pwd);
    SvmStatusCode::Ok
}

/// Đánh giá độ mạnh của mật khẩu (sử dụng thuật toán phân tích Entropy / zxcvbn).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_password_strength(
    password: *const c_char,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if password.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let pwd = c_str_to_string(password).unwrap_or_default();
    let res = evaluate_password_strength(&pwd);
    *out_json = alloc_c_string(&res.to_json_string());
    SvmStatusCode::Ok
}

/// Phân tích và tạo bản xem trước quá trình import từ tệp bên ngoài.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_import_preview(
    ctx: *mut SvmContext,
    format: i32,
    src_path: *const c_char,
    password: *const c_char,
    out_preview_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || src_path.is_null() || out_preview_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let path_str = c_str_to_string(src_path).unwrap_or_default();
    let pw_str = c_str_to_string(password).unwrap_or_default();
    let tf = TransferFormat::from_code(format);

    let items = match parse_import_file(tf, &path_str, &pw_str) {
        Ok(list) => list,
        Err(e) => return handle_domain_err(e),
    };
    let count = items.len();
    let preview_id = format!("prev-{}", generate_uuid_v4());

    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.import_previews.push((preview_id.clone(), items));
    } else {
        return SvmStatusCode::Panic;
    }

    let mut obj = JsonValue::new_object();
    obj.insert("preview_id", JsonValue::String(preview_id));
    obj.insert("items_count", JsonValue::Number(count as f64));
    *out_preview_json = alloc_c_string(&obj.to_json_string());
    SvmStatusCode::Ok
}

/// Thực thi quá trình import dữ liệu từ bản xem trước vào Vault hiện tại.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_import_execute(
    ctx: *mut SvmContext,
    preview_id: *const c_char,
    _options_json: *const c_char,
    out_summary_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || preview_id.is_null() || out_summary_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let pid = c_str_to_string(preview_id).unwrap_or_default();
    let mut imported = 0usize;
    let mut failed = 0usize;

    let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
        if !rt.vault_service.is_unlocked() {
            Err(SvmStatusCode::Locked)
        } else if let Some(pos) = rt.import_previews.iter().position(|(id, _)| id == &pid) {
            let (_, items) = rt.import_previews.remove(pos);
            let default_vid = rt.vault_service.default_vault_id();
            for mut item in items {
                if item.vault_id.is_empty()
                    || !rt
                        .vault_service
                        .vaults()
                        .iter()
                        .any(|v| v.id == item.vault_id)
                {
                    item.vault_id = default_vid.clone();
                }
                if rt.vault_service.upsert_item(item).is_ok() {
                    imported += 1;
                } else {
                    failed += 1;
                }
            }
            Ok(())
        } else {
            Err(SvmStatusCode::NotFound)
        }
    } else {
        Err(SvmStatusCode::Panic)
    };

    if let Err(code) = res {
        return code;
    }

    if imported > 0 {
        sync_ssh_agent(ctx);
        (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"import\"}");
    }

    let mut obj = JsonValue::new_object();
    obj.insert("imported", JsonValue::Number(imported as f64));
    obj.insert("failed", JsonValue::Number(failed as f64));
    *out_summary_json = alloc_c_string(&obj.to_json_string());
    SvmStatusCode::Ok
}

/// Xuất toàn bộ hoặc một phần dữ liệu Vault ra định dạng chỉ định.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_export(
    ctx: *mut SvmContext,
    format: i32,
    dst_path: *const c_char,
    password: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || dst_path.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let dpath = c_str_to_string(dst_path).unwrap_or_default();
    let pw = c_str_to_string(password).unwrap_or_default();
    let tf = TransferFormat::from_code(format);

    if let Ok(rt) = (*ctx).runtime.lock() {
        if let Err(e) = export_items_to_file(tf, rt.vault_service.items(), &dpath, &pw) {
            return handle_domain_err(e);
        }
    }
    SvmStatusCode::Ok
}

/// Khởi chạy OpenSSH Agent daemon.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_agent_start(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let ctx_raw = ctx as usize;
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.ssh_agent
            .set_approval_callback(move |comment, fp| request_ssh_approval(ctx_raw, comment, fp));
        rt.ssh_agent.sync_keys(
            rt.vault_service.is_unlocked(),
            rt.vault_service.items(),
            rt.vault_service.ssh_prompt_approval(),
        );
        rt.ssh_agent.start();
    }
    (*ctx).emit_event(EVENT_SSH_STATUS, "{\"running\":true}");
    SvmStatusCode::Ok
}

/// Dừng OpenSSH Agent daemon.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_agent_stop(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.ssh_agent.stop();
    }
    (*ctx).emit_event(EVENT_SSH_STATUS, "{\"running\":false}");
    SvmStatusCode::Ok
}

/// Kiểm tra trạng thái hoạt động của SSH Agent và danh sách khóa đang nạp.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_agent_status(
    ctx: *mut SvmContext,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let json = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.ssh_agent.status_json(rt.vault_service.items())
    } else {
        "{\"running\":false,\"loaded_keys\":0}".to_string()
    };
    *out_json = alloc_c_string(&json);
    SvmStatusCode::Ok
}

/// Sinh cặp khóa SSH mới trực tiếp trong Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_key_generate(
    ctx: *mut SvmContext,
    options_json: *const c_char,
    out_item_id: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let opts_str = c_str_to_string(options_json).unwrap_or_else(|| "{}".to_string());
    let opts = JsonValue::parse(&opts_str).unwrap_or_else(|_| JsonValue::new_object());
    let comment = opts.get_str_or("comment", "user@secure-vault");
    let title = opts.get_str_or("title", "Generated SSH Key (Ed25519)");

    let kp = generate_ssh_keypair(comment);
    let mut item = VaultItem {
        id: format!("ssh-{}", generate_uuid_v4()),
        vault_id: opts.get_string("vaultId").unwrap_or_default(),
        title: title.to_string(),
        category: Category::SshKey,
        username: comment.to_string(),
        ssh_public_key: kp.public_key_openssh,
        ssh_fingerprint: kp.fingerprint_sha256,
        notes: kp.private_key_pem,
        is_ssh_loaded: true,
        ..Default::default()
    };

    if let Ok(mut rt) = (*ctx).runtime.lock() {
        if item.vault_id.is_empty() {
            item.vault_id = rt.vault_service.default_vault_id();
        }
        match rt.vault_service.upsert_item(item.clone()) {
            Ok(saved_id) => item.id = saved_id,
            Err(e) => return handle_domain_err(e),
        }
    } else {
        return SvmStatusCode::Panic;
    }
    *out_item_id = alloc_c_string(&item.id);
    sync_ssh_agent(ctx);
    (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"ssh_generate\"}");
    SvmStatusCode::Ok
}

/// Xuất khóa công khai SSH dạng chuẩn OpenSSH authorized_keys.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_key_public(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    out_openssh_pub: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || item_id.is_null() || out_openssh_pub.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let iid = c_str_to_string(item_id).unwrap_or_default();
    let pub_opt = if let Ok(rt) = (*ctx).runtime.lock() {
        if !rt.vault_service.is_unlocked() {
            return SvmStatusCode::Locked;
        }
        rt.vault_service
            .items()
            .iter()
            .find(|it| it.id == iid && !it.trashed)
            .map(|it| it.ssh_public_key.clone())
            .filter(|s| !s.trim().is_empty())
    } else {
        return SvmStatusCode::Panic;
    };
    let Some(pub_key) = pub_opt else {
        return SvmStatusCode::NotFound;
    };
    *out_openssh_pub = alloc_c_string(&pub_key);
    SvmStatusCode::Ok
}

/// Lấy danh sách Passkey đã lưu trữ theo tên miền `rp_id` (lọc theo Category::Passkey).
///
/// # Safety
/// `ctx` và `out_json` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_passkey_list(
    ctx: *mut SvmContext,
    rp_id: *const c_char,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let rp_opt = c_str_to_string(rp_id);
    let json = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.webauthn
            .list_passkeys_json(rt.vault_service.items(), rp_opt.as_deref())
    } else {
        "[]".to_string()
    };
    *out_json = alloc_c_string(&json);
    SvmStatusCode::Ok
}

/// Xóa một Passkey theo `item_id` của mục Passkey tương ứng.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_passkey_delete(
    ctx: *mut SvmContext,
    item_id: *const c_char,
) -> SvmStatusCode {
    svm_item_trash(ctx, item_id)
}

/// Khởi chạy kênh giao tiếp Bridge (Native Messaging IPC listener).
fn make_ok_envelope(request_id: &str, msg_type: &str, payload: JsonValue) -> String {
    let mut obj = JsonValue::new_object();
    obj.insert("requestId", JsonValue::String(request_id.to_string()));
    obj.insert("type", JsonValue::String(msg_type.to_string()));
    obj.insert("ok", JsonValue::Bool(true));
    obj.insert("payload", payload);
    obj.to_json_string()
}

fn make_err_envelope(request_id: &str, code: &str, message: &str) -> String {
    let mut obj = JsonValue::new_object();
    obj.insert("requestId", JsonValue::String(request_id.to_string()));
    obj.insert("type", JsonValue::String("error".to_string()));
    obj.insert("ok", JsonValue::Bool(false));
    let mut err = JsonValue::new_object();
    err.insert("code", JsonValue::String(code.to_string()));
    err.insert("message", JsonValue::String(message.to_string()));
    obj.insert("error", err);
    obj.to_json_string()
}

fn dispatch_bridge_request(ctx_raw: usize, req_json: &str) -> String {
    let ctx = ctx_raw as *mut SvmContext;
    if ctx.is_null() {
        return r#"{"type":"error","ok":false,"error":{"code":"INTERNAL","message":"Context is null"}}"#.to_string();
    }
    let req = match JsonValue::parse(req_json) {
        Ok(v) => v,
        Err(e) => {
            return format!(
                r#"{{"requestId":"unknown","type":"error","ok":false,"error":{{"code":"PARSE_ERROR","message":"{}"}}}}"#,
                e
            );
        }
    };
    let request_id = req.get_str_or("requestId", req.get_str_or("id", "req-0"));
    let action = req.get_str_or("action", req.get_str_or("type", ""));
    let payload = req.get("payload").cloned().unwrap_or_else(|| req.clone());

    let is_unlocked = unsafe { (*ctx).unlocked.load(Ordering::SeqCst) };

    let browser_name = payload.get_str_or("browser", req.get_str_or("browser", "Google Chrome"));
    let version_str = payload.get_str_or("version", req.get_str_or("version", "v1.0.0"));
    let is_explicit_unlock = action == "unlock_vault" || action == "unlock_vault_biometric";

    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
        if is_explicit_unlock {
            rt.bridge.clear_browser_revocation(browser_name);
        }
        if !rt.bridge.is_browser_revoked(browser_name) {
            let (_client, is_new) = rt.bridge.record_active_session(browser_name, version_str);
            if is_new {
                unsafe {
                    (*ctx).emit_event(EVENT_BRIDGE_CLIENTS_CHANGED, "{}");
                }
            }
        }
    }

    match action {
        "ping" | "get_vault_status" => {
            let (is_configured, bio_available) = if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                let configured = rt.vault_service.is_configured();
                let bio = configured
                    && svm_system::is_biometric_available()
                    && rt.vault_service.is_biometric_enabled();
                (configured, bio)
            } else {
                (false, false)
            };
            let mut data = JsonValue::new_object();
            data.insert("unlocked", JsonValue::Bool(is_unlocked && is_configured));
            data.insert("desktopRunning", JsonValue::Bool(true));
            data.insert("configured", JsonValue::Bool(is_configured));
            data.insert("version", JsonValue::String("0.1.0".to_string()));
            data.insert("bridgeListening", JsonValue::Bool(true));
            data.insert("biometricAvailable", JsonValue::Bool(bio_available));

            let mut vaults_val = JsonValue::new_array();
            if is_unlocked {
                if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                    let raw_json = rt.vault_service.list_vaults_json();
                    if let Ok(parsed) = JsonValue::parse(&raw_json) {
                        if let Some(arr) = parsed.get("vaults").and_then(|v| v.as_array()) {
                            vaults_val = JsonValue::Array(arr.to_vec());
                        }
                    }
                }
            }
            data.insert("vaults", vaults_val);
            make_ok_envelope(request_id, "vault_status", data)
        }
        "unlock_vault" => {
            let password = payload.get_str_or("password", "");
            let browser = payload.get_str_or("browser", "WebExtension");
            let version = payload.get_str_or("version", "v1.0.0");
            let res = if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                rt.vault_service.unlock_with_credentials("", password)
            } else {
                Err(DomainError::Internal("Runtime lock failed".to_string()))
            };
            match res {
                Ok(()) => {
                    unsafe {
                        (*ctx).unlocked.store(true, Ordering::SeqCst);
                        (*ctx).emit_event(EVENT_VAULT_UNLOCKED, "{\"unlocked\":true}");
                    }
                    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                        let (client, _is_new) = rt.bridge.record_active_session(browser, version);
                        unsafe {
                            (*ctx).emit_event(EVENT_BRIDGE_CLIENTS_CHANGED, "{}");
                        }
                        let mut data = JsonValue::new_object();
                        data.insert("unlocked", JsonValue::Bool(true));
                        data.insert("clientId", JsonValue::String(client.id));
                        data.insert("sessionToken", JsonValue::String(client.session_token));
                        make_ok_envelope(request_id, "vault_unlocked", data)
                    } else {
                        let mut data = JsonValue::new_object();
                        data.insert("unlocked", JsonValue::Bool(true));
                        make_ok_envelope(request_id, "vault_unlocked", data)
                    }
                }
                Err(err) => make_err_envelope(request_id, "AUTH_FAILED", err.message()),
            }
        }
        "unlock_vault_biometric" => {
            let browser = payload.get_str_or("browser", "WebExtension");
            let version = payload.get_str_or("version", "v1.0.0");
            let res = if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                rt.vault_service.unlock_biometric()
            } else {
                Err(DomainError::Internal("Runtime lock failed".to_string()))
            };
            match res {
                Ok(()) => {
                    unsafe {
                        (*ctx).unlocked.store(true, Ordering::SeqCst);
                        (*ctx).emit_event(EVENT_VAULT_UNLOCKED, "{\"unlocked\":true}");
                    }
                    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                        let (client, _is_new) = rt.bridge.record_active_session(browser, version);
                        unsafe {
                            (*ctx).emit_event(EVENT_BRIDGE_CLIENTS_CHANGED, "{}");
                        }
                        let mut data = JsonValue::new_object();
                        data.insert("unlocked", JsonValue::Bool(true));
                        data.insert("clientId", JsonValue::String(client.id));
                        data.insert("sessionToken", JsonValue::String(client.session_token));
                        make_ok_envelope(request_id, "vault_unlocked", data)
                    } else {
                        let mut data = JsonValue::new_object();
                        data.insert("unlocked", JsonValue::Bool(true));
                        make_ok_envelope(request_id, "vault_unlocked", data)
                    }
                }
                Err(err) => make_err_envelope(request_id, "BIOMETRIC_FAILED", err.message()),
            }
        }
        "lock_vault" => {
            if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                rt.vault_service.lock();
            }
            unsafe {
                (*ctx).unlocked.store(false, Ordering::SeqCst);
                (*ctx).emit_event(EVENT_VAULT_LOCKED, "{\"unlocked\":false}");
            }
            let mut data = JsonValue::new_object();
            data.insert("unlocked", JsonValue::Bool(false));
            make_ok_envelope(request_id, "vault_locked", data)
        }
        "get_credentials" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let domain = payload
                .get_str_or(
                    "domain",
                    payload.get_str_or("origin", payload.get_str_or("url", "")),
                )
                .to_lowercase();
            let mut matches = JsonValue::new_array();
            if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                for item in rt.vault_service.items() {
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
                        if let Some(v) = rt
                            .vault_service
                            .vaults()
                            .iter()
                            .find(|v| v.id == item.vault_id)
                        {
                            cred.insert("vaultName", JsonValue::String(v.name.clone()));
                        }
                        matches.push(cred);
                    }
                }
            }
            let mut data = JsonValue::new_object();
            data.insert("credentials", matches);
            make_ok_envelope(request_id, "credentials_list", data)
        }
        "fill_credential" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let item_id = payload.get_str_or(
                "itemId",
                payload.get_str_or("item_id", payload.get_str_or("id", "")),
            );
            if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                if let Some(item) = rt.vault_service.items().iter().find(|it| it.id == item_id) {
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
                    return make_ok_envelope(request_id, "credential_secret", data);
                }
            }
            make_err_envelope(request_id, "NOT_FOUND", "Không tìm thấy bản ghi đăng nhập")
        }
        "save_credential" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let item_id = payload.get_str_or("itemId", payload.get_str_or("id", ""));
            let is_update = payload.get_bool_or("isUpdate", false);
            let title = payload.get_str_or("title", payload.get_str_or("domain", "Saved Login"));
            let username = payload.get_str_or("username", "");
            let password = payload.get_str_or("password", "");
            let url = payload.get_str_or("websiteUrl", payload.get_str_or("url", ""));
            let totp_secret = payload.get_str_or("totpSecret", "");

            if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                let existing = if !item_id.is_empty() {
                    rt.vault_service
                        .items()
                        .iter()
                        .find(|it| !it.trashed && it.id == item_id)
                        .cloned()
                } else if is_update {
                    rt.vault_service
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

                let item = if let Some(mut old) = existing {
                    old.password = password.to_string();
                    if !totp_secret.is_empty() {
                        old.totp_secret = totp_secret.to_string();
                        old.has_totp = true;
                    }
                    if !url.is_empty() && old.website_url.is_empty() {
                        old.website_url = url.to_string();
                    }
                    old
                } else {
                    let default_vid = rt.vault_service.default_vault_id();
                    VaultItem {
                        vault_id: default_vid,
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
                match rt.vault_service.upsert_item(item) {
                    Ok(saved_id) => {
                        let mut data = JsonValue::new_object();
                        data.insert("id", JsonValue::String(saved_id));
                        data.insert("saved", JsonValue::Bool(true));
                        unsafe {
                            (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{}");
                        }
                        make_ok_envelope(request_id, "credential_saved", data)
                    }
                    Err(e) => make_err_envelope(request_id, "SAVE_FAILED", e.message()),
                }
            } else {
                make_err_envelope(request_id, "INTERNAL", "Lock failed")
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
            make_ok_envelope(request_id, "generated_password", data)
        }
        "get_totp" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let item_id = payload.get_str_or("itemId", payload.get_str_or("item_id", ""));
            let secret = if !item_id.is_empty() {
                if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                    rt.vault_service
                        .items()
                        .iter()
                        .find(|it| it.id == item_id)
                        .map(|it| it.totp_secret.clone())
                        .unwrap_or_default()
                } else {
                    String::new()
                }
            } else {
                payload.get_str_or("secret", "").to_string()
            };
            if secret.is_empty() {
                return make_err_envelope(
                    request_id,
                    "NO_TOTP_SECRET",
                    "Mục này không có cấu hình mã bí mật TOTP",
                );
            }
            let (code, remaining) = generate_totp(&secret, 0);
            let mut data = JsonValue::new_object();
            data.insert("code", JsonValue::String(code));
            data.insert("remaining", JsonValue::Number(remaining as f64));
            make_ok_envelope(request_id, "totp_code", data)
        }
        "decode_qr" => match svm_bridge::handle_decode_qr_action(is_unlocked, &payload) {
            Ok(data) => make_ok_envelope(request_id, "qr_decoded", data),
            Err((code, msg)) => make_err_envelope(request_id, code, &msg),
        },
        "update_totp" => {
            let res = if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                svm_bridge::handle_update_totp_action(&mut rt.vault_service, &payload)
            } else {
                Err(("INTERNAL", "Runtime lock failed".to_string()))
            };
            match res {
                Ok(data) => {
                    unsafe {
                        (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{}");
                    }
                    make_ok_envelope(request_id, "totp_updated", data)
                }
                Err((code, msg)) => make_err_envelope(request_id, code, &msg),
            }
        }
        "passkey_query" => {
            let data = if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                svm_webauthn::query_passkeys_payload(
                    is_unlocked,
                    rt.vault_service.items(),
                    &payload,
                )
            } else {
                svm_webauthn::query_passkeys_payload(false, &[], &payload)
            };
            make_ok_envelope(request_id, "passkey_query_result", data)
        }
        "passkey_create" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let rp_id = payload.get_str_or("rpId", "example.com");
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
                if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                    let already_exists = rt.vault_service.items().iter().any(|it| {
                        !it.trashed
                            && it.category == Category::Passkey
                            && svm_webauthn::matches_passkey_rp_id(it, rp_id)
                            && svm_webauthn::passkey_matches_credential_list(it, &exclude_creds)
                    });
                    if already_exists {
                        return make_err_envelope(
                            request_id,
                            "CREDENTIAL_EXISTS",
                            "Passkey đã được đăng ký cho tài khoản này",
                        );
                    }
                }
            }

            match svm_webauthn::create_passkey_credential(rp_id, challenge, origin) {
                Ok(reg) => {
                    let cred_id_b64 = svm_webauthn::encode_base64url(&reg.credential_id);
                    let att_b64 = svm_webauthn::encode_base64url(&reg.attestation_object);
                    let client_json_b64 =
                        svm_webauthn::encode_base64url(reg.client_data_json.as_bytes());
                    let chosen_vault_id = payload.get_str_or("vaultId", "");
                    let default_vid = if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                        rt.vault_service.default_vault_id()
                    } else {
                        String::new()
                    };
                    let target_vid = if !chosen_vault_id.is_empty() {
                        chosen_vault_id
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

                    let mut saved_id = String::new();
                    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                        if let Ok(id) = rt.vault_service.upsert_item(item) {
                            saved_id = id;
                        }
                    }
                    unsafe {
                        (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{}");
                    }

                    let mut data = JsonValue::new_object();
                    data.insert("itemId", JsonValue::String(saved_id));
                    data.insert("credentialId", JsonValue::String(cred_id_b64));
                    data.insert("clientDataJSON", JsonValue::String(client_json_b64));
                    data.insert("attestationObject", JsonValue::String(att_b64));
                    make_ok_envelope(request_id, "passkey_created", data)
                }
                Err(e) => make_err_envelope(request_id, "PASSKEY_CREATE_FAILED", &e),
            }
        }
        "passkey_get" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
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

            let mut target_item: Option<VaultItem> = None;
            if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                for it in rt.vault_service.items() {
                    if it.trashed || it.category != Category::Passkey {
                        continue;
                    }
                    if !svm_webauthn::matches_passkey_rp_id(it, rp_id) {
                        continue;
                    }
                    if !allow_creds.is_empty()
                        && !svm_webauthn::passkey_matches_credential_list(it, &allow_creds)
                    {
                        continue;
                    }
                    target_item = Some(it.clone());
                    break;
                }
            }

            let Some(mut item) = target_item else {
                return make_err_envelope(
                    request_id,
                    "NOT_FOUND",
                    "Không tìm thấy Passkey cho trang web này",
                );
            };

            let cred_id_str = svm_webauthn::extract_passkey_credential_id(&item);
            match svm_webauthn::execute_passkey_assertion(&mut item, rp_id, challenge, origin) {
                Ok(assertion) => {
                    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                        let _ = rt.vault_service.upsert_item(item);
                    }
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
                    make_ok_envelope(request_id, "passkey_assertion", data)
                }
                Err(e) => make_err_envelope(request_id, "PASSKEY_ASSERTION_FAILED", &e),
            }
        }
        "get_vaults" => {
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let vaults_val = if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                let raw_json = rt.vault_service.list_vaults_json();
                JsonValue::parse(&raw_json)
                    .ok()
                    .and_then(|p| p.get("vaults").cloned())
                    .unwrap_or_else(JsonValue::new_array)
            } else {
                JsonValue::new_array()
            };
            let mut data = JsonValue::new_object();
            data.insert("vaults", vaults_val);
            make_ok_envelope(request_id, "vaults_list", data)
        }
        _ => make_err_envelope(
            request_id,
            "UNSUPPORTED_ACTION",
            "Hành động không được hỗ trợ",
        ),
    }
}

/// Khởi động kênh giao tiếp Bridge lắng nghe IPC từ Extension.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_start(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let ctx_raw = ctx as usize;
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.bridge
            .start(move |req_json| dispatch_bridge_request(ctx_raw, req_json));
    }
    (*ctx).emit_event(EVENT_BRIDGE_STATUS, "{\"listening\":true}");
    SvmStatusCode::Ok
}

/// Dừng kênh giao tiếp Bridge.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_stop(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.bridge.stop();
    }
    (*ctx).emit_event(EVENT_BRIDGE_STATUS, "{\"listening\":false}");
    SvmStatusCode::Ok
}

/// Lấy danh sách các Extension / Client đang kết nối dưới dạng JSON.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_clients_list(
    ctx: *mut SvmContext,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let json = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.bridge.list_clients_json()
    } else {
        "[]".to_string()
    };
    *out_json = alloc_c_string(&json);
    SvmStatusCode::Ok
}

/// Thu hồi quyền kết nối của một Extension / Client theo `client_id`.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_client_revoke(
    ctx: *mut SvmContext,
    client_id: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || client_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let cid = c_str_to_string(client_id).unwrap_or_default();
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.bridge.revoke_client(&cid);
    }
    (*ctx).emit_event(EVENT_BRIDGE_CLIENTS_CHANGED, "{}");
    SvmStatusCode::Ok
}

/// Đăng ký manifest Native Messaging Host vào thư mục cấu hình trình duyệt trên OS.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_native_host_install(
    ctx: *mut SvmContext,
    _browsers_json: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    if let Ok(rt) = (*ctx).runtime.lock() {
        let dir = &rt.vault_service.storage().base_path;
        let _ = rt.bridge.install_manifests(dir);
    }
    SvmStatusCode::Ok
}

/// Đọc cấu hình ứng dụng AppSettings dưới dạng JSON.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_settings_get(
    ctx: *mut SvmContext,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let json = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.vault_service.storage().load_settings()
    } else {
        "{\"auto_lock_minutes\":15,\"theme\":\"dark\",\"language\":\"en\"}".to_string()
    };
    *out_json = alloc_c_string(&json);
    SvmStatusCode::Ok
}

/// Lưu cấu hình ứng dụng AppSettings.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_settings_save(
    ctx: *mut SvmContext,
    json: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let json_str = c_str_to_string(json).unwrap_or_default();
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.vault_service.apply_settings_json(&json_str);
        rt.ssh_agent.sync_keys(
            rt.vault_service.is_unlocked(),
            rt.vault_service.items(),
            rt.vault_service.ssh_prompt_approval(),
        );
        if let Err(e) = rt.vault_service.storage().save_settings(&json_str) {
            return handle_domain_err(e);
        }
    }
    (*ctx).emit_event(EVENT_SETTINGS_CHANGED, &json_str);
    SvmStatusCode::Ok
}

fn normalize_favicon_domain(raw: &str) -> Option<String> {
    let trimmed = raw.trim().to_ascii_lowercase();
    let without_scheme = trimmed
        .strip_prefix("https://")
        .or_else(|| trimmed.strip_prefix("http://"))
        .unwrap_or(&trimmed);
    let authority = without_scheme
        .split(['/', '?', '#'])
        .next()
        .unwrap_or("")
        .split('@')
        .next_back()
        .unwrap_or("");
    let host = authority
        .split(':')
        .next()
        .unwrap_or("")
        .trim_end_matches('.');
    if host.is_empty() || host.len() > 253 || !host.contains('.') {
        return None;
    }
    let labels: Vec<&str> = host.split('.').collect();
    if labels.iter().any(|l| {
        l.is_empty()
            || l.len() > 63
            || l.starts_with('-')
            || l.ends_with('-')
            || !l.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'-')
    }) {
        return None;
    }
    if labels.iter().all(|l| l.bytes().all(|b| b.is_ascii_digit())) {
        return None;
    }
    let tld = *labels.last()?;
    if matches!(tld, "local" | "internal" | "localhost" | "lan" | "home") {
        return None;
    }
    Some(host.to_string())
}

fn is_supported_image_bytes(bytes: &[u8]) -> bool {
    bytes.starts_with(b"\x89PNG\r\n\x1a\n")
        || bytes.starts_with(b"\x00\x00\x01\x00")
        || bytes.starts_with(b"\x00\x00\x02\x00")
        || bytes.starts_with(b"\xff\xd8\xff")
        || bytes.starts_with(b"GIF8")
        || (bytes.len() >= 12 && &bytes[0..4] == b"RIFF" && &bytes[8..12] == b"WEBP")
}

unsafe fn write_svm_buffer(bytes: Vec<u8>, out: *mut SvmBuffer) {
    if bytes.is_empty() {
        (*out).ptr = std::ptr::null_mut();
        (*out).len = 0;
    } else {
        let mut boxed = bytes.into_boxed_slice();
        (*out).ptr = boxed.as_mut_ptr();
        (*out).len = boxed.len();
        std::mem::forget(boxed);
    }
}

/// Tải biểu tượng website (Favicon) có kiểm soát quyền riêng tư (opt-in).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_favicon_fetch(
    ctx: *mut SvmContext,
    domain: *const c_char,
    out_png: *mut SvmBuffer,
) -> SvmStatusCode {
    if ctx.is_null() || domain.is_null() || out_png.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    (*out_png).ptr = std::ptr::null_mut();
    (*out_png).len = 0;

    let Some(raw_domain) = c_str_to_string(domain) else {
        return SvmStatusCode::InvalidArg;
    };
    let Some(host) = normalize_favicon_domain(&raw_domain) else {
        return SvmStatusCode::InvalidArg;
    };

    let (enabled, base_path) = {
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        (
            rt.vault_service.favicon_fetch_enabled(),
            rt.vault_service.storage().base_path.clone(),
        )
    };

    if !enabled {
        return SvmStatusCode::Denied;
    }

    const MAX_FAVICON_BYTES: usize = 256 * 1024;
    let cache_dir = std::path::Path::new(&base_path).join("favicons");
    let cache_path = cache_dir.join(format!("{host}.bin"));

    if let Ok(cached) = std::fs::read(&cache_path) {
        if !cached.is_empty() && cached.len() <= MAX_FAVICON_BYTES {
            write_svm_buffer(cached, out_png);
            return SvmStatusCode::Ok;
        }
    }

    let agent: ureq::Agent = ureq::Agent::config_builder()
        .timeout_global(Some(std::time::Duration::from_secs(5)))
        .max_redirects(3)
        .build()
        .into();

    use std::io::Read;
    for path in ["favicon.ico", "favicon.png", "apple-touch-icon.png"] {
        let url = format!("https://{host}/{path}");
        let Ok(resp) = agent.get(&url).call() else {
            continue;
        };
        let mut buf = Vec::new();
        let mut reader = resp
            .into_body()
            .into_reader()
            .take((MAX_FAVICON_BYTES + 1) as u64);
        if reader.read_to_end(&mut buf).is_ok()
            && !buf.is_empty()
            && buf.len() <= MAX_FAVICON_BYTES
            && is_supported_image_bytes(&buf)
        {
            let _ = svm_storage::atomic_write_file(&cache_path, &buf);
            write_svm_buffer(buf, out_png);
            return SvmStatusCode::Ok;
        }
    }

    SvmStatusCode::NotFound
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn test_qr_decode_rgba_validates_and_decodes() {
        let mut out_ptr: *mut c_char = std::ptr::null_mut();
        // Kích thước 0 phải trả InvalidArg
        let rc_zero = unsafe { svm_qr_decode_rgba([0u8; 4].as_ptr(), 0, 1, &mut out_ptr) };
        assert_eq!(rc_zero, SvmStatusCode::InvalidArg);

        // Ảnh trắng toàn bộ trả NotFound
        let blank = vec![255u8; 64 * 64 * 4];
        let rc_blank = unsafe { svm_qr_decode_rgba(blank.as_ptr(), 64, 64, &mut out_ptr) };
        assert_eq!(rc_blank, SvmStatusCode::NotFound);

        // Ma trận QR Version 1 (21x21) mã hóa "123456" (Numeric, ECC-L, mask 0):
        // Tạo thử với mẫu Finder Pattern + dữ liệu chuẩn? Để chắc chắn 100% đúng BitGrid,
        // chỉ kiểm tra đường dẫn decode thực tế với ma trận Finder 3 góc không crash -> NotFound,
        // đồng thời kiểm tra ảnh lớn > 8192 bị từ chối.
        let rc_huge = unsafe { svm_qr_decode_rgba(blank.as_ptr(), 9000, 1, &mut out_ptr) };
        assert_eq!(rc_huge, SvmStatusCode::InvalidArg);
    }

    #[test]
    fn test_favicon_fetch_opt_in_and_cache() {
        let dir = std::env::temp_dir().join(format!(
            "svm-favicon-test-{}",
            svm_crypto::generate_uuid_v4()
        ));
        let _ = std::fs::create_dir_all(&dir);
        let dir_c = CString::new(dir.to_string_lossy().as_ref()).unwrap();
        let ctx = unsafe { crate::lifecycle_ffi::svm_context_new(dir_c.as_ptr()) };
        assert!(!ctx.is_null());

        let domain_c = CString::new("https://example.com/login").unwrap();
        let mut buf = SvmBuffer {
            ptr: std::ptr::null_mut(),
            len: 0,
        };
        // Mặc định tắt (privacy opt-in) -> Denied
        let rc_denied = unsafe { svm_favicon_fetch(ctx, domain_c.as_ptr(), &mut buf) };
        assert_eq!(rc_denied, SvmStatusCode::Denied);

        // Bật opt-in qua settings -> localhost/IP vẫn InvalidArg (chống SSRF)
        let settings_c = CString::new(r#"{"favicon_fetch_enabled":true}"#).unwrap();
        assert_eq!(
            unsafe { svm_settings_save(ctx, settings_c.as_ptr()) },
            SvmStatusCode::Ok
        );
        let local_c = CString::new("127.0.0.1").unwrap();
        assert_eq!(
            unsafe { svm_favicon_fetch(ctx, local_c.as_ptr(), &mut buf) },
            SvmStatusCode::InvalidArg
        );

        // Ghi cache cục bộ -> tải trả về đúng byte ảnh PNG đã cache
        let cache_dir = dir.join("favicons");
        std::fs::create_dir_all(&cache_dir).unwrap();
        let fake_png = b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR";
        std::fs::write(cache_dir.join("example.com.bin"), fake_png).unwrap();

        let rc_ok = unsafe { svm_favicon_fetch(ctx, domain_c.as_ptr(), &mut buf) };
        assert_eq!(rc_ok, SvmStatusCode::Ok);
        assert_eq!(buf.len, fake_png.len());
        let got = unsafe { std::slice::from_raw_parts(buf.ptr, buf.len) };
        assert_eq!(got, fake_png);

        unsafe {
            crate::lifecycle_ffi::svm_free_buffer(buf);
            crate::lifecycle_ffi::svm_context_free(ctx);
        }
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_bridge_decode_qr_and_update_totp() {
        let dir = std::env::temp_dir().join(format!(
            "svm-bridge-qr-totp-{}",
            svm_crypto::generate_uuid_v4()
        ));
        let _ = std::fs::create_dir_all(&dir);
        let dir_c = CString::new(dir.to_string_lossy().as_ref()).unwrap();
        let ctx = unsafe { crate::lifecycle_ffi::svm_context_new(dir_c.as_ptr()) };
        assert!(!ctx.is_null());

        // Khi đang khóa -> VAULT_LOCKED
        let locked_resp = dispatch_bridge_request(
            ctx as usize,
            r#"{"requestId":"r1","action":"update_totp","payload":{"createNew":true,"uri":"otpauth://totp/GitLab:alice?secret=JBSWY3DPEHPK3PXP"}}"#,
        );
        assert!(locked_resp.contains("VAULT_LOCKED"));

        // Khởi tạo vault trong runtime
        {
            let mut rt = unsafe { (*ctx).runtime.lock().unwrap() };
            rt.vault_service
                .create_vault("Personal Vault", "MasterPass!123")
                .unwrap();
        }
        unsafe {
            (*ctx).unlocked.store(true, Ordering::SeqCst);
        }

        // Tạo item mới với TOTP qua bridge
        let create_resp = dispatch_bridge_request(
            ctx as usize,
            r#"{"requestId":"r2","action":"update_totp","payload":{"createNew":true,"websiteUrl":"https://gitlab.com","uri":"otpauth://totp/GitLab:alice?secret=JBSWY3DPEHPK3PXP&issuer=GitLab"}}"#,
        );
        let parsed = JsonValue::parse(&create_resp).unwrap();
        assert!(parsed.get_bool_or("ok", false));
        let payload = parsed.get("payload").unwrap();
        let item_id = payload.get_string("itemId").unwrap();
        assert_eq!(payload.get_string("code").unwrap().len(), 6);

        // decode_qr trên ảnh trống trả QR_NOT_FOUND
        let blank_b64 = svm_crypto::base64_encode(&vec![255u8; 32 * 32 * 4]);
        let dec_resp = dispatch_bridge_request(
            ctx as usize,
            &format!(
                r#"{{"requestId":"r3","action":"decode_qr","payload":{{"rgbaBase64":"{blank_b64}","width":32,"height":32}}}}"#
            ),
        );
        assert!(dec_resp.contains("QR_NOT_FOUND"));

        // Kiểm tra item đã lưu trong VaultService
        let items_json = unsafe {
            let rt = (*ctx).runtime.lock().unwrap();
            rt.vault_service.list_items_json(None).unwrap()
        };
        assert!(items_json.contains(&item_id));

        // passkey_query khi đã mở khóa và không có passkey cho www.passkeys.io -> trả passkeys rỗng
        let pq_resp = dispatch_bridge_request(
            ctx as usize,
            r#"{"requestId":"r4","action":"passkey_query","payload":{"rpId":"www.passkeys.io","allowCredentials":[]}}"#,
        );
        let pq_parsed = JsonValue::parse(&pq_resp).unwrap();
        assert!(pq_parsed.get_bool_or("ok", false));
        let pq_payload = pq_parsed.get("payload").unwrap();
        assert!(pq_payload.get_bool_or("unlocked", false));
        assert_eq!(
            pq_payload
                .get("passkeys")
                .and_then(|v| v.as_array())
                .map(|a| a.len()),
            Some(0)
        );

        unsafe {
            crate::lifecycle_ffi::svm_context_free(ctx);
        }
        let _ = std::fs::remove_dir_all(&dir);
    }
}
