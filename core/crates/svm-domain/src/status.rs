//! # status
//!
//! Định nghĩa mã trạng thái nhị phân (Status Codes) chuẩn C-ABI của Secure Vault Manager.

/// Bảng mã lỗi nhị phân chuẩn C-ABI (32-bit signed integer).
///
/// Tương thích 100% với enum `SvmStatus` trong tệp tiêu đề `svm_core.h`.
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SvmStatusCode {
    /// Thao tác hoàn thành thành công.
    Ok = 0,
    /// Tham số đầu vào không hợp lệ hoặc con trỏ rỗng (NULL).
    InvalidArg = 1,
    /// Kho dữ liệu đang bị khóa; yêu cầu mở khóa trước khi truy cập.
    Locked = 2,
    /// Mật khẩu Master Password hoặc thông tin xác thực không chính xác.
    WrongPassword = 3,
    /// Lỗi nhập/xuất hệ thống tệp hoặc socket IPC.
    Io = 4,
    /// Lỗi mật mã học (giải mã thất bại, sai Auth Tag).
    Crypto = 5,
    /// Không tìm thấy mục dữ liệu, thư mục hoặc hồ sơ chỉ định.
    NotFound = 6,
    /// Lỗi phân tích cú pháp định dạng tệp (JSON, CBOR, XML).
    Parse = 7,
    /// Lỗi kết nối mạng (khi tải Favicon).
    Network = 8,
    /// Quyền truy cập bị từ chối hoặc người dùng bấm Deny trên UI.
    Denied = 9,
    /// Hết thời gian chờ phản hồi phê duyệt (Prompt timeout).
    Timeout = 10,
    /// Xung đột dữ liệu hoặc định danh trùng lặp.
    Conflict = 11,
    /// Tính năng hoặc định dạng không được hỗ trợ trên nền tảng hiện tại.
    Unsupported = 12,
    /// Tác vụ bị hủy bỏ trước khi hoàn tất.
    Cancelled = 13,
    /// Lỗi ngoại lệ nội bộ Rust (được chặn tại ranh giới FFI bằng catch_unwind).
    Panic = 99,
}
