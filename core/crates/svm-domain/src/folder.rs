//! # folder
//!
//! Thực thể thư mục tổ chức phân cấp các mục dữ liệu trong kho.

/// Thư mục tổ chức lưu trữ các mục trong kho theo mô hình cây phân cấp.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Folder {
    /// Định danh UUID duy nhất của thư mục.
    pub id: String,
    /// Tên hiển thị của thư mục.
    pub name: String,
    /// Định danh UUID của thư mục cha (hoặc None nếu nằm tại thư mục gốc).
    pub parent_id: Option<String>,
}
