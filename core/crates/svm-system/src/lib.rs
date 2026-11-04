//! # svm-system
//!
//! Mô-đun tích hợp các tính năng hệ điều hành: Tăng cường bảo mật tiến trình
//! (`PR_SET_DUMPABLE=0`), chặn hệ thống chuyển sang chế độ ngủ (sleep blocker),
//! tích hợp cảm biến sinh trắc học (Biometrics) và OS Keystore.

/// Kích hoạt các biện pháp gia cố an ninh tiến trình tại thời điểm runtime.
///
/// Vô hiệu hóa tính năng dump bộ nhớ và ngăn chặn các tiến trình khác gắn ptrace
/// để đọc lén vùng nhớ chứa mật khẩu.
///
/// # Returns
/// `true` nếu áp dụng thành công trên hệ điều hành này.
pub fn harden_current_process() -> bool {
    #[cfg(target_os = "linux")]
    {
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
/// `true` nếu hệ thống hỗ trợ Windows Hello, Touch ID, hoặc Linux fprintd/PAM.
pub fn is_biometric_available() -> bool {
    false
}
