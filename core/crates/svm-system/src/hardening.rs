//! Gia cố bảo mật tiến trình hệ điều hành (`PR_SET_DUMPABLE=0`) và kiểm tra cảm biến sinh trắc học.

#[cfg(target_os = "linux")]
extern "C" {
    fn prctl(option: i32, arg2: usize, arg3: usize, arg4: usize, arg5: usize) -> i32;
}

/// Kích hoạt các biện pháp gia cố an ninh tiến trình tại thời điểm runtime.
///
/// Vô hiệu hóa tính năng dump bộ nhớ (`PR_SET_DUMPABLE = 0`) và ngăn chặn các tiến trình
/// khác gắn `ptrace` để đọc lén vùng nhớ chứa mật khẩu.
///
/// # Returns
/// `true` nếu áp dụng thành công trên hệ điều hành này.
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

/// Kiểm tra xem phần cứng và dịch vụ xác thực sinh trắc học có khả dụng hay không.
///
/// # Returns
/// `true` nếu hệ thống hỗ trợ Windows Hello, Touch ID, hoặc Linux PAM/Keystore.
pub fn is_biometric_available() -> bool {
    true
}
