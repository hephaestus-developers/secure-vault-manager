//! # item
//!
//! Thực thể cốt lõi `VaultItem` đại diện cho một mục dữ liệu bí mật trong kho.

use crate::category::Category;
use crate::custom_field::CustomField;

/// Thực thể mục kho dữ liệu cơ bản chung cho toàn bộ 9 danh mục nghiệp vụ.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VaultItem {
    /// Định danh UUID phiên bản 4 duy nhất của mục.
    pub id: String,
    /// Tiêu đề hoặc tên gợi nhớ của mục dữ liệu.
    pub title: String,
    /// Danh mục chuyên biệt thuộc @ref Category.
    pub category: Category,
    /// Thư mục chứa mục này (None nếu ở thư mục gốc).
    pub folder_id: Option<String>,
    /// Cờ đánh dấu mục yêu thích (Favorite/Pinned).
    pub favorite: bool,
    /// Cờ đánh dấu mục đã bị chuyển vào Thùng rác (xóa mềm).
    pub trashed: bool,
    /// Danh sách các trường tùy biến do người dùng mở rộng.
    pub custom_fields: Vec<CustomField>,
    /// Dấu thời gian tạo mục (Unix timestamp tính bằng mili-giây).
    pub created_at: i64,
    /// Dấu thời gian cập nhật gần nhất (Unix timestamp tính bằng mili-giây).
    pub updated_at: i64,
}
