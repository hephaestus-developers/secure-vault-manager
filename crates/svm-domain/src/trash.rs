//! # trash
//!
//! Thực thể quản lý các mục nằm trong Thùng rác (phục vụ tính năng phục hồi dữ liệu).

use crate::json::JsonValue;

/// Đại diện cho một mục nằm trong Thùng rác.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TrashItem {
    /// Định danh UUID của mục bị đưa vào thùng rác.
    pub item_id: String,
    /// Dấu thời gian khi mục bị chuyển vào thùng rác (Unix timestamp).
    pub trashed_at: i64,
}

impl TrashItem {
    /// Chuyển đổi `TrashItem` sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("itemId", JsonValue::String(self.item_id.clone()));
        obj.insert("trashedAt", JsonValue::Number(self.trashed_at as f64));
        obj
    }

    /// Khôi phục `TrashItem` từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let item_id = val
            .get_string("itemId")
            .or_else(|| val.get_string("item_id"))?;
        let trashed_at = val.get_i64_or("trashedAt", val.get_i64_or("trashed_at", 0));
        Some(Self {
            item_id,
            trashed_at,
        })
    }
}
