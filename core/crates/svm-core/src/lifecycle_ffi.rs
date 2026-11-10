//! Các hàm C-ABI quản lý vòng đời ngữ cảnh `SvmContext`, cấp phát/giải phóng bộ nhớ và điều phối Prompt.

use std::ffi::{c_char, CString};
use std::sync::atomic::AtomicBool;
use std::sync::Mutex;

use svm_bridge::BridgeServer;
use svm_crypto::zeroize_slice;
use svm_domain::status::SvmStatusCode;
use svm_domain::JsonValue;
use svm_ssh_agent::SshAgentServer;
use svm_storage::VaultStorage;
use svm_vault::VaultService;
use svm_webauthn::WebAuthnAuthenticator;

use crate::context::{
    alloc_c_string, c_str_to_string, CoreRuntime, SvmBuffer, SvmContext, SvmEventCallback,
    LAST_ERROR,
};

/// Lấy mã số phiên bản nhị phân ABI (dùng để UI xác thực tính tương thích).
#[no_mangle]
pub extern "C" fn svm_abi_version() -> u32 {
    1
}

/// Lấy chuỗi phiên bản tĩnh của lõi SVM Core (Caller KHÔNG giải phóng con trỏ này).
#[no_mangle]
pub extern "C" fn svm_core_version() -> *const c_char {
    static VERSION: &[u8] = b"0.1.0\0";
    VERSION.as_ptr() as *const c_char
}

/// Khởi tạo ngữ cảnh `SvmContext` mới.
///
/// # Safety
/// `data_dir` có thể là NULL hoặc con trỏ C-string hợp lệ kết thúc bằng `\0`.
#[no_mangle]
pub unsafe extern "C" fn svm_context_new(data_dir: *const c_char) -> *mut SvmContext {
    let dir = c_str_to_string(data_dir);
    let effective_dir = dir
        .clone()
        .filter(|s| !s.trim().is_empty())
        .unwrap_or_else(svm_storage::resolve_default_data_dir);
    let storage = VaultStorage::new(effective_dir.clone());

    let runtime = CoreRuntime {
        vault_service: VaultService::with_storage(storage),
        ssh_agent: SshAgentServer::new(),
        bridge: BridgeServer::new(effective_dir),
        webauthn: WebAuthnAuthenticator::new(),
        import_previews: Vec::new(),
        pending_prompts: Vec::new(),
    };

    let ctx = Box::new(SvmContext {
        data_dir: dir,
        unlocked: AtomicBool::new(false),
        event_callback: None,
        event_user_data: std::ptr::null_mut(),
        runtime: Mutex::new(runtime),
    });

    Box::into_raw(ctx)
}

/// Hủy ngữ cảnh và giải phóng toàn bộ runtime, socket listeners và bộ nhớ.
///
/// # Safety
/// `ctx` phải là con trỏ được cấp phát từ `svm_context_new` hoặc NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_context_free(ctx: *mut SvmContext) {
    if !ctx.is_null() {
        drop(Box::from_raw(ctx));
    }
}

/// Đọc thông điệp lỗi chi tiết nhất phát sinh trên luồng hiện tại (mượn từ TLS, KHÔNG giải phóng).
#[no_mangle]
pub extern "C" fn svm_last_error() -> *const c_char {
    LAST_ERROR.with(|cell| cell.borrow().as_ptr())
}

/// Đăng ký hàm callback nhận sự kiện từ Rust Core lên UI.
///
/// # Safety
/// `ctx` phải là con trỏ ngữ cảnh hợp lệ.
#[no_mangle]
pub unsafe extern "C" fn svm_set_event_callback(
    ctx: *mut SvmContext,
    cb: SvmEventCallback,
    user_data: *mut std::ffi::c_void,
) {
    if let Some(ctx_ref) = ctx.as_mut() {
        ctx_ref.event_callback = cb;
        ctx_ref.event_user_data = user_data;
    }
}

/// Thiết lập các biện pháp gia cố tiến trình (tắt core dump, chặn ptrace).
#[no_mangle]
pub extern "C" fn svm_harden_process() -> SvmStatusCode {
    svm_system::harden_current_process();
    SvmStatusCode::Ok
}

/// Giải phóng chuỗi thông thường được cấp phát bởi Rust.
///
/// # Safety
/// `p` phải là con trỏ do Rust cấp phát qua CString::into_raw hoặc NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_free_string(p: *mut c_char) {
    if !p.is_null() {
        drop(CString::from_raw(p));
    }
}

/// Giải phóng chuỗi mật mã: ghi đè bằng 0 (zeroize) trước khi giải phóng bộ nhớ.
///
/// # Safety
/// `p` phải là con trỏ chuỗi mật mã do Rust cấp phát hoặc NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_free_secret(p: *mut c_char) {
    if !p.is_null() {
        let s = CString::from_raw(p);
        let mut bytes = s.into_bytes_with_nul();
        zeroize_slice(&mut bytes);
    }
}

/// Giải phóng bộ đệm nhị phân `SvmBuffer` (tự động zeroize vùng nhớ trước khi thu hồi).
///
/// # Safety
/// `buf.ptr` phải là con trỏ bộ nhớ hợp lệ hoặc NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_free_buffer(buf: SvmBuffer) {
    if !buf.ptr.is_null() && buf.len > 0 {
        let slice = std::slice::from_raw_parts_mut(buf.ptr, buf.len);
        zeroize_slice(slice);
        drop(Vec::from_raw_parts(buf.ptr, buf.len, buf.len));
    }
}

/// Phản hồi một yêu cầu phê duyệt từ UI gửi xuống Core.
///
/// # Safety
/// `ctx` và `request_id` không được phép là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_prompt_respond(
    ctx: *mut SvmContext,
    request_id: *const c_char,
    decision_json: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || request_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let Some(req_id) = c_str_to_string(request_id) else {
        return SvmStatusCode::InvalidArg;
    };
    let decision_str = c_str_to_string(decision_json).unwrap_or_else(|| "{}".to_string());
    let approved = JsonValue::parse(&decision_str)
        .map(|v| v.get_bool_or("approved", false))
        .unwrap_or(false);

    let removed_waiter = if let Ok(mut rt) = (*ctx).runtime.lock() {
        if let Some(pos) = rt.pending_prompts.iter().position(|(id, _)| id == &req_id) {
            let (_, entry) = rt.pending_prompts.remove(pos);
            Some(entry.waiter)
        } else {
            None
        }
    } else {
        return SvmStatusCode::Panic;
    };

    if let Some(waiter) = removed_waiter {
        let (lock, cvar) = &*waiter;
        let mut guard = lock.lock().unwrap_or_else(|e| e.into_inner());
        *guard = Some(approved);
        cvar.notify_all();
    }
    SvmStatusCode::Ok
}

/// Lấy danh sách toàn bộ các yêu cầu phê duyệt đang chờ xử lý dưới dạng JSON.
///
/// # Safety
/// `ctx` và `out_json` không được phép là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_prompt_list_pending(
    ctx: *mut SvmContext,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let mut arr = JsonValue::new_array();
    if let Ok(rt) = (*ctx).runtime.lock() {
        for (_, entry) in &rt.pending_prompts {
            arr.push(entry.info.clone());
        }
    }
    *out_json = alloc_c_string(&arr.to_json_string());
    SvmStatusCode::Ok
}
