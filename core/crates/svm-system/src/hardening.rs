//! Gia cố bảo mật tiến trình hệ điều hành (`PR_SET_DUMPABLE=0`), kiểm tra và thực thi
//! xác thực sinh trắc học / hệ điều hành thật.

#[cfg(target_os = "linux")]
use std::path::Path;
#[cfg(target_os = "linux")]
use std::process::{Command, Stdio};

#[cfg(target_os = "linux")]
extern "C" {
    fn prctl(option: i32, arg2: usize, arg3: usize, arg4: usize, arg5: usize) -> i32;
}

/// Kích hoạt các biện pháp gia cố an ninh tiến trình tại thời điểm runtime.
///
/// Vô hiệu hóa tính năng dump bộ nhớ (`PR_SET_DUMPABLE = 0`) và ngăn chặn các tiến trình
/// khác gắn `ptrace` để đọc lén vùng nhớ chứa mật khẩu.
pub fn harden_current_process() -> bool {
    #[cfg(target_os = "linux")]
    unsafe {
        const PR_SET_DUMPABLE: i32 = 4;
        let _ = prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
        true
    }
    #[cfg(not(target_os = "linux"))]
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
/// (fprintd / Polkit GUI) có khả dụng hay không.
pub fn is_biometric_available() -> bool {
    #[cfg(target_os = "linux")]
    {
        has_enrolled_fingerprint() || has_polkit_gui_session()
    }
    #[cfg(not(target_os = "linux"))]
    {
        false
    }
}

/// Yêu cầu người dùng xác thực danh tính với hệ điều hành (Vân tay `fprintd-verify`
/// hoặc hộp thoại xác thực Polkit `pkexec`) trước khi cho phép mở khóa kho.
pub fn authenticate_os_user(_reason: &str) -> bool {
    #[cfg(target_os = "linux")]
    {
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
            return false;
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
    #[cfg(not(target_os = "linux"))]
    {
        false
    }
}
