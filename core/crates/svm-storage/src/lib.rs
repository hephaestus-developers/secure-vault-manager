//! # svm-storage
//!
//! Mô-đun quản lý lưu trữ tệp nhị phân `vault.svm`, tệp companion sinh trắc học
//! `biometric.env`, atomic write `.tmp -> rename`, file lock OS và tệp đính kèm streaming.

/// Bộ quản lý hệ thống tệp lưu trữ của kho dữ liệu.
pub struct VaultStorage {
    /// Đường dẫn thư mục gốc chứa các tệp kho dữ liệu.
    pub base_path: String,
}

impl VaultStorage {
    /// Khởi tạo một đối tượng lưu trữ mới với đường dẫn chỉ định.
    ///
    /// # Arguments
    /// * `base_path` - Đường dẫn thư mục dữ liệu trên hệ điều hành.
    pub fn new(base_path: impl Into<String>) -> Self {
        Self {
            base_path: base_path.into(),
        }
    }
}
