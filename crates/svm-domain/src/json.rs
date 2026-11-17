//! # json
//!
//! Bộ phân tích cú pháp (Parser) và tuần tự hóa (Serializer) JSON thuần Rust (RFC 8259)
//! không phụ thuộc thư viện ngoài, đảm bảo biên dịch tức thì và an toàn tuyệt đối.

use std::fmt::Write;

/// Giá trị JSON đa kiểu theo chuẩn RFC 8259.
#[derive(Debug, Clone, PartialEq)]
pub enum JsonValue {
    /// Giá trị rỗng `null`.
    Null,
    /// Giá trị logic `true` hoặc `false`.
    Bool(bool),
    /// Giá trị số thực / số nguyên.
    Number(f64),
    /// Chuỗi ký tự UTF-8.
    String(String),
    /// Mảng danh sách các giá trị JSON.
    Array(Vec<JsonValue>),
    /// Đối tượng ánh xạ khóa - giá trị (giữ nguyên thứ tự chèn).
    Object(Vec<(String, JsonValue)>),
}

impl JsonValue {
    /// Khởi tạo một đối tượng JSON rỗng `{}`.
    pub fn new_object() -> Self {
        Self::Object(Vec::new())
    }

    /// Khởi tạo một mảng JSON rỗng `[]`.
    pub fn new_array() -> Self {
        Self::Array(Vec::new())
    }

    /// Đặt hoặc cập nhật một trường trong đối tượng JSON.
    pub fn insert(&mut self, key: impl Into<String>, val: JsonValue) {
        if let Self::Object(ref mut entries) = self {
            let k = key.into();
            for (existing_k, existing_v) in entries.iter_mut() {
                if existing_k == &k {
                    *existing_v = val;
                    return;
                }
            }
            entries.push((k, val));
        }
    }

    /// Thêm một phần tử vào mảng JSON.
    pub fn push(&mut self, val: JsonValue) {
        if let Self::Array(ref mut items) = self {
            items.push(val);
        }
    }

    /// Tra cứu giá trị theo khóa trong đối tượng JSON.
    pub fn get(&self, key: &str) -> Option<&JsonValue> {
        if let Self::Object(entries) = self {
            for (k, v) in entries {
                if k == key {
                    return Some(v);
                }
            }
        }
        None
    }

    /// Trích xuất chuỗi nếu giá trị là `JsonValue::String`.
    pub fn as_str(&self) -> Option<&str> {
        if let Self::String(s) = self {
            Some(s.as_str())
        } else {
            None
        }
    }

    /// Trích xuất chuỗi từ một khóa con, trả về chuỗi mặc định nếu không tồn tại.
    pub fn get_str_or<'a>(&'a self, key: &str, default: &'a str) -> &'a str {
        self.get(key).and_then(|v| v.as_str()).unwrap_or(default)
    }

    /// Trích xuất chuỗi `String` từ một khóa con.
    pub fn get_string(&self, key: &str) -> Option<String> {
        self.get(key)
            .and_then(|v| v.as_str())
            .map(|s| s.to_string())
    }

    /// Trích xuất giá trị boolean nếu là `JsonValue::Bool`.
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Self::Bool(b) => Some(*b),
            Self::String(s) => match s.as_str() {
                "true" | "1" => Some(true),
                "false" | "0" => Some(false),
                _ => None,
            },
            _ => None,
        }
    }

    /// Trích xuất giá trị boolean từ khóa con kèm mặc định.
    pub fn get_bool_or(&self, key: &str, default: bool) -> bool {
        self.get(key).and_then(|v| v.as_bool()).unwrap_or(default)
    }

    /// Trích xuất số nguyên `i64`.
    pub fn as_i64(&self) -> Option<i64> {
        match self {
            Self::Number(n) => Some(*n as i64),
            Self::String(s) => s.parse::<i64>().ok(),
            _ => None,
        }
    }

    /// Trích xuất số nguyên `i64` từ khóa con kèm mặc định.
    pub fn get_i64_or(&self, key: &str, default: i64) -> i64 {
        self.get(key).and_then(|v| v.as_i64()).unwrap_or(default)
    }

    /// Trích xuất danh sách mảng nếu là `JsonValue::Array`.
    pub fn as_array(&self) -> Option<&[JsonValue]> {
        if let Self::Array(arr) = self {
            Some(arr.as_slice())
        } else {
            None
        }
    }

    /// Trích xuất danh sách cặp khóa-giá trị nếu là `JsonValue::Object`.
    pub fn as_object(&self) -> Option<&[(String, JsonValue)]> {
        if let Self::Object(obj) = self {
            Some(obj.as_slice())
        } else {
            None
        }
    }

    /// Chuyển đổi giá trị JSON thành chuỗi văn bản chuẩn.
    pub fn to_json_string(&self) -> String {
        let mut out = String::new();
        self.write_json(&mut out);
        out
    }

    fn write_json(&self, out: &mut String) {
        match self {
            Self::Null => out.push_str("null"),
            Self::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
            Self::Number(n) => {
                if n.fract() == 0.0
                    && n.is_finite()
                    && *n >= (i64::MIN as f64)
                    && *n <= (i64::MAX as f64)
                {
                    let _ = write!(out, "{}", *n as i64);
                } else if n.is_finite() {
                    let _ = write!(out, "{n}");
                } else {
                    out.push('0');
                }
            }
            Self::String(s) => write_escaped_string(s, out),
            Self::Array(items) => {
                out.push('[');
                for (i, item) in items.iter().enumerate() {
                    if i > 0 {
                        out.push(',');
                    }
                    item.write_json(out);
                }
                out.push(']');
            }
            Self::Object(entries) => {
                out.push('{');
                for (i, (k, v)) in entries.iter().enumerate() {
                    if i > 0 {
                        out.push(',');
                    }
                    write_escaped_string(k, out);
                    out.push(':');
                    v.write_json(out);
                }
                out.push('}');
            }
        }
    }

    /// Phân tích cú pháp một chuỗi văn bản JSON thành `JsonValue`.
    pub fn parse(input: &str) -> Result<Self, String> {
        let mut parser = Parser::new(input.as_bytes());
        let val = parser.parse_value()?;
        parser.skip_whitespace();
        Ok(val)
    }
}

/// Escape một chuỗi thành định dạng JSON string hợp lệ (bao gồm dấu ngoặc kép).
pub fn write_escaped_string(s: &str, out: &mut String) {
    out.push('"');
    for ch in s.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\u{08}' => out.push_str("\\b"),
            '\u{0C}' => out.push_str("\\f"),
            c if (c as u32) < 0x20 => {
                let _ = write!(out, "\\u{:04x}", c as u32);
            }
            c => out.push(c),
        }
    }
    out.push('"');
}

struct Parser<'a> {
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(bytes: &'a [u8]) -> Self {
        Self { bytes, pos: 0 }
    }

    fn skip_whitespace(&mut self) {
        while self.pos < self.bytes.len() {
            match self.bytes[self.pos] {
                b' ' | b'\t' | b'\n' | b'\r' => self.pos += 1,
                _ => break,
            }
        }
    }

    fn peek(&mut self) -> Option<u8> {
        self.skip_whitespace();
        self.bytes.get(self.pos).copied()
    }

    fn parse_value(&mut self) -> Result<JsonValue, String> {
        match self.peek() {
            Some(b'{') => self.parse_object(),
            Some(b'[') => self.parse_array(),
            Some(b'"') => self.parse_string().map(JsonValue::String),
            Some(b't') | Some(b'f') => self.parse_bool(),
            Some(b'n') => self.parse_null(),
            Some(b'-') | Some(b'0'..=b'9') => self.parse_number(),
            Some(c) => Err(format!(
                "Unexpected character '{}' at pos {}",
                c as char, self.pos
            )),
            None => Err("Unexpected end of JSON input".to_string()),
        }
    }

    fn parse_object(&mut self) -> Result<JsonValue, String> {
        self.pos += 1; // '{'
        let mut entries = Vec::new();
        if self.peek() == Some(b'}') {
            self.pos += 1;
            return Ok(JsonValue::Object(entries));
        }
        loop {
            if self.peek() != Some(b'"') {
                return Err(format!("Expected string key at pos {}", self.pos));
            }
            let key = self.parse_string()?;
            if self.peek() != Some(b':') {
                return Err(format!("Expected ':' at pos {}", self.pos));
            }
            self.pos += 1;
            let val = self.parse_value()?;
            entries.push((key, val));
            match self.peek() {
                Some(b',') => {
                    self.pos += 1;
                    // Cho phép trailing comma nhẹ nhàng
                    if self.peek() == Some(b'}') {
                        self.pos += 1;
                        break;
                    }
                }
                Some(b'}') => {
                    self.pos += 1;
                    break;
                }
                _ => return Err(format!("Expected ',' or '}}' at pos {}", self.pos)),
            }
        }
        Ok(JsonValue::Object(entries))
    }

    fn parse_array(&mut self) -> Result<JsonValue, String> {
        self.pos += 1; // '['
        let mut items = Vec::new();
        if self.peek() == Some(b']') {
            self.pos += 1;
            return Ok(JsonValue::Array(items));
        }
        loop {
            let val = self.parse_value()?;
            items.push(val);
            match self.peek() {
                Some(b',') => {
                    self.pos += 1;
                    if self.peek() == Some(b']') {
                        self.pos += 1;
                        break;
                    }
                }
                Some(b']') => {
                    self.pos += 1;
                    break;
                }
                _ => return Err(format!("Expected ',' or ']' at pos {}", self.pos)),
            }
        }
        Ok(JsonValue::Array(items))
    }

    fn parse_string(&mut self) -> Result<String, String> {
        self.skip_whitespace();
        if self.bytes.get(self.pos) != Some(&b'"') {
            return Err("Expected '\"'".to_string());
        }
        self.pos += 1;
        let mut out = Vec::new();
        while self.pos < self.bytes.len() {
            let b = self.bytes[self.pos];
            self.pos += 1;
            if b == b'"' {
                return String::from_utf8(out).map_err(|e| e.to_string());
            } else if b == b'\\' {
                if self.pos >= self.bytes.len() {
                    return Err("Unterminated escape sequence".to_string());
                }
                let esc = self.bytes[self.pos];
                self.pos += 1;
                match esc {
                    b'"' => out.push(b'"'),
                    b'\\' => out.push(b'\\'),
                    b'/' => out.push(b'/'),
                    b'n' => out.push(b'\n'),
                    b'r' => out.push(b'\r'),
                    b't' => out.push(b'\t'),
                    b'b' => out.push(0x08),
                    b'f' => out.push(0x0C),
                    b'u' => {
                        if self.pos + 4 > self.bytes.len() {
                            return Err("Invalid unicode escape".to_string());
                        }
                        let hex_str = std::str::from_utf8(&self.bytes[self.pos..self.pos + 4])
                            .map_err(|_| "Invalid hex")?;
                        self.pos += 4;
                        let code =
                            u32::from_str_radix(hex_str, 16).map_err(|_| "Invalid hex digits")?;
                        if let Some(ch) = char::from_u32(code) {
                            let mut buf = [0u8; 4];
                            out.extend_from_slice(ch.encode_utf8(&mut buf).as_bytes());
                        }
                    }
                    other => out.push(other),
                }
            } else {
                out.push(b);
            }
        }
        Err("Unterminated string literal".to_string())
    }

    fn parse_bool(&mut self) -> Result<JsonValue, String> {
        self.skip_whitespace();
        if self.bytes[self.pos..].starts_with(b"true") {
            self.pos += 4;
            Ok(JsonValue::Bool(true))
        } else if self.bytes[self.pos..].starts_with(b"false") {
            self.pos += 5;
            Ok(JsonValue::Bool(false))
        } else {
            Err("Invalid boolean literal".to_string())
        }
    }

    fn parse_null(&mut self) -> Result<JsonValue, String> {
        self.skip_whitespace();
        if self.bytes[self.pos..].starts_with(b"null") {
            self.pos += 4;
            Ok(JsonValue::Null)
        } else {
            Err("Invalid null literal".to_string())
        }
    }

    fn parse_number(&mut self) -> Result<JsonValue, String> {
        self.skip_whitespace();
        let start = self.pos;
        while self.pos < self.bytes.len() {
            match self.bytes[self.pos] {
                b'0'..=b'9' | b'-' | b'+' | b'.' | b'e' | b'E' => self.pos += 1,
                _ => break,
            }
        }
        let num_str =
            std::str::from_utf8(&self.bytes[start..self.pos]).map_err(|_| "Invalid number")?;
        let n = num_str
            .parse::<f64>()
            .map_err(|_| format!("Invalid number '{num_str}'"))?;
        Ok(JsonValue::Number(n))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_json_roundtrip() {
        let mut obj = JsonValue::new_object();
        obj.insert("name", JsonValue::String("Personal Vault".to_string()));
        obj.insert("count", JsonValue::Number(42.0));
        obj.insert("active", JsonValue::Bool(true));

        let serialized = obj.to_json_string();
        let parsed = JsonValue::parse(&serialized).expect("should parse");
        assert_eq!(parsed.get_str_or("name", ""), "Personal Vault");
        assert_eq!(parsed.get_i64_or("count", 0), 42);
        assert!(parsed.get_bool_or("active", false));
    }
}
