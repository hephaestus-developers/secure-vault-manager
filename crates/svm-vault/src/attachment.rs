//! Quản lý siêu dữ liệu và tệp đính kèm mã hóa (Encrypted Attachments) của các mục trong kho.

use svm_domain::JsonValue;

/// Metadata của tệp đính kèm được mã hóa trong kho.
#[derive(Debug, Clone)]
pub struct AttachmentMeta {
    /// Định danh UUID của tệp đính kèm.
    pub id: String,
    /// Định danh mục chứa tệp đính kèm.
    pub item_id: String,
    /// Tên tệp gốc.
    pub file_name: String,
    /// Kích thước tệp tính bằng bytes.
    pub size_bytes: u64,
    /// Dấu thời gian tạo (Unix ms).
    pub created_at: i64,
}

impl AttachmentMeta {
    /// Chuyển đổi sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("itemId", JsonValue::String(self.item_id.clone()));
        obj.insert("fileName", JsonValue::String(self.file_name.clone()));
        obj.insert("sizeBytes", JsonValue::Number(self.size_bytes as f64));
        obj.insert("createdAt", JsonValue::Number(self.created_at as f64));
        obj
    }

    /// Khôi phục từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        Some(Self {
            id: val.get_string("id")?,
            item_id: val.get_string("itemId").unwrap_or_default(),
            file_name: val.get_string("fileName").unwrap_or_default(),
            size_bytes: val.get_i64_or("sizeBytes", 0) as u64,
            created_at: val.get_i64_or("createdAt", 0),
        })
    }
}
