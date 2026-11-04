//! # trash
//!
//! Thực thể quản lý các mục nằm trong Thùng rác (phục vụ tính năng phục hồi dữ liệu).

/// Đại diện cho một mục nằm trong Thùng rác kèm thông tin vị trí thư mục gốc trước khi xóa.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TrashItem {
    /// Định danh UUID của mục bị đưa vào thùng rác.
    pub item_id: String,
    /// Thư mục chứa mục trước khi bị xóa (dùng để khôi phục chính xác).
    pub original_folder_id: Option<String>,
    /// Dấu thời gian khi mục bị chuyển vào thùng rác (Unix timestamp).
    pub trashed_at: i64,
}
