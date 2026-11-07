//! # trash
//!
//! Thực thể quản lý các mục nằm trong Thùng rác (phục vụ tính năng phục hồi dữ liệu).

use crate::json::JsonValue;

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

impl TrashItem {
    /// Chuyển đổi `TrashItem` sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("itemId", JsonValue::String(self.item_id.clone()));
        if let Some(ref fid) = self.original_folder_id {
            obj.insert("originalFolderId", JsonValue::String(fid.clone()));
        } else {
            obj.insert("originalFolderId", JsonValue::Null);
        }
        obj.insert("trashedAt", JsonValue::Number(self.trashed_at as f64));
        obj
    }

    /// Khôi phục `TrashItem` từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let item_id = val
            .get_string("itemId")
            .or_else(|| val.get_string("item_id"))?;
        let original_folder_id = val
            .get("originalFolderId")
            .or_else(|| val.get("original_folder_id"))
            .and_then(|v| v.as_str())
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string());
        let trashed_at = val.get_i64_or("trashedAt", val.get_i64_or("trashed_at", 0));
        Some(Self {
            item_id,
            original_folder_id,
            trashed_at,
        })
    }
}
