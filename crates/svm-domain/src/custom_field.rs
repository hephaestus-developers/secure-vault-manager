//! # custom_field
//!
//! Định nghĩa cấu trúc trường tùy biến (Custom Fields) và phân loại kiểu dữ liệu.

use crate::json::JsonValue;

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

impl FieldKind {
    /// Trả về chuỗi định danh của `FieldKind`.
    pub const fn as_str(&self) -> &'static str {
        match self {
            Self::Text => "text",
            Self::Concealed => "concealed",
            Self::Boolean => "boolean",
            Self::Date => "date",
        }
    }

    /// Phân tích chuỗi hoặc mã số nguyên thành `FieldKind`.
    pub fn parse_str(s: &str) -> Self {
        match s.trim().to_ascii_lowercase().as_str() {
            "concealed" | "password" | "secret" | "hidden" | "1" => Self::Concealed,
            "boolean" | "bool" | "2" => Self::Boolean,
            "date" | "3" => Self::Date,
            _ => Self::Text,
        }
    }
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

impl CustomField {
    /// Chuyển đổi `CustomField` sang đối tượng `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("label", JsonValue::String(self.label.clone()));
        obj.insert("value", JsonValue::String(self.value.clone()));
        obj.insert("kind", JsonValue::String(self.kind.as_str().to_string()));
        obj
    }

    /// Khôi phục `CustomField` từ đối tượng `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let label = val.get_string("label").unwrap_or_default();
        let id = val
            .get_string("id")
            .unwrap_or_else(|| format!("cf-{}", label.len()));
        let value = val.get_string("value").unwrap_or_default();
        let kind_str = val.get_str_or("kind", "text");
        Some(Self {
            id,
            label,
            value,
            kind: FieldKind::parse_str(kind_str),
        })
    }
}
