//! Các hàm C-ABI quản lý mục dữ liệu (`VaultItem`), lịch sử sửa đổi và tệp đính kèm.

use std::ffi::c_char;

use svm_domain::status::SvmStatusCode;

use crate::context::{
    alloc_c_string, c_str_to_string, ffi_guard, handle_domain_err, sync_ssh_agent, SvmBuffer,
    SvmContext, EVENT_ITEMS_CHANGED,
};

/// Lấy danh sách các mục trong Vault theo bộ lọc `filter_json`.
///
/// # Safety
/// `ctx` và `out_items_json` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_items_list(
    ctx: *mut SvmContext,
    filter_json: *const c_char,
    out_items_json: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || out_items_json.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let filter_opt = c_str_to_string(filter_json);
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.list_items_json(filter_opt.as_deref()) {
            Ok(json) => {
                *out_items_json = alloc_c_string(&json);
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Lấy thông tin chi tiết của một mục.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_get(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    out_item_json: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() || out_item_json.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let id = c_str_to_string(item_id).unwrap_or_default();
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.get_item_json(&id) {
            Ok(json) => {
                *out_item_json = alloc_c_string(&json);
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Thêm mới hoặc cập nhật thông tin một mục trong Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_upsert(
    ctx: *mut SvmContext,
    item_json: *const c_char,
    out_item_id: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_json.is_null() || out_item_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let json = c_str_to_string(item_json).unwrap_or_default();
        let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
            rt.vault_service.upsert_item_json(&json)
        } else {
            return SvmStatusCode::Panic;
        };
        match res {
            Ok(id) => {
                *out_item_id = alloc_c_string(&id);
                sync_ssh_agent(ctx);
                (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"upsert\"}");
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Chuyển một mục vào Thùng rác (xóa mềm).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_trash(
    ctx: *mut SvmContext,
    item_id: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let id = c_str_to_string(item_id).unwrap_or_default();
        let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
            rt.vault_service.trash_item(&id)
        } else {
            return SvmStatusCode::Panic;
        };
        match res {
            Ok(()) => {
                sync_ssh_agent(ctx);
                (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"trash\"}");
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Khôi phục một mục từ Thùng rác về vị trí ban đầu.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_restore(
    ctx: *mut SvmContext,
    item_id: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let id = c_str_to_string(item_id).unwrap_or_default();
        let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
            rt.vault_service.restore_item(&id)
        } else {
            return SvmStatusCode::Panic;
        };
        match res {
            Ok(()) => {
                sync_ssh_agent(ctx);
                (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"restore\"}");
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Xóa vĩnh viễn một mục khỏi Thùng rác (xóa triệt để).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_purge(
    ctx: *mut SvmContext,
    item_id: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let id = c_str_to_string(item_id).unwrap_or_default();
        let res = if let Ok(mut rt) = (*ctx).runtime.lock() {
            rt.vault_service.purge_item(&id)
        } else {
            return SvmStatusCode::Panic;
        };
        match res {
            Ok(()) => {
                sync_ssh_agent(ctx);
                (*ctx).emit_event(EVENT_ITEMS_CHANGED, "{\"action\":\"purge\"}");
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Xóa nhanh một mục khỏi Vault (tương đương đưa vào thùng rác).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_delete(
    ctx: *mut SvmContext,
    item_id: *const c_char,
) -> SvmStatusCode {
    svm_item_trash(ctx, item_id)
}

/// Trích xuất plaintext của một trường bí mật theo `field_id`.
///
/// # Safety
/// Vùng nhớ trả về qua `out_secret` BẮT BUỘC phải được giải phóng bằng `svm_free_secret`.
#[no_mangle]
pub unsafe extern "C" fn svm_item_reveal_field(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    field_id: *const c_char,
    out_secret: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() || field_id.is_null() || out_secret.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let iid = c_str_to_string(item_id).unwrap_or_default();
        let fid = c_str_to_string(field_id).unwrap_or_default();
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.reveal_field(&iid, &fid) {
            Ok(secret) => {
                *out_secret = alloc_c_string(&secret);
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Lấy danh sách các bản sửa đổi lịch sử của một mục.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_history_list(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() || out_json.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let iid = c_str_to_string(item_id).unwrap_or_default();
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.list_history_json(&iid) {
            Ok(json) => {
                *out_json = alloc_c_string(&json);
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Phục hồi một mục về bản sửa đổi cụ thể trong lịch sử.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_history_restore(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    revision: u32,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let iid = c_str_to_string(item_id).unwrap_or_default();
        let Ok(mut rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.restore_history(&iid, revision) {
            Ok(()) => SvmStatusCode::Ok,
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Thêm tệp đính kèm vào một mục (mã hóa AEAD).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_attachment_add(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    src_path: *const c_char,
    out_attachment_json: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() || src_path.is_null() || out_attachment_json.is_null()
        {
            return SvmStatusCode::InvalidArg;
        }
        let iid = c_str_to_string(item_id).unwrap_or_default();
        let spath = c_str_to_string(src_path).unwrap_or_default();
        let Ok(mut rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.add_attachment(&iid, &spath) {
            Ok(json) => {
                *out_attachment_json = alloc_c_string(&json);
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Giải mã và xuất tệp đính kèm ra đường dẫn ngoài đĩa.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_attachment_export(
    ctx: *mut SvmContext,
    attachment_id: *const c_char,
    dst_path: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || attachment_id.is_null() || dst_path.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let aid = c_str_to_string(attachment_id).unwrap_or_default();
        let dpath = c_str_to_string(dst_path).unwrap_or_default();
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.export_attachment(&aid, &dpath) {
            Ok(()) => SvmStatusCode::Ok,
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Đọc toàn bộ nội dung tệp đính kèm vào RAM (dùng cho việc xem trước tệp nhỏ).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_attachment_read(
    ctx: *mut SvmContext,
    attachment_id: *const c_char,
    out: *mut SvmBuffer,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || attachment_id.is_null() || out.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let aid = c_str_to_string(attachment_id).unwrap_or_default();
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        let bytes = match rt.vault_service.read_attachment(&aid) {
            Ok(b) => b,
            Err(e) => return handle_domain_err(e),
        };
        if bytes.is_empty() {
            (*out).ptr = std::ptr::null_mut();
            (*out).len = 0;
        } else {
            let mut boxed = bytes.into_boxed_slice();
            (*out).ptr = boxed.as_mut_ptr();
            (*out).len = boxed.len();
            std::mem::forget(boxed);
        }
        SvmStatusCode::Ok
    })
}

/// Xóa một tệp đính kèm khỏi kho dữ liệu.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_attachment_delete(
    ctx: *mut SvmContext,
    attachment_id: *const c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || attachment_id.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let aid = c_str_to_string(attachment_id).unwrap_or_default();
        let Ok(mut rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.delete_attachment(&aid) {
            Ok(()) => SvmStatusCode::Ok,
            Err(e) => handle_domain_err(e),
        }
    })
}

/// Lấy danh sách metadata các tệp đính kèm của một mục dưới dạng JSON.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_attachments_list(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    ffi_guard(|| {
        if ctx.is_null() || item_id.is_null() || out_json.is_null() {
            return SvmStatusCode::InvalidArg;
        }
        let iid = c_str_to_string(item_id).unwrap_or_default();
        let Ok(rt) = (*ctx).runtime.lock() else {
            return SvmStatusCode::Panic;
        };
        match rt.vault_service.list_attachments_json(&iid) {
            Ok(json) => {
                *out_json = alloc_c_string(&json);
                SvmStatusCode::Ok
            }
            Err(e) => handle_domain_err(e),
        }
    })
}
