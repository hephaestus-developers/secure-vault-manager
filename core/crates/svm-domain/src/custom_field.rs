//! # custom_field
//!
//! Định nghĩa cấu trúc trường tùy biến (Custom Fields) và phân loại kiểu dữ liệu.

/// Kiểu dữ liệu xác định cách hiển thị trên giao diện người dùng và cơ chế bảo vệ bộ nhớ.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FieldKind {
    /// Văn bản thông thường, hiển thị rõ ràng trên giao diện.
    Text,
    /// Trường bí mật (Mật khẩu phụ, PIN, Token), mặc định bị ẩn và cần quyền để mở xem.
    Concealed,
    /// Giá trị logic Đúng / Sai (Bật / Tắt tùy chọn).
    Boolean,
    /// Ngày tháng định dạng ISO 8601 (YYYY-MM-DD).
    Date,
}

/// Trường tùy biến do người dùng định nghĩa, có mặt trên toàn bộ 9 danh mục của `VaultItem`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CustomField {
    /// Định danh UUID duy nhất của trường tùy biến.
    pub id: String,
    /// Tên nhãn hiển thị do người dùng đặt (ví dụ: "Mã PIN phụ", "IP dự phòng").
    pub label: String,
    /// Giá trị lưu trữ dạng chuỗi.
    pub value: String,
    /// Phân loại kiểu hiển thị và bảo vệ bộ nhớ.
    pub kind: FieldKind,
}
