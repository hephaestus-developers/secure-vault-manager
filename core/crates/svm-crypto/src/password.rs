//! Trình sinh mật khẩu ngẫu nhiên mật mã học và bộ đánh giá độ mạnh mật khẩu (Entropy / zxcvbn).

use svm_domain::JsonValue;

use crate::mem::random_bytes;

/// Sinh mật khẩu ngẫu nhiên mật mã học theo tùy chọn JSON.
pub fn generate_password(options_json: &str) -> String {
    let opts = JsonValue::parse(options_json).unwrap_or_else(|_| JsonValue::new_object());
    let length = (opts.get_i64_or("length", 20) as usize).clamp(6, 128);
    let use_upper = opts.get_bool_or("uppercase", true);
    let use_lower = opts.get_bool_or("lowercase", true);
    let use_digits = opts.get_bool_or("digits", true);
    let use_symbols = opts.get_bool_or("symbols", true);

    let mut charset = Vec::new();
    if use_lower {
        charset.extend_from_slice(b"abcdefghijkmnopqrstuvwxyz");
    }
    if use_upper {
        charset.extend_from_slice(b"ABCDEFGHJKLMNPQRSTUVWXYZ");
    }
    if use_digits {
        charset.extend_from_slice(b"23456789");
    }
    if use_symbols {
        charset.extend_from_slice(b"!@#$%^&*()-_=+[]{}?");
    }
    if charset.is_empty() {
        charset.extend_from_slice(b"abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    }

    let mut rnd = vec![0u8; length];
    random_bytes(&mut rnd);
    let mut out = String::with_capacity(length);
    for b in rnd {
        let idx = (b as usize) % charset.len();
        out.push(charset[idx] as char);
    }
    out
}

/// Kết quả đánh giá độ mạnh mật khẩu.
#[derive(Debug, Clone)]
pub struct PasswordStrength {
    /// Điểm chuẩn zxcvbn (0..=4).
    pub score: u8,
    /// Điểm phần trăm (0..=100) dùng cho Watchtower.
    pub percent: i32,
    /// Ước lượng số lần thử vét cạn.
    pub guesses: u64,
    /// Gợi ý cải thiện độ mạnh mật khẩu.
    pub feedback: String,
}

impl PasswordStrength {
    /// Chuyển đổi sang chuỗi JSON.
    pub fn to_json_string(&self) -> String {
        let mut obj = JsonValue::new_object();
        obj.insert("score", JsonValue::Number(self.score as f64));
        obj.insert("percent", JsonValue::Number(self.percent as f64));
        obj.insert("guesses", JsonValue::Number(self.guesses as f64));
        obj.insert("feedback", JsonValue::String(self.feedback.clone()));
        obj.to_json_string()
    }
}

/// Đánh giá độ mạnh của mật khẩu dựa trên độ dài, độ đa dạng ký tự và mẫu lặp.
pub fn evaluate_password_strength(password: &str) -> PasswordStrength {
    if password.is_empty() {
        return PasswordStrength {
            score: 0,
            percent: 0,
            guesses: 1,
            feedback: "Enter a master password".to_string(),
        };
    }

    let chars: Vec<char> = password.chars().collect();
    let len = chars.len();

    let mut has_lower = false;
    let mut has_upper = false;
    let mut has_digit = false;
    let mut has_symbol = false;

    for &c in &chars {
        if c.is_ascii_lowercase() {
            has_lower = true;
        } else if c.is_ascii_uppercase() {
            has_upper = true;
        } else if c.is_ascii_digit() {
            has_digit = true;
        } else {
            has_symbol = true;
        }
    }

    let variety =
        (has_lower as i32) + (has_upper as i32) + (has_digit as i32) + (has_symbol as i32);

    // Đếm số ký tự duy nhất để phạt các chuỗi lặp (như "aaaaaaaa")
    let mut unique = chars.clone();
    unique.sort_unstable();
    unique.dedup();
    let unique_len = unique.len();

    let (score, percent, guesses, feedback) = if len < 8 || unique_len <= 3 {
        (
            1,
            25,
            1_000,
            "Weak - Use at least 8 characters with numbers & symbols".to_string(),
        )
    } else if len >= 16 && variety >= 3 && unique_len >= 8 {
        (
            4,
            96,
            100_000_000_000,
            "Excellent - Highly secure and resilient".to_string(),
        )
    } else if len >= 12 && variety >= 3 && unique_len >= 6 {
        (
            3,
            82,
            100_000_000,
            "Strong - Safe and reliable master password".to_string(),
        )
    } else if len >= 8 && variety >= 2 {
        (
            2,
            55,
            1_000_000,
            "Fair - Good, but can be stronger with more length".to_string(),
        )
    } else {
        (
            1,
            35,
            10_000,
            "Weak - Add uppercase letters, numbers, or symbols".to_string(),
        )
    };

    PasswordStrength {
        score,
        percent,
        guesses,
        feedback,
    }
}
