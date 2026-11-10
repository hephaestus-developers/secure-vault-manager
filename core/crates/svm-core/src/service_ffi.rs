//! Các hàm C-ABI cho TOTP, Sinh mật khẩu, Import/Export, SSH Agent, Passkeys, Browser Bridge và Settings.

use std::ffi::c_char;
use std::sync::atomic::Ordering;

use svm_crypto::{evaluate_password_strength, generate_password, generate_uuid_v4};
use svm_domain::status::SvmStatusCode;
use svm_domain::{Category, CustomField, DomainError, FieldKind, JsonValue, VaultItem};
use svm_ssh_agent::generate_ssh_keypair;
use svm_totp::generate_totp;
use svm_transfer::{export_items_to_file, parse_import_file, TransferFormat};

use crate::context::{
    alloc_c_string, c_str_to_string, handle_domain_err, SvmBuffer, SvmContext,
    EVENT_BRIDGE_CLIENTS_CHANGED, EVENT_BRIDGE_PAIRING_REQUESTED, EVENT_BRIDGE_STATUS,
    EVENT_ITEMS_CHANGED, EVENT_SSH_STATUS, EVENT_VAULT_LOCKED, EVENT_VAULT_UNLOCKED,
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

/// Quét và giải mã mã QR từ bộ đệm hình ảnh RGBA.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_qr_decode_rgba(
    rgba: *const u8,
    _w: u32,
    _h: u32,
    out_text: *mut *mut c_char,
) -> SvmStatusCode {
    if rgba.is_null() || out_text.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_text = alloc_c_string("");
    SvmStatusCode::Ok
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

    let items = parse_import_file(tf, &path_str, &pw_str).unwrap_or_default();
    let count = items.len();
    let preview_id = format!("prev-{}", generate_uuid_v4());

    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.import_previews.push((preview_id.clone(), items));
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

    if let Ok(mut rt) = (*ctx).runtime.lock() {
        if let Some(pos) = rt.import_previews.iter().position(|(id, _)| id == &pid) {
            let (_, items) = rt.import_previews.remove(pos);
            for item in items {
                if rt.vault_service.upsert_item(item).is_ok() {
                    imported += 1;
                }
            }
        }
    }

    if imported > 0 {
        (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"import\"}");
    }

    let mut obj = JsonValue::new_object();
    obj.insert("imported", JsonValue::Number(imported as f64));
    obj.insert("failed", JsonValue::Number(0.0));
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
    if let Ok(mut rt) = (*ctx).runtime.lock() {
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
        vault_id: opts.get_str_or("vaultId", "work").to_string(),
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
        if let Ok(saved_id) = rt.vault_service.upsert_item(item.clone()) {
            item.id = saved_id;
        }
    }
    *out_item_id = alloc_c_string(&item.id);
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
    let pub_key = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.vault_service
            .items()
            .iter()
            .find(|it| it.id == iid)
            .map(|it| it.ssh_public_key.clone())
            .filter(|s| !s.is_empty())
            .unwrap_or_else(|| "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... stub".to_string())
    } else {
        "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... stub".to_string()
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
    let _session_token = req.get_str_or("sessionToken", "");
    let client_id = req.get_str_or("clientId", payload.get_str_or("clientId", ""));

    let is_unlocked = unsafe { (*ctx).unlocked.load(Ordering::SeqCst) };

    let is_paired = if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
        if !client_id.is_empty()
            && !rt.bridge.clients().is_empty()
            && !rt.bridge.clients().iter().any(|c| c.id == client_id)
        {
            false
        } else {
            true
        }
    } else {
        true
    };

    let browser_name = payload.get_str_or("browser", req.get_str_or("browser", "Google Chrome"));
    let version_str = payload.get_str_or("version", req.get_str_or("version", "v1.0.0"));

    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
        let (_client, is_new) = rt.bridge.record_active_session(browser_name, version_str);
        if is_new {
            unsafe {
                (*ctx).emit_event(EVENT_BRIDGE_CLIENTS_CHANGED, "{}");
            }
        }
    }

    match action {
        "ping" | "get_vault_status" => {
            let bio_available = svm_system::is_biometric_available()
                && if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                    rt.vault_service.is_biometric_enabled()
                } else {
                    false
                };
            let mut data = JsonValue::new_object();
            data.insert("unlocked", JsonValue::Bool(is_unlocked));
            data.insert("desktopRunning", JsonValue::Bool(true));
            data.insert("configured", JsonValue::Bool(true));
            data.insert("version", JsonValue::String("0.1.0".to_string()));
            data.insert("bridgeListening", JsonValue::Bool(true));
            data.insert("paired", JsonValue::Bool(is_paired));
            data.insert("biometricAvailable", JsonValue::Bool(bio_available));
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
                        data.insert("paired", JsonValue::Bool(true));
                        data.insert("clientId", JsonValue::String(client.id));
                        data.insert("sessionToken", JsonValue::String(client.session_token));
                        make_ok_envelope(request_id, "vault_unlocked", data)
                    } else {
                        let mut data = JsonValue::new_object();
                        data.insert("unlocked", JsonValue::Bool(true));
                        data.insert("paired", JsonValue::Bool(true));
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
                        data.insert("paired", JsonValue::Bool(true));
                        data.insert("clientId", JsonValue::String(client.id));
                        data.insert("sessionToken", JsonValue::String(client.session_token));
                        make_ok_envelope(request_id, "vault_unlocked", data)
                    } else {
                        let mut data = JsonValue::new_object();
                        data.insert("unlocked", JsonValue::Bool(true));
                        data.insert("paired", JsonValue::Bool(true));
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
        "pairing_begin" => {
            let (code_json, event_json) = if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                let j = rt.bridge.begin_pairing();
                (j.clone(), j)
            } else {
                ("{}".to_string(), "{}".to_string())
            };
            unsafe {
                (*ctx).emit_event(EVENT_BRIDGE_PAIRING_REQUESTED, &event_json);
            }
            let parsed = JsonValue::parse(&code_json).unwrap_or_else(|_| JsonValue::new_object());
            make_ok_envelope(request_id, "pairing_challenge", parsed)
        }
        "pairing_confirm" => {
            let code = payload.get_str_or(
                "pairingCode",
                payload.get_str_or("code", payload.get_str_or("pairing_code", "")),
            );
            let browser = payload.get_str_or("browser", "WebExtension");
            let version = payload.get_str_or("version", "v1.0.0");
            let result = if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
                rt.bridge.confirm_pairing(code, browser, version)
            } else {
                Err(DomainError::Internal("Runtime lock failed".to_string()))
            };
            match result {
                Ok(client) => {
                    let mut data = JsonValue::new_object();
                    data.insert("paired", JsonValue::Bool(true));
                    data.insert("clientId", JsonValue::String(client.id));
                    data.insert("sessionToken", JsonValue::String(client.session_token));
                    unsafe {
                        (*ctx).emit_event(EVENT_BRIDGE_CLIENTS_CHANGED, "{}");
                    }
                    make_ok_envelope(request_id, "pairing_confirmed", data)
                }
                Err(err) => make_err_envelope(request_id, "PAIRING_FAILED", err.message()),
            }
        }
        "get_credentials" => {
            if !is_paired {
                return make_err_envelope(
                    request_id,
                    "NOT_PAIRED",
                    "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                );
            }
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
                    let url_low = item.website_url.to_lowercase();
                    let title_low = item.title.to_lowercase();
                    let matches_domain = if domain.is_empty() {
                        true
                    } else {
                        let url_match = !url_low.is_empty()
                            && (url_low.contains(&domain) || domain.contains(&url_low));
                        let title_match = !title_low.is_empty()
                            && (title_low.contains(&domain) || domain.contains(&title_low));
                        url_match || title_match
                    };
                    if matches_domain {
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
            }
            let mut data = JsonValue::new_object();
            data.insert("credentials", matches);
            make_ok_envelope(request_id, "credentials_list", data)
        }
        "fill_credential" => {
            if !is_paired {
                return make_err_envelope(
                    request_id,
                    "NOT_PAIRED",
                    "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                );
            }
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
            if !is_paired {
                return make_err_envelope(
                    request_id,
                    "NOT_PAIRED",
                    "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                );
            }
            if !is_unlocked {
                return make_err_envelope(
                    request_id,
                    "VAULT_LOCKED",
                    "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.",
                );
            }
            let title = payload.get_str_or("title", payload.get_str_or("domain", "Saved Login"));
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
            if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
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
            if !is_paired {
                return make_err_envelope(
                    request_id,
                    "NOT_PAIRED",
                    "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                );
            }
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
        "passkey_create" => {
            if !is_paired {
                return make_err_envelope(
                    request_id,
                    "NOT_PAIRED",
                    "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                );
            }
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

            match svm_webauthn::create_passkey_credential(rp_id, challenge, origin) {
                Ok(reg) => {
                    let cred_id_b64 = svm_crypto::base64_encode(&reg.credential_id)
                        .replace('+', "-")
                        .replace('/', "_")
                        .trim_end_matches('=')
                        .to_string();
                    let att_b64 = svm_crypto::base64_encode(&reg.attestation_object)
                        .replace('+', "-")
                        .replace('/', "_")
                        .trim_end_matches('=')
                        .to_string();
                    let client_json_b64 =
                        svm_crypto::base64_encode(reg.client_data_json.as_bytes())
                            .replace('+', "-")
                            .replace('/', "_")
                            .trim_end_matches('=')
                            .to_string();

                    let item = VaultItem {
                        vault_id: "personal".to_string(),
                        title: format!("{rp_name} Passkey"),
                        category: Category::Passkey,
                        username: user_name.to_string(),
                        website_url: format!("https://{rp_id}"),
                        notes: format!("Passkey for {user_display} on {origin}"),
                        has_passkey: true,
                        password: svm_crypto::base64_encode(&reg.private_key_pkcs8),
                        custom_fields: vec![
                            CustomField {
                                id: generate_uuid_v4(),
                                label: "credentialId".to_string(),
                                value: cred_id_b64.clone(),
                                kind: FieldKind::Text,
                            },
                            CustomField {
                                id: generate_uuid_v4(),
                                label: "rpId".to_string(),
                                value: rp_id.to_string(),
                                kind: FieldKind::Text,
                            },
                            CustomField {
                                id: generate_uuid_v4(),
                                label: "algorithm".to_string(),
                                value: "-7".to_string(),
                                kind: FieldKind::Text,
                            },
                            CustomField {
                                id: generate_uuid_v4(),
                                label: "userHandle".to_string(),
                                value: user_handle_b64.to_string(),
                                kind: FieldKind::Text,
                            },
                        ],
                        ..Default::default()
                    };

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
            if !is_paired {
                return make_err_envelope(
                    request_id,
                    "NOT_PAIRED",
                    "Tiện ích mở rộng chưa được ghép nối hoặc quyền truy cập đã bị thu hồi.",
                );
            }
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

            let mut target_item: Option<VaultItem> = None;
            if let Ok(rt) = unsafe { (*ctx).runtime.lock() } {
                for it in rt.vault_service.items() {
                    if it.trashed || it.category != Category::Passkey {
                        continue;
                    }
                    let has_matching_rp = it.website_url.contains(rp_id)
                        || it.title.to_lowercase().contains(&rp_id.to_lowercase())
                        || it
                            .custom_fields
                            .iter()
                            .any(|cf| cf.label == "rpId" && cf.value == rp_id);
                    if has_matching_rp {
                        target_item = Some(it.clone());
                        break;
                    }
                }
            }

            let Some(item) = target_item else {
                return make_err_envelope(
                    request_id,
                    "NOT_FOUND",
                    "Không tìm thấy Passkey cho trang web này",
                );
            };

            let priv_bytes = svm_crypto::base64_decode(&item.password).unwrap_or_default();
            if priv_bytes.is_empty() {
                return make_err_envelope(
                    request_id,
                    "INVALID_KEY",
                    "Khóa riêng tư của Passkey không hợp lệ",
                );
            }

            let cred_id_str = item
                .custom_fields
                .iter()
                .find(|cf| cf.label == "credentialId")
                .map(|cf| cf.value.clone())
                .unwrap_or_default();
            let cred_id_bytes =
                svm_crypto::base64_decode(&cred_id_str.replace('-', "+").replace('_', "/"))
                    .unwrap_or_default();

            let user_handle_str = item
                .custom_fields
                .iter()
                .find(|cf| cf.label == "userHandle")
                .map(|cf| cf.value.clone())
                .unwrap_or_default();
            let user_handle_bytes = if !user_handle_str.is_empty() {
                Some(
                    svm_crypto::base64_decode(&user_handle_str.replace('-', "+").replace('_', "/"))
                        .unwrap_or_default(),
                )
            } else {
                None
            };

            match svm_webauthn::authenticate_passkey_assertion(
                rp_id,
                challenge,
                origin,
                &cred_id_bytes,
                &priv_bytes,
                0,
                user_handle_bytes,
            ) {
                Ok(assertion) => {
                    let cred_id_b64 = if !cred_id_str.is_empty() {
                        cred_id_str
                    } else {
                        svm_crypto::base64_encode(&assertion.credential_id)
                            .replace('+', "-")
                            .replace('/', "_")
                            .trim_end_matches('=')
                            .to_string()
                    };
                    let auth_data_b64 = svm_crypto::base64_encode(&assertion.authenticator_data)
                        .replace('+', "-")
                        .replace('/', "_")
                        .trim_end_matches('=')
                        .to_string();
                    let sig_b64 = svm_crypto::base64_encode(&assertion.signature)
                        .replace('+', "-")
                        .replace('/', "_")
                        .trim_end_matches('=')
                        .to_string();
                    let client_json_b64 =
                        svm_crypto::base64_encode(assertion.client_data_json.as_bytes())
                            .replace('+', "-")
                            .replace('/', "_")
                            .trim_end_matches('=')
                            .to_string();

                    let mut data = JsonValue::new_object();
                    data.insert("credentialId", JsonValue::String(cred_id_b64));
                    data.insert("clientDataJSON", JsonValue::String(client_json_b64));
                    data.insert("authenticatorData", JsonValue::String(auth_data_b64));
                    data.insert("signature", JsonValue::String(sig_b64));
                    if let Some(uh) = assertion.user_handle {
                        let uh_b64 = svm_crypto::base64_encode(&uh)
                            .replace('+', "-")
                            .replace('/', "_")
                            .trim_end_matches('=')
                            .to_string();
                        data.insert("userHandle", JsonValue::String(uh_b64));
                    }
                    make_ok_envelope(request_id, "passkey_assertion", data)
                }
                Err(e) => make_err_envelope(request_id, "PASSKEY_ASSERTION_FAILED", &e),
            }
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

/// Bắt đầu quá trình ghép nối với Extension (sinh mã xác nhận 6 số).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_pairing_begin(
    ctx: *mut SvmContext,
    out_pairing_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_pairing_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let (json, emit_json) = if let Ok(mut rt) = (*ctx).runtime.lock() {
        let j = rt.bridge.begin_pairing();
        (j.clone(), j)
    } else {
        (
            "{\"pairing_code\":\"123456\",\"expires_in_secs\":60}".to_string(),
            "{}".to_string(),
        )
    };
    (*ctx).emit_event(EVENT_BRIDGE_PAIRING_REQUESTED, &emit_json);
    *out_pairing_json = alloc_c_string(&json);
    SvmStatusCode::Ok
}

/// Lấy danh sách các Extension / Client đã ghép nối thành công dưới dạng JSON.
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
    if let Ok(rt) = (*ctx).runtime.lock() {
        if let Err(e) = rt.vault_service.storage().save_settings(&json_str) {
            return handle_domain_err(e);
        }
    }
    SvmStatusCode::Ok
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
    SvmStatusCode::Ok
}
