//! Mô hình dữ liệu của một trình duyệt / Tiện ích mở rộng đã ghép nối (`PairedClient`).

use svm_domain::JsonValue;

/// Thông tin một trình duyệt / Extension đã ghép nối với ứng dụng Desktop.
#[derive(Debug, Clone)]
pub struct PairedClient {
    /// Định danh duy nhất của client.
    pub id: String,
    /// Tên trình duyệt (ví dụ: "Google Chrome", "Mozilla Firefox").
    pub browser: String,
    /// Phiên bản Extension.
    pub version: String,
    /// Chuỗi mô tả lần hoạt động gần nhất.
    pub last_used: String,
}

impl PairedClient {
    /// Chuyển đổi sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("browser", JsonValue::String(self.browser.clone()));
        obj.insert("version", JsonValue::String(self.version.clone()));
        obj.insert("lastUsed", JsonValue::String(self.last_used.clone()));
        obj
    }

    /// Khôi phục từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        Some(Self {
            id: val.get_string("id")?,
            browser: val.get_string("browser").unwrap_or_default(),
            version: val.get_string("version").unwrap_or_default(),
            last_used: val.get_string("lastUsed").unwrap_or_default(),
        })
    }
}
