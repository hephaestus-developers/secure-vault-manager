//! Phân tích cú pháp chuỗi URI `otpauth://totp/...` và cấu hình `OtpAuthConfig`.

use svm_crypto::base32_decode;
use svm_domain::{DomainError, JsonValue};

/// Cấu hình xác thực OTP được trích xuất từ URI `otpauth://`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OtpAuthConfig {
    /// Loại OTP ("totp" hoặc "hotp").
    pub kind: String,
    /// Nhãn hiển thị (Issuer:Account).
    pub label: String,
    /// Nhà phát hành (Issuer).
    pub issuer: String,
    /// Tên tài khoản (Account).
    pub account: String,
    /// Khóa bí mật mã hóa Base32.
    pub secret: String,
    /// Thuật toán HMAC ("SHA1", "SHA256").
    pub algorithm: String,
    /// Số chữ số của mã OTP (mặc định 6).
    pub digits: u32,
    /// Chu kỳ thời gian tính bằng giây (mặc định 30).
    pub period: u32,
}

impl OtpAuthConfig {
    /// Chuyển đổi sang đối tượng `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("kind", JsonValue::String(self.kind.clone()));
        obj.insert("label", JsonValue::String(self.label.clone()));
        obj.insert("issuer", JsonValue::String(self.issuer.clone()));
        obj.insert("account", JsonValue::String(self.account.clone()));
        obj.insert("secret", JsonValue::String(self.secret.clone()));
        obj.insert("algorithm", JsonValue::String(self.algorithm.clone()));
        obj.insert("digits", JsonValue::Number(self.digits as f64));
        obj.insert("period", JsonValue::Number(self.period as f64));
        obj
    }
}

/// Phân tích cú pháp chuỗi URI `otpauth://totp/...` chuẩn.
pub fn parse_otpauth_uri(uri: &str) -> Result<OtpAuthConfig, DomainError> {
    let trimmed = uri.trim();
    if !trimmed.starts_with("otpauth://") {
        // Nếu người dùng truyền trực tiếp chuỗi Base32 secret
        let clean_secret: String = trimmed
            .chars()
            .filter(|c| !c.is_whitespace() && *c != '-')
            .collect::<String>()
            .to_uppercase();
        if clean_secret.is_empty() {
            return Err(DomainError::InvalidArg("Empty TOTP secret".to_string()));
        }
        let _ = base32_decode(&clean_secret)?;
        return Ok(OtpAuthConfig {
            kind: "totp".to_string(),
            label: String::new(),
            issuer: String::new(),
            account: String::new(),
            secret: clean_secret,
            algorithm: "SHA1".to_string(),
            digits: 6,
            period: 30,
        });
    }

    let without_scheme = &trimmed["otpauth://".len()..];
    let (host_and_path, query) = match without_scheme.find('?') {
        Some(idx) => (&without_scheme[..idx], &without_scheme[idx + 1..]),
        None => (without_scheme, ""),
    };

    let (kind, raw_label) = match host_and_path.find('/') {
        Some(idx) => (&host_and_path[..idx], &host_and_path[idx + 1..]),
        None => (host_and_path, ""),
    };

    let decoded_label = url_decode(raw_label);
    let (mut issuer, account) = match decoded_label.find(':') {
        Some(idx) => (
            decoded_label[..idx].trim().to_string(),
            decoded_label[idx + 1..].trim().to_string(),
        ),
        None => (String::new(), decoded_label.clone()),
    };

    let mut secret = String::new();
    let mut algorithm = "SHA1".to_string();
    let mut digits = 6u32;
    let mut period = 30u32;

    for pair in query.split('&') {
        if pair.is_empty() {
            continue;
        }
        if let Some(eq_idx) = pair.find('=') {
            let k = pair[..eq_idx].to_ascii_lowercase();
            let v = url_decode(&pair[eq_idx + 1..]);
            match k.as_str() {
                "secret" => secret = v.to_uppercase(),
                "issuer" if !v.is_empty() => issuer = v,
                "algorithm" => algorithm = v.to_uppercase(),
                "digits" => {
                    if let Ok(d) = v.parse::<u32>() {
                        if d == 6 || d == 8 {
                            digits = d;
                        }
                    }
                }
                "period" => {
                    if let Ok(p) = v.parse::<u32>() {
                        if p > 0 {
                            period = p;
                        }
                    }
                }
                _ => {}
            }
        }
    }

    if secret.is_empty() {
        return Err(DomainError::Parse(
            "Missing 'secret' parameter in otpauth URI".to_string(),
        ));
    }

    Ok(OtpAuthConfig {
        kind: kind.to_ascii_lowercase(),
        label: decoded_label,
        issuer,
        account,
        secret,
        algorithm,
        digits,
        period,
    })
}

fn url_decode(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            if let Ok(hex_str) = std::str::from_utf8(&bytes[i + 1..i + 3]) {
                if let Ok(val) = u8::from_str_radix(hex_str, 16) {
                    out.push(val);
                    i += 3;
                    continue;
                }
            }
        } else if bytes[i] == b'+' {
            out.push(b' ');
            i += 1;
            continue;
        }
        out.push(bytes[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).to_string()
}
