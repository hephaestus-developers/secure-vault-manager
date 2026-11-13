//! Gia cố bảo mật tiến trình hệ điều hành (`PR_SET_DUMPABLE=0` / `PT_DENY_ATTACH`),
//! kiểm tra và thực thi xác thực sinh trắc học / hệ điều hành thật.

#[cfg(target_os = "linux")]
use std::path::Path;
#[cfg(any(target_os = "linux", target_os = "macos"))]
use std::process::{Command, Stdio};

#[cfg(target_os = "linux")]
extern "C" {
    fn prctl(option: i32, arg2: usize, arg3: usize, arg4: usize, arg5: usize) -> i32;
}

#[cfg(target_os = "macos")]
extern "C" {
    fn ptrace(request: i32, pid: i32, addr: *mut core::ffi::c_void, data: i32) -> i32;
}

/// Kích hoạt các biện pháp gia cố an ninh tiến trình tại thời điểm runtime.
///
/// Vô hiệu hóa tính năng dump bộ nhớ (`PR_SET_DUMPABLE = 0` trên Linux, `PT_DENY_ATTACH`
/// trên macOS) để ngăn các tiến trình khác gắn debugger đọc lén vùng nhớ chứa mật khẩu.
pub fn harden_current_process() -> bool {
    #[cfg(target_os = "linux")]
    unsafe {
        const PR_SET_DUMPABLE: i32 = 4;
        let _ = prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
        true
    }
    #[cfg(target_os = "macos")]
    unsafe {
        const PT_DENY_ATTACH: i32 = 31;
        let _ = ptrace(PT_DENY_ATTACH, 0, core::ptr::null_mut(), 0);
        true
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    {
        true
    }
}

#[cfg(target_os = "linux")]
fn find_in_path(bin: &str) -> bool {
    if let Ok(path_var) = std::env::var("PATH") {
        for dir in std::env::split_paths(&path_var) {
            if dir.join(bin).is_file() {
                return true;
            }
        }
    }
    Path::new("/usr/bin").join(bin).is_file()
}

#[cfg(target_os = "linux")]
fn current_username() -> Option<String> {
    std::env::var("USER")
        .ok()
        .filter(|s| !s.trim().is_empty())
        .or_else(|| {
            std::env::var("LOGNAME")
                .ok()
                .filter(|s| !s.trim().is_empty())
        })
}

#[cfg(target_os = "linux")]
fn has_enrolled_fingerprint() -> bool {
    if !find_in_path("fprintd-verify") || !find_in_path("fprintd-list") {
        return false;
    }
    let Some(user) = current_username() else {
        return false;
    };
    let Ok(out) = Command::new("fprintd-list")
        .arg(&user)
        .stdin(Stdio::null())
        .stderr(Stdio::null())
        .output()
    else {
        return false;
    };
    if !out.status.success() {
        return false;
    }
    let stdout = String::from_utf8_lossy(&out.stdout).to_lowercase();
    !stdout.contains("no fingers enrolled") && stdout.contains("finger")
}

#[cfg(target_os = "linux")]
fn has_polkit_gui_session() -> bool {
    let has_display = std::env::var_os("WAYLAND_DISPLAY").is_some_and(|v| !v.is_empty())
        || std::env::var_os("DISPLAY").is_some_and(|v| !v.is_empty());
    has_display && find_in_path("pkexec")
}

/// Kiểm tra xem phần cứng sinh trắc học hoặc dịch vụ xác thực hệ điều hành tương tác
/// (fprintd / Polkit GUI trên Linux, Touch ID / LocalAuthentication trên macOS,
/// Windows Hello / CredUI trên Windows) có khả dụng hay không.
pub fn is_biometric_available() -> bool {
    #[cfg(target_os = "linux")]
    {
        (has_enrolled_fingerprint() || has_polkit_gui_session())
            && crate::keystore::is_os_keystore_available()
    }
    #[cfg(target_os = "macos")]
    {
        let has_touch_id = Command::new("bioutil")
            .arg("-r")
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status()
            .is_ok_and(|s| s.success());
        (has_touch_id
            || std::path::Path::new("/usr/bin/swift").is_file()
            || std::path::Path::new("/usr/bin/osascript").is_file())
            && crate::keystore::is_os_keystore_available()
    }
    #[cfg(target_os = "windows")]
    {
        crate::keystore::is_os_keystore_available()
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
    {
        false
    }
}

/// Yêu cầu người dùng xác thực danh tính với hệ điều hành trước khi cho phép mở khóa kho.
pub fn authenticate_os_user(reason: &str) -> bool {
    #[cfg(target_os = "linux")]
    {
        let _ = reason;
        if has_enrolled_fingerprint() {
            if let Ok(status) = Command::new("fprintd-verify")
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .status()
            {
                if status.success() {
                    return true;
                }
            }
            // Fallback sang Polkit nếu fprintd-verify thất bại hoặc cảm biến bị hủy
        }

        if has_polkit_gui_session() {
            let mut cmd = Command::new("pkexec");
            if let Some(user) = current_username() {
                cmd.arg("--user").arg(user);
            }
            cmd.arg("/bin/true")
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::null());
            if let Ok(status) = cmd.status() {
                return status.success();
            }
        }

        false
    }
    #[cfg(target_os = "macos")]
    {
        let safe_reason = reason.replace(['\\', '"', '\n', '\r'], " ");
        // Thử LocalAuthentication (Touch ID / Apple Watch / macOS user password) qua swift
        if std::path::Path::new("/usr/bin/swift").is_file() {
            let script = format!(
                "import LocalAuthentication\n\
                 import Foundation\n\
                 let ctx = LAContext()\n\
                 let sema = DispatchSemaphore(value: 0)\n\
                 var ok = false\n\
                 var err: NSError?\n\
                 if ctx.canEvaluatePolicy(.deviceOwnerAuthentication, error: &err) {{\n\
                     ctx.evaluatePolicy(.deviceOwnerAuthentication, localizedReason: \"{safe_reason}\") {{ res, _ in\n\
                         ok = res\n\
                         sema.signal()\n\
                     }}\n\
                     sema.wait()\n\
                 }}\n\
                 exit(ok ? 0 : 1)"
            );
            if let Ok(status) = Command::new("/usr/bin/swift")
                .arg("-e")
                .arg(script)
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .status()
            {
                return status.success();
            }
        }
        false
    }
    #[cfg(target_os = "windows")]
    {
        use windows_sys::Win32::Foundation::ERROR_SUCCESS;
        use windows_sys::Win32::Security::Credentials::{
            CredUIPromptForWindowsCredentialsW, CREDUIWIN_GENERIC, CREDUI_INFOW,
        };

        let mut caption: Vec<u16> = "Secure Vault Manager"
            .encode_utf16()
            .chain(std::iter::once(0))
            .collect();
        let msg_text = if reason.trim().is_empty() {
            "Authenticate to unlock Secure Vault Manager"
        } else {
            reason
        };
        let mut message: Vec<u16> = msg_text.encode_utf16().chain(std::iter::once(0)).collect();

        let mut info: CREDUI_INFOW = unsafe { core::mem::zeroed() };
        info.cbSize = core::mem::size_of::<CREDUI_INFOW>() as u32;
        info.pszCaptionText = caption.as_mut_ptr();
        info.pszMessageText = message.as_mut_ptr();

        let mut auth_package: u32 = 0;
        let mut out_buf: *mut core::ffi::c_void = core::ptr::null_mut();
        let mut out_size: u32 = 0;
        let mut save: i32 = 0;

        let rc = unsafe {
            CredUIPromptForWindowsCredentialsW(
                &info,
                0,
                &mut auth_package,
                core::ptr::null(),
                0,
                &mut out_buf,
                &mut out_size,
                &mut save,
                CREDUIWIN_GENERIC,
            )
        };
        if !out_buf.is_null() && out_size > 0 {
            unsafe {
                core::ptr::write_bytes(out_buf as *mut u8, 0, out_size as usize);
            }
        }
        rc == ERROR_SUCCESS
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
    {
        let _ = reason;
        false
    }
}
