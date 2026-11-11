//! Các hàm C-ABI quản trị danh sách hồ sơ Vault, mở khóa/khóa kho, sinh trắc học và đổi mật khẩu.

use std::ffi::c_char;
use std::sync::atomic::Ordering;

use svm_domain::status::SvmStatusCode;

use crate::context::{
    alloc_c_string, c_str_to_string, ffi_guard, handle_domain_err, sync_ssh_agent, SvmContext,
    EVENT_VAULT_LOCKED, EVENT_VAULT_UNLOCKED,
};

/// Lấy danh sách các hồ sơ Vault đã tạo / đã biết.
///
/// # Safety
/// `ctx` và `out_registry_json` không được phép là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_list(
    ctx: *mut SvmContext,
    out_registry_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_registry_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let json = if let Ok(rt) = (*ctx).runtime.lock() {
        rt.vault_service.list_vaults_json()
    } else {
        "{\"vaults\":[]}".to_string()
    };
    *out_registry_json = alloc_c_string(&json);
    SvmStatusCode::Ok
}

/// Đọc Recovery Key của Vault (chỉ khả dụng khi kho đang MỞ KHÓA).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_get_recovery_key(
    ctx: *mut SvmContext,
    out_key: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_key.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let Ok(rt) = (*ctx).runtime.lock() else {
        return SvmStatusCode::Panic;
    };
    if !rt.vault_service.is_unlocked() {
        return SvmStatusCode::Locked;
    }
    let key = rt.vault_service.recovery_key();
    *out_key = alloc_c_string(key);
    SvmStatusCode::Ok
}

/// Tạo mới một Vault hoàn chỉnh với Master Password.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_create(
    ctx: *mut SvmContext,
    name: *const c_char,
    password: *const c_char,
    out_vault_id: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || name.is_null() || password.is_null() || out_vault_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let name_str = c_str_to_string(name).unwrap_or_default();
    let pw_str = c_str_to_string(password).unwrap_or_default();

    let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.vault_service.create_vault(&name_str, &pw_str)
    } else {
        return SvmStatusCode::Panic;
    };
    match res {
        Ok(id) => {
            (*ctx).unlocked.store(true, Ordering::SeqCst);
            sync_ssh_agent(ctx);
            *out_vault_id = alloc_c_string(&id);
            SvmStatusCode::Ok
        }
        Err(e) => handle_domain_err(e),
    }
}

/// Mở khóa một Vault bằng Master Password.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_unlock(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
    password: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || vault_id.is_null() || password.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let vid = c_str_to_string(vault_id).unwrap_or_default();
    let pw = c_str_to_string(password).unwrap_or_default();

    let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.vault_service.unlock_with_credentials(&vid, &pw)
    } else {
        return SvmStatusCode::Panic;
    };

    match res {
        Ok(()) => {
            (*ctx).unlocked.store(true, Ordering::SeqCst);
            sync_ssh_agent(ctx);
            (*ctx).emit_event(EVENT_VAULT_UNLOCKED, "{\"unlocked\":true}");
            SvmStatusCode::Ok
        }
        Err(e) => handle_domain_err(e),
    }
}

/// Kiểm tra phần cứng và dịch vụ xác thực sinh trắc học của OS có khả dụng hay không.
#[no_mangle]
pub extern "C" fn svm_biometric_is_available() -> bool {
    svm_system::is_biometric_available()
}

/// Kích hoạt mở khóa sinh trắc học cho một Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_biometric_enable(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
    password: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || vault_id.is_null() || password.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let pw = c_str_to_string(password).unwrap_or_default();
    let Ok(mut rt) = (*ctx).runtime.lock() else {
        return SvmStatusCode::Panic;
    };
    match rt.vault_service.enable_biometric(&pw) {
        Ok(()) => SvmStatusCode::Ok,
        Err(e) => handle_domain_err(e),
    }
}

/// Hủy kích hoạt sinh trắc học của một Vault và xóa khóa bọc khỏi OS Keystore.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_biometric_disable(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || vault_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let Ok(mut rt) = (*ctx).runtime.lock() else {
        return SvmStatusCode::Panic;
    };
    match rt.vault_service.disable_biometric() {
        Ok(()) => SvmStatusCode::Ok,
        Err(e) => handle_domain_err(e),
    }
}

/// Mở khóa Vault thông qua xác thực sinh trắc học hệ thống.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_unlock_biometric(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || vault_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.vault_service.unlock_biometric()
    } else {
        return SvmStatusCode::Panic;
    };
    match res {
        Ok(()) => {
            (*ctx).unlocked.store(true, Ordering::SeqCst);
            sync_ssh_agent(ctx);
            (*ctx).emit_event(EVENT_VAULT_UNLOCKED, "{\"unlocked\":true}");
            SvmStatusCode::Ok
        }
        Err(e) => handle_domain_err(e),
    }
}

/// Khóa tức thì Vault hiện tại, xóa toàn bộ khóa giải mã trong RAM.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_lock(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.vault_service.lock();
    }
    (*ctx).unlocked.store(false, Ordering::SeqCst);
    sync_ssh_agent(ctx);
    (*ctx).emit_event(EVENT_VAULT_LOCKED, "{\"unlocked\":false}");
    SvmStatusCode::Ok
}

/// Kiểm tra trạng thái Vault hiện tại có đang mở khóa hay không.
///
/// # Safety
/// `ctx` có thể là NULL (trả về false).
#[no_mangle]
pub unsafe extern "C" fn svm_vault_is_unlocked(ctx: *const SvmContext) -> bool {
    if ctx.is_null() {
        return false;
    }
    (*ctx).unlocked.load(Ordering::SeqCst)
}

/// Đổi tên hoặc cập nhật thông tin một Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_rename(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
    name: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || vault_id.is_null() || name.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let vid = c_str_to_string(vault_id).unwrap_or_default();
        let n = c_str_to_string(name).unwrap_or_default();
        let Ok(mut rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.rename_vault(&vid, &n) {
            Ok(()) => SvmStatusCode::Ok,
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Xóa một Vault (yêu cầu cung cấp mật khẩu để xác nhận).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_delete(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
    password: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || vault_id.is_null() || password.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let vid = c_str_to_string(vault_id).unwrap_or_default();
        let pw = c_str_to_string(password).unwrap_or_default();
        let Ok(mut rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.delete_vault(&vid, &pw) {
            Ok(()) => SvmStatusCode::Ok,
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Đặt một Vault làm kho mặc định khi khởi động ứng dụng.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_set_default(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || vault_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let vid = c_str_to_string(vault_id).unwrap_or_default();
        let Ok(mut rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.set_default_vault(&vid) {
            Ok(()) => SvmStatusCode::Ok,
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Đổi Master Password của Vault (tái mã hóa DEK bằng KEK mới).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_change_password(
    ctx: *mut SvmContext,
    old_pw: *const c_char,
    new_pw: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || old_pw.is_null() || new_pw.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let old_s = c_str_to_string(old_pw).unwrap_or_default();
    let new_s = c_str_to_string(new_pw).unwrap_or_default();
    let Ok(mut rt) = (*ctx).runtime.lock() else {
        return SvmStatusCode::Panic;
    };
    match rt.vault_service.change_password(&old_s, &new_s) {
        Ok(()) => SvmStatusCode::Ok,
        Err(e) => handle_domain_err(e),
    }
}

/// Tạo lại (xoay) Recovery Key mới cho Vault (yêu cầu Master Password hợp lệ).
/// Trả về chuỗi khóa mới qua `out_key` (cần giải phóng bằng `svm_free_secret`).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_rotate_recovery_key(
    ctx: *mut SvmContext,
    master_pw: *const c_char,
    out_key: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || master_pw.is_null() || out_key.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let pw_s = c_str_to_string(master_pw).unwrap_or_default();
    let Ok(mut rt) = (*ctx).runtime.lock() else {
        return SvmStatusCode::Panic;
    };
    match rt.vault_service.rotate_recovery_key(&pw_s) {
        Ok(new_key) => {
            *out_key = alloc_c_string(&new_key);
            SvmStatusCode::Ok
        }
        Err(e) => handle_domain_err(e),
    }
}

/// Báo cáo số mili-giây nhàn rỗi của hệ thống phục vụ bộ đếm Auto-Lock.
///
/// # Safety
/// `ctx` có thể là NULL (bỏ qua an toàn).
#[no_mangle]
pub unsafe extern "C" fn svm_vault_report_idle(ctx: *mut SvmContext, idle_ms: u64) {
    if ctx.is_null() {
        return;
    }
    let locked_now = if let Ok(mut rt) = (*ctx).runtime.lock() {
        rt.vault_service.report_idle(idle_ms)
    } else {
        false
    };
    if locked_now {
        (*ctx).unlocked.store(false, Ordering::SeqCst);
        sync_ssh_agent(ctx);
        (*ctx).emit_event(
            EVENT_VAULT_LOCKED,
            "{\"unlocked\":false,\"reason\":\"idle\"}",
        );
    }
}

/// Đặt lại kho dữ liệu hoàn toàn (sao lưu kho cũ dạng abandoned và xóa các cấu hình liên quan).
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_reset(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    let Ok(mut rt) = (*ctx).runtime.lock() else {
        return SvmStatusCode::Panic;
    };
    match rt.vault_service.reset_vault() {
        Ok(_) => {
            (*ctx).unlocked.store(false, Ordering::SeqCst);
            sync_ssh_agent(ctx);
            (*ctx).emit_event(
                EVENT_VAULT_LOCKED,
                "{\"unlocked\":false,\"reason\":\"reset\"}",
            );
            SvmStatusCode::Ok
        }
        Err(e) => handle_domain_err(e),
    }
}
