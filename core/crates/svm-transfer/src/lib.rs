//! # svm-transfer
//!
//! Mô-đun phân tích cú pháp tệp nhập và xuất dữ liệu tương thích với các trình quản lý
//! mật khẩu phổ biến (1Password `.1pux`, Bitwarden JSON/CSV, KeePass XML, và SVM native format).

/// Các định dạng tệp được hỗ trợ trong tính năng Nhập và Xuất dữ liệu.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransferFormat {
    /// Định dạng native nhị phân mã hóa của SVM (.svm).
    Svm = 1,
    /// Tệp xuất định dạng JSON của Bitwarden.
    BitwardenJson = 2,
    /// Container sao lưu 1PUX của 1Password.
    OnePux = 3,
    /// Tệp xuất XML của KeePass 2.x.
    KeePassXml = 4,
    /// Tệp bảng tính CSV chuẩn của các trình duyệt web.
    Csv = 5,
}
