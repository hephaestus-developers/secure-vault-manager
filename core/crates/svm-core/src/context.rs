//! Ngữ cảnh runtime `SvmContext`, bộ đệm nhị phân `SvmBuffer`, quản lý lỗi TLS và các hàm tiện ích C-String.

use std::cell::RefCell;
use std::ffi::{c_char, CStr, CString};
use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, Instant};

use svm_bridge::BridgeServer;
use svm_crypto::generate_uuid_v4;
use svm_domain::status::SvmStatusCode;
use svm_domain::{DomainError, JsonValue, VaultItem};
use svm_ssh_agent::SshAgentServer;
use svm_vault::VaultService;

pub(crate) const EVENT_VAULT_LOCKED: i32 = 1;
pub(crate) const EVENT_VAULT_UNLOCKED: i32 = 2;
pub(crate) const EVENT_ITEMS_CHANGED: i32 = 3;
pub(crate) const EVENT_PROMPT: i32 = 4;
pub(crate) const EVENT_PROMPT_CANCELLED: i32 = 5;
pub(crate) const EVENT_BRIDGE_STATUS: i32 = 7;
pub(crate) const EVENT_SSH_STATUS: i32 = 8;
pub(crate) const EVENT_BRIDGE_CLIENTS_CHANGED: i32 = 10;
pub(crate) const EVENT_SETTINGS_CHANGED: i32 = 11;

pub(crate) struct PendingPromptEntry {
    pub(crate) waiter: Arc<(Mutex<Option<bool>>, Condvar)>,
}

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
    pub(crate) import_previews: Vec<(String, Vec<VaultItem>)>,
    pub(crate) pending_prompts: Vec<(String, PendingPromptEntry)>,
}

pub(crate) unsafe fn sync_ssh_agent(ctx: *mut SvmContext) {
    if ctx.is_null() {
        return;
    }
    if let Ok(rt) = (*ctx).runtime.lock() {
        rt.ssh_agent.sync_keys(
            rt.vault_service.is_unlocked(),
            rt.vault_service.items(),
            rt.vault_service.ssh_prompt_approval(),
        );
    }
}

pub(crate) fn request_ssh_approval(ctx_raw: usize, key_comment: &str, fingerprint: &str) -> bool {
    let ctx = ctx_raw as *const SvmContext;
    if ctx.is_null() {
        return false;
    }
    let has_cb = unsafe { (*ctx).event_callback.is_some() };
    if !has_cb {
        return false;
    }

    let req_id = format!("prm-{}", generate_uuid_v4());
    let mut info = JsonValue::new_object();
    info.insert("request_id", JsonValue::String(req_id.clone()));
    info.insert("kind", JsonValue::String("ssh_sign".to_string()));
    info.insert(
        "title",
        JsonValue::String("SSH Signing Request".to_string()),
    );
    info.insert("key", JsonValue::String(key_comment.to_string()));
    info.insert("fingerprint", JsonValue::String(fingerprint.to_string()));
    let info_json = info.to_json_string();

    let waiter = Arc::new((Mutex::new(None), Condvar::new()));
    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
        rt.pending_prompts.push((
            req_id.clone(),
            PendingPromptEntry {
                waiter: Arc::clone(&waiter),
            },
        ));
    } else {
        return false;
    }

    unsafe {
        (*ctx).emit_event(EVENT_PROMPT, &info_json);
    }

    let (lock, cvar) = &*waiter;
    let deadline = Instant::now() + Duration::from_secs(30);
    let mut guard = lock.lock().unwrap_or_else(|e| e.into_inner());
    while guard.is_none() {
        let now = Instant::now();
        if now >= deadline {
            break;
        }
        let res = cvar
            .wait_timeout(guard, deadline - now)
            .unwrap_or_else(|e| e.into_inner());
        guard = res.0;
        if res.1.timed_out() {
            break;
        }
    }

    let approved = guard.unwrap_or(false);
    drop(guard);

    if let Ok(mut rt) = unsafe { (*ctx).runtime.lock() } {
        let had = rt.pending_prompts.iter().any(|(id, _)| id == &req_id);
        rt.pending_prompts.retain(|(id, _)| id != &req_id);
        if had {
            let mut cancel_obj = JsonValue::new_object();
            cancel_obj.insert("request_id", JsonValue::String(req_id));
            unsafe {
                (*ctx).emit_event(EVENT_PROMPT_CANCELLED, &cancel_obj.to_json_string());
            }
        }
    }

    approved
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

pub(crate) fn ffi_guard<F>(f: F) -> SvmStatusCode
where
    F: FnOnce() -> SvmStatusCode,
{
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
        Ok(code) => code,
        Err(_) => {
            set_last_error("Unexpected internal panic in Rust Core");
            SvmStatusCode::Panic
        }
    }
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
