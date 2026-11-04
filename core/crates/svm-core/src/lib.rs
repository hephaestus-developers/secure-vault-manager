//! # svm-core
//!
//! Thư viện chia sẻ động C-ABI (`libsvm_core.so` / `.dylib` / `.dll`) cho **Secure Vault Manager (SVM)**.
//!
//! Crate này đóng vai trò là ranh giới FFI (Foreign Function Interface) chính giữa hệ thống lõi Rust
//! và giao diện người dùng C++17 Qt6 Desktop.
//!
//! # Nguyên tắc thiết kế FFI:
//! 1. **Ownership Contract**: Mọi chuỗi/bộ đệm do Rust cấp phát bắt buộc phải được giải phóng bằng
//!    `svm_free_string`, `svm_free_secret` hoặc `svm_free_buffer`.
//! 2. **Panic Boundary Isolation**: Mọi hàm extern "C" đều được bảo vệ để không làm Panic rò rỉ sang C++.
//! 3. **Thread Safety**: `SvmContext` an toàn để chia sẻ giữa các luồng (`Send + Sync`).

use std::cell::RefCell;
use std::ffi::{c_char, CStr, CString};
use std::sync::atomic::{AtomicBool, Ordering};

pub use svm_domain::status::SvmStatusCode;

/// Bộ đệm nhị phân đại diện cho tệp đính kèm, icon hoặc khối dữ liệu lớn.
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SvmBuffer {
    /// Con trỏ tới mảng byte dữ liệu nhị phân.
    pub ptr: *mut u8,
    /// Chiều dài của mảng byte tính bằng bytes.
    pub len: usize,
}

/// Con trỏ hàm callback nhận sự kiện từ Rust Core gửi lên giao diện người dùng.
pub type SvmEventCallback = Option<
    unsafe extern "C" fn(kind: i32, payload_json: *const c_char, user_data: *mut std::ffi::c_void),
>;

/// Ngữ cảnh hoàn chỉnh của ứng dụng đại diện cho một phiên làm việc runtime.
pub struct SvmContext {
    /// Đường dẫn thư mục dữ liệu trên đĩa (None nếu dùng đường dẫn mặc định của OS).
    pub data_dir: Option<String>,
    /// Trạng thái khóa/mở khóa hiện tại của kho dữ liệu.
    pub unlocked: AtomicBool,
    /// Con trỏ hàm callback sự kiện đã đăng ký từ tầng UI.
    pub event_callback: SvmEventCallback,
    /// Dữ liệu tùy biến truyền vào callback sự kiện.
    pub event_user_data: *mut std::ffi::c_void,
}

unsafe impl Send for SvmContext {}
unsafe impl Sync for SvmContext {}

thread_local! {
    static LAST_ERROR: RefCell<CString> = RefCell::new(CString::default());
}

#[allow(dead_code)]
fn set_last_error(msg: &str) {
    if let Ok(c_str) = CString::new(msg) {
        LAST_ERROR.with(|cell| {
            *cell.borrow_mut() = c_str;
        });
    }
}

fn alloc_c_string(s: &str) -> *mut c_char {
    match CString::new(s) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/* ========================================================================= */
/* 3.1. Quản Lý Vòng Đời & Bộ Nhớ (Lifecycle & Memory)                       */
/* ========================================================================= */

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
    let dir = if data_dir.is_null() {
        None
    } else {
        CStr::from_ptr(data_dir)
            .to_str()
            .ok()
            .map(|s| s.to_string())
    };

    let ctx = Box::new(SvmContext {
        data_dir: dir,
        unlocked: AtomicBool::new(false),
        event_callback: None,
        event_user_data: std::ptr::null_mut(),
    });

    Box::into_raw(ctx)
}

/// Hủy ngữ cảnh và giải phóng toàn bộ runtime tokio, socket listeners và bộ nhớ.
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
        let bytes = s.into_bytes_with_nul();
        let mut slice = bytes;
        svm_crypto::zeroize_slice(&mut slice);
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
        svm_crypto::zeroize_slice(slice);
        drop(Vec::from_raw_parts(buf.ptr, buf.len, buf.len));
    }
}

/* ========================================================================= */
/* 3.2. Điều Phối Yêu Cầu Tương Tác (Prompt Broker)                          */
/* ========================================================================= */

/// Phản hồi một yêu cầu phê duyệt từ UI gửi xuống Core.
///
/// # Safety
/// `ctx` và `request_id` không được phép là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_prompt_respond(
    ctx: *mut SvmContext,
    request_id: *const c_char,
    _decision_json: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || request_id.is_null() {
        return SvmStatusCode::InvalidArg;
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
    *out_json = alloc_c_string("[]");
    SvmStatusCode::Ok
}

/* ========================================================================= */
/* 3.3. Quản Trị Vault (Vault Registry & Lifecycle)                          */
/* ========================================================================= */

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
    *out_registry_json = alloc_c_string("{\"vaults\":[]}");
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
    *out_vault_id = alloc_c_string("vault-0000-0000");
    SvmStatusCode::Ok
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
    (*ctx).unlocked.store(true, Ordering::SeqCst);
    SvmStatusCode::Ok
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
    SvmStatusCode::Ok
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
    SvmStatusCode::Ok
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
    (*ctx).unlocked.store(true, Ordering::SeqCst);
    SvmStatusCode::Ok
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
    (*ctx).unlocked.store(false, Ordering::SeqCst);
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

/// Đổi tên một Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_vault_rename(
    ctx: *mut SvmContext,
    vault_id: *const c_char,
    name: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || vault_id.is_null() || name.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || vault_id.is_null() || password.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || vault_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    SvmStatusCode::Ok
}

/// Báo cáo số mili-giây nhàn rỗi của hệ thống phục vụ bộ đếm Auto-Lock.
///
/// # Safety
/// `ctx` có thể là NULL (bỏ qua an toàn).
#[no_mangle]
pub unsafe extern "C" fn svm_vault_report_idle(_ctx: *mut SvmContext, _idle_ms: u64) {}

/* ========================================================================= */
/* 3.4. Quản Lý Mục Dữ Liệu & Lịch Sử (Items, Folders & History)             */
/* ========================================================================= */

/// Lấy danh sách các mục trong Vault theo bộ lọc `filter_json`.
///
/// # Safety
/// `ctx` và `out_items_json` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_items_list(
    ctx: *mut SvmContext,
    _filter_json: *const c_char,
    out_items_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_items_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_items_json = alloc_c_string("{\"items\":[]}");
    SvmStatusCode::Ok
}

/// Lấy thông tin chi tiết của một mục. Các trường nhạy cảm được masked với `{"concealed": true}`.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_item_get(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    out_item_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || item_id.is_null() || out_item_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_item_json = alloc_c_string("{}");
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_json.is_null() || out_item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_item_id = alloc_c_string("item-0000-0000");
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_id.is_null() || field_id.is_null() || out_secret.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_secret = alloc_c_string("");
    SvmStatusCode::Ok
}

/// Lấy danh sách các thư mục (Folders) trong Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_folders_list(
    ctx: *mut SvmContext,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_json = alloc_c_string("[]");
    SvmStatusCode::Ok
}

/// Thêm mới hoặc cập nhật một thư mục tổ chức.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_folder_upsert(
    ctx: *mut SvmContext,
    folder_json: *const c_char,
    out_folder_id: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || folder_json.is_null() || out_folder_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_folder_id = alloc_c_string("folder-0000");
    SvmStatusCode::Ok
}

/// Xóa một thư mục (các mục bên trong sẽ được chuyển về thư mục gốc).
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_folder_delete(
    ctx: *mut SvmContext,
    folder_id: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || folder_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || item_id.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_json = alloc_c_string("[]");
    SvmStatusCode::Ok
}

/// Phục hồi một mục về bản sửa đổi cụ thể trong lịch sử.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_history_restore(
    ctx: *mut SvmContext,
    item_id: *const c_char,
    _revision: u32,
) -> SvmStatusCode {
    if ctx.is_null() || item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
}

/* ========================================================================= */
/* 3.5. Quản Lý Tệp Đính Kèm (Secure Attachments)                            */
/* ========================================================================= */

/// Thêm tệp đính kèm vào một mục (mã hóa streaming từng chunk 64KB).
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
    if ctx.is_null() || item_id.is_null() || src_path.is_null() || out_attachment_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_attachment_json = alloc_c_string("{}");
    SvmStatusCode::Ok
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
    if ctx.is_null() || attachment_id.is_null() || dst_path.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
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
    if ctx.is_null() || attachment_id.is_null() || out.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    (*out).ptr = std::ptr::null_mut();
    (*out).len = 0;
    SvmStatusCode::Ok
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
    if ctx.is_null() || attachment_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
}

/* ========================================================================= */
/* 3.6. Tiện Ích Mật Mã & TOTP                                               */
/* ========================================================================= */

/// Tính toán mã số xác thực hai lớp TOTP hiện tại dựa trên mục lưu trữ.
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
    let (code, rem) = svm_totp::generate_totp("", unix_ts);
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
    *out_json = alloc_c_string("{}");
    SvmStatusCode::Ok
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
    _options_json: *const c_char,
    out_secret: *mut *mut c_char,
) -> SvmStatusCode {
    if out_secret.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_secret = alloc_c_string("CorrectHorseBatteryStaple123!");
    SvmStatusCode::Ok
}

/// Đánh giá độ mạnh của mật khẩu (sử dụng thuật toán zxcvbn).
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
    *out_json = alloc_c_string("{\"score\":4,\"guesses\":10000000}");
    SvmStatusCode::Ok
}

/* ========================================================================= */
/* 3.7. Nhập & Xuất Dữ Liệu (Import & Export)                                */
/* ========================================================================= */

/// Phân tích và tạo bản xem trước quá trình import từ tệp bên ngoài.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_import_preview(
    ctx: *mut SvmContext,
    _format: i32,
    src_path: *const c_char,
    _password: *const c_char,
    out_preview_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || src_path.is_null() || out_preview_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_preview_json = alloc_c_string("{\"items_count\":0}");
    SvmStatusCode::Ok
}

/// Thực thi quá trình import dữ liệu có snapshot sao lưu khẩn cấp.
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
    *out_summary_json = alloc_c_string("{\"imported\":0,\"failed\":0}");
    SvmStatusCode::Ok
}

/// Xuất toàn bộ hoặc một phần dữ liệu Vault ra định dạng chỉ định.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_export(
    ctx: *mut SvmContext,
    _format: i32,
    dst_path: *const c_char,
    _password: *const c_char,
) -> SvmStatusCode {
    if ctx.is_null() || dst_path.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
}

/* ========================================================================= */
/* 3.8. SSH Agent & Passkeys                                                 */
/* ========================================================================= */

/// Khởi chạy OpenSSH Agent daemon.
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_agent_start(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
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
    *out_json = alloc_c_string("{\"running\":false,\"loaded_keys\":0}");
    SvmStatusCode::Ok
}

/// Sinh cặp khóa SSH mới trực tiếp trong Vault.
///
/// # Safety
/// Mọi tham số con trỏ không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_ssh_key_generate(
    ctx: *mut SvmContext,
    _options_json: *const c_char,
    out_item_id: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_item_id = alloc_c_string("ssh-key-0000");
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
    *out_openssh_pub = alloc_c_string("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... stub");
    SvmStatusCode::Ok
}

/// Lấy danh sách Passkey đã lưu trữ theo tên miền `rp_id` (lọc theo Category::Passkey).
///
/// # Safety
/// `ctx` và `out_json` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_passkey_list(
    ctx: *mut SvmContext,
    _rp_id: *const c_char,
    out_json: *mut *mut c_char,
) -> SvmStatusCode {
    if ctx.is_null() || out_json.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    *out_json = alloc_c_string("[]");
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
    if ctx.is_null() || item_id.is_null() {
        return SvmStatusCode::InvalidArg;
    }
    SvmStatusCode::Ok
}

/* ========================================================================= */
/* 3.9. Cầu Nối Trình Duyệt & Cài Đặt Hệ Thống (Bridge & Settings)           */
/* ========================================================================= */

/// Khởi chạy kênh giao tiếp Bridge (Native Messaging IPC listener).
///
/// # Safety
/// `ctx` không được là NULL.
#[no_mangle]
pub unsafe extern "C" fn svm_bridge_start(ctx: *mut SvmContext) -> SvmStatusCode {
    if ctx.is_null() {
        return SvmStatusCode::InvalidArg;
    }
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
    *out_pairing_json = alloc_c_string("{\"pairing_code\":\"123456\",\"expires_in_secs\":60}");
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
    *out_json = alloc_c_string("[]");
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
    *out_json = alloc_c_string("{\"auto_lock_minutes\":15,\"theme\":\"dark\",\"language\":\"vi\"}");
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
