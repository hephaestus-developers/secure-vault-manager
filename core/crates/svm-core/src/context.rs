//! Ngữ cảnh runtime `SvmContext`, bộ đệm nhị phân `SvmBuffer`, quản lý lỗi TLS và các hàm tiện ích C-String.

use std::cell::RefCell;
use std::ffi::{c_char, CStr, CString};
use std::sync::atomic::AtomicBool;
use std::sync::Mutex;

use svm_bridge::BridgeServer;
use svm_domain::status::SvmStatusCode;
use svm_domain::{DomainError, JsonValue, VaultItem};
use svm_ssh_agent::SshAgentServer;
use svm_vault::VaultService;
use svm_webauthn::WebAuthnAuthenticator;

pub(crate) const EVENT_VAULT_LOCKED: i32 = 1;
pub(crate) const EVENT_VAULT_UNLOCKED: i32 = 2;
pub(crate) const EVENT_ITEMS_CHANGED: i32 = 3;
pub(crate) const EVENT_BRIDGE_STATUS: i32 = 7;
pub(crate) const EVENT_SSH_STATUS: i32 = 8;
#[allow(dead_code)]
pub(crate) const EVENT_SHOW_WINDOW: i32 = 9;
pub(crate) const EVENT_BRIDGE_CLIENTS_CHANGED: i32 = 10;
pub(crate) const EVENT_BRIDGE_PAIRING_REQUESTED: i32 = 11;

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

pub(crate) struct CoreRuntime {
    pub(crate) vault_service: VaultService,
    pub(crate) ssh_agent: SshAgentServer,
    pub(crate) bridge: BridgeServer,
    pub(crate) webauthn: WebAuthnAuthenticator,
    pub(crate) import_previews: Vec<(String, Vec<VaultItem>)>,
    pub(crate) pending_prompts: Vec<(String, JsonValue)>,
}

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
    pub(crate) runtime: Mutex<CoreRuntime>,
}

unsafe impl Send for SvmContext {}
unsafe impl Sync for SvmContext {}

impl SvmContext {
    pub(crate) fn emit_event(&self, kind: i32, payload_json: &str) {
        if let Some(cb) = self.event_callback {
            if let Ok(c_str) = CString::new(payload_json) {
                unsafe {
                    cb(kind, c_str.as_ptr(), self.event_user_data);
                }
            }
        }
    }
}

thread_local! {
    pub(crate) static LAST_ERROR: RefCell<CString> = RefCell::new(CString::default());
}

pub(crate) fn set_last_error(msg: &str) {
    if let Ok(c_str) = CString::new(msg) {
        LAST_ERROR.with(|cell| {
            *cell.borrow_mut() = c_str;
        });
    }
}

pub(crate) fn handle_domain_err(err: DomainError) -> SvmStatusCode {
    set_last_error(err.message());
    err.to_status_code()
}

pub(crate) fn alloc_c_string(s: &str) -> *mut c_char {
    match CString::new(s) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

pub(crate) unsafe fn c_str_to_string(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        None
    } else {
        CStr::from_ptr(ptr).to_str().ok().map(|s| s.to_string())
    }
}
