//! # error
//!
//! Định nghĩa lỗi miền nghiệp vụ và hàm chuyển đổi sang mã trạng thái FFI C-ABI.

use crate::status::SvmStatusCode;

/// Lỗi xảy ra trong các thao tác nghiệp vụ miền.
#[derive(Debug)]
pub enum DomainError {
    /// Tham số không hợp lệ kèm thông điệp mô tả.
    InvalidArg(String),
    /// Không tìm thấy thực thể theo định danh.
    NotFound(String),
    /// Trạng thái kho bị khóa.
    Locked,
    /// Lỗi nội bộ hệ thống.
    Internal(String),
}

impl DomainError {
    /// Ánh xạ lỗi miền thành mã trạng thái FFI C-ABI tương ứng.
    pub fn to_status_code(&self) -> SvmStatusCode {
        match self {
            Self::InvalidArg(_) => SvmStatusCode::InvalidArg,
            Self::NotFound(_) => SvmStatusCode::NotFound,
            Self::Locked => SvmStatusCode::Locked,
            Self::Internal(_) => SvmStatusCode::Panic,
        }
    }
}
