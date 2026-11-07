//! # error
//!
//! Định nghĩa lỗi miền nghiệp vụ và hàm chuyển đổi sang mã trạng thái FFI C-ABI.

use crate::status::SvmStatusCode;

/// Lỗi xảy ra trong các thao tác nghiệp vụ miền.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DomainError {
    /// Tham số không hợp lệ kèm thông điệp mô tả.
    InvalidArg(String),
    /// Không tìm thấy thực thể theo định danh.
    NotFound(String),
    /// Trạng thái kho bị khóa.
    Locked,
    /// Mật khẩu hoặc khóa khôi phục không chính xác.
    WrongPassword,
    /// Lỗi mật mã học (giải mã thất bại, sai Auth Tag).
    Crypto(String),
    /// Lỗi đọc/ghi hệ thống tệp.
    Io(String),
    /// Lỗi phân tích cú pháp dữ liệu.
    Parse(String),
    /// Xung đột dữ liệu hoặc vi phạm ràng buộc.
    Conflict(String),
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
            Self::WrongPassword => SvmStatusCode::WrongPassword,
            Self::Crypto(_) => SvmStatusCode::Crypto,
            Self::Io(_) => SvmStatusCode::Io,
            Self::Parse(_) => SvmStatusCode::Parse,
            Self::Conflict(_) => SvmStatusCode::Conflict,
            Self::Internal(_) => SvmStatusCode::Panic,
        }
    }

    /// Trả về thông điệp mô tả chi tiết lỗi.
    pub fn message(&self) -> &str {
        match self {
            Self::InvalidArg(m)
            | Self::NotFound(m)
            | Self::Crypto(m)
            | Self::Io(m)
            | Self::Parse(m)
            | Self::Conflict(m)
            | Self::Internal(m) => m.as_str(),
            Self::Locked => "Vault is currently locked",
            Self::WrongPassword => "Incorrect master password or recovery key",
        }
    }
}
