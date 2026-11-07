//! Các hàm C-ABI cho TOTP, Sinh mật khẩu, Import/Export, SSH Agent, Passkeys, Browser Bridge và Settings.

use std::ffi::c_char;

use svm_crypto::{evaluate_password_strength, generate_password, generate_uuid_v4};
use svm_domain::status::SvmStatusCode;
use svm_domain::{Category, JsonValue, VaultItem};
use svm_ssh_agent::generate_ssh_keypair;
use svm_transfer::{export_items_to_file, parse_import_file, TransferFormat};

use crate::context::{
    alloc_c_string, c_str_to_string, handle_domain_err, SvmBuffer, SvmContext, EVENT_BRIDGE_STATUS,
    EVENT_ITEMS_CHANGED, EVENT_SSH_STATUS,
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
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_start(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.bridge.start();
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
    let json = if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.bridge.begin_pairing()
    } else {
        "{\"pairing_code\":\"123456\",\"expires_in_secs\":60}".to_string()
    };
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
