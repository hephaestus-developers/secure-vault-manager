//! # folder
//!
//! Thực thể thư mục tổ chức phân cấp các mục dữ liệu trong kho.

use crate::json::JsonValue;

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

impl Folder {
    /// Chuyển đổi `Folder` sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("name", JsonValue::String(self.name.clone()));
        if let Some(ref pid) = self.parent_id {
            obj.insert("parentId", JsonValue::String(pid.clone()));
        } else {
            obj.insert("parentId", JsonValue::Null);
        }
        obj
    }

    /// Khôi phục `Folder` từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let id = val.get_string("id").unwrap_or_default();
        let name = val.get_string("name")?;
        let parent_id = val
            .get("parentId")
            .or_else(|| val.get("parent_id"))
            .and_then(|v| v.as_str())
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string());
        Some(Self {
            id,
            name,
            parent_id,
        })
    }
}
