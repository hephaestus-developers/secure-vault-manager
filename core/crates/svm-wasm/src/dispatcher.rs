//! Bộ điều phối lệnh JSON thống nhất (`wasm_dispatch_json`) và các hàm tiện ích mật mã cho WebAssembly.

use svm_crypto::{
    evaluate_password_strength, generate_password, hex_encode, hmac_sha256, seed_entropy, sha256,
};
use svm_domain::{DomainError, JsonValue};
use svm_totp::{generate_totp, parse_otpauth_uri};

use crate::envelope::{wasm_decrypt_offline_envelope, wasm_encrypt_offline_envelope};

/// Khởi tạo bộ tạo số ngẫu nhiên (PRNG) trong môi trường WASM với hạt giống từ `crypto.getRandomValues`.
pub fn wasm_init(entropy_seed: u64) {
    seed_entropy(entropy_seed);
}

/// Sinh mã TOTP (RFC 6238) tại mốc thời gian Unix (`timestamp_secs`).
pub fn wasm_generate_totp(secret_or_uri: &str, timestamp_secs: u64) -> Result<String, DomainError> {
    let (code, remaining) = generate_totp(secret_or_uri, timestamp_secs);
    let mut obj = JsonValue::new_object();
    obj.insert("code", JsonValue::String(code));
    obj.insert("remaining", JsonValue::Number(remaining as f64));
    Ok(obj.to_json_string())
}

/// Phân tích chuỗi `otpauth://totp/...` thành đối tượng JSON cấu hình TOTP.
pub fn wasm_parse_otpauth(uri: &str) -> Result<String, DomainError> {
    let cfg = parse_otpauth_uri(uri)?;
    Ok(cfg.to_json().to_json_string())
}

/// Sinh mật khẩu ngẫu nhiên mật mã học theo cấu hình độ dài và tập ký tự.
pub fn wasm_generate_password(
    length: usize,
    upper: bool,
    lower: bool,
    digits: bool,
    symbols: bool,
) -> String {
    let mut opts = JsonValue::new_object();
    opts.insert("length", JsonValue::Number(length as f64));
    opts.insert("uppercase", JsonValue::Bool(upper));
    opts.insert("lowercase", JsonValue::Bool(lower));
    opts.insert("digits", JsonValue::Bool(digits));
    opts.insert("symbols", JsonValue::Bool(symbols));
    generate_password(&opts.to_json_string())
}

/// Đánh giá độ mạnh của mật khẩu và trả về JSON (`score`, `percent`, `guesses`, `feedback`).
pub fn wasm_evaluate_password_strength(password: &str) -> String {
    evaluate_password_strength(password).to_json_string()
}

/// Tính băm SHA-256 của chuỗi đầu vào và trả về chuỗi hex.
pub fn wasm_sha256_hex(input: &str) -> String {
    hex_encode(&sha256(input.as_bytes()))
}

/// Tính mã xác thực thông điệp HMAC-SHA256 và trả về chuỗi hex.
pub fn wasm_hmac_sha256_hex(key: &[u8], message: &[u8]) -> String {
    hex_encode(&hmac_sha256(key, message))
}

/// Bộ điều phối lệnh JSON thống nhất cho môi trường WASM (`wasm_dispatch_json`).
/// Cho phép JavaScript gọi mọi tác vụ mật mã qua một cổng giao tiếp chuỗi JSON duy nhất.
pub fn wasm_dispatch_json(request_json: &str) -> String {
    let parsed = match JsonValue::parse(request_json) {
        Ok(v) => v,
        Err(e) => return error_json(&format!("Invalid JSON request: {e}")),
    };

    let action = parsed.get_str_or("action", parsed.get_str_or("op", ""));
    match action {
        "totp" | "generate_totp" => {
            let secret = parsed.get_str_or("secret", "");
            let ts = parsed.get_i64_or("timestamp", 0).max(0) as u64;
            let (code, remaining) = generate_totp(secret, ts);
            let mut data = JsonValue::new_object();
            data.insert("code", JsonValue::String(code));
            data.insert("remaining", JsonValue::Number(remaining as f64));
            ok_json(data)
        }
        "parse_otpauth" => {
            let uri = parsed.get_str_or("uri", "");
            match parse_otpauth_uri(uri) {
                Ok(cfg) => ok_json(cfg.to_json()),
                Err(e) => error_json(e.message()),
            }
        }
        "generate_password" => {
            let length = (parsed.get_i64_or("length", 20).max(4) as usize).min(128);
            let upper = parsed.get_bool_or("uppercase", parsed.get_bool_or("upper", true));
            let lower = parsed.get_bool_or("lowercase", parsed.get_bool_or("lower", true));
            let digits = parsed.get_bool_or("digits", true);
            let symbols = parsed.get_bool_or("symbols", true);
            let pwd = wasm_generate_password(length, upper, lower, digits, symbols);
            let eval = evaluate_password_strength(&pwd);
            let mut data = JsonValue::new_object();
            data.insert("password", JsonValue::String(pwd));
            data.insert("score", JsonValue::Number(eval.score as f64));
            data.insert("percent", JsonValue::Number(eval.percent as f64));
            data.insert("feedback", JsonValue::String(eval.feedback));
            ok_json(data)
        }
        "password_strength" => {
            let pwd = parsed.get_str_or("password", "");
            let eval = evaluate_password_strength(pwd);
            let mut data = JsonValue::new_object();
            data.insert("score", JsonValue::Number(eval.score as f64));
            data.insert("percent", JsonValue::Number(eval.percent as f64));
            data.insert("guesses", JsonValue::Number(eval.guesses as f64));
            data.insert("feedback", JsonValue::String(eval.feedback));
            ok_json(data)
        }
        "encrypt_envelope" => {
            let pwd = parsed.get_str_or("password", "");
            let data = parsed.get_str_or("data", "");
            match wasm_encrypt_offline_envelope(pwd, data) {
                Ok(env_str) => {
                    let env_val = JsonValue::parse(&env_str).unwrap_or(JsonValue::String(env_str));
                    ok_json(env_val)
                }
                Err(e) => error_json(e.message()),
            }
        }
        "decrypt_envelope" => {
            let pwd = parsed.get_str_or("password", "");
            let env = parsed.get_str_or("envelope", "");
            match wasm_decrypt_offline_envelope(pwd, env) {
                Ok(plain) => {
                    let mut data = JsonValue::new_object();
                    data.insert("plaintext", JsonValue::String(plain));
                    ok_json(data)
                }
                Err(e) => error_json(e.message()),
            }
        }
        "sha256" => {
            let input = parsed.get_str_or("data", "");
            let mut data = JsonValue::new_object();
            data.insert("hash", JsonValue::String(wasm_sha256_hex(input)));
            ok_json(data)
        }
        other => error_json(&format!("Unsupported wasm operation: {other}")),
    }
}

fn ok_json(data: JsonValue) -> String {
    let mut obj = JsonValue::new_object();
    obj.insert("ok", JsonValue::Bool(true));
    obj.insert("data", data);
    obj.to_json_string()
}

fn error_json(msg: &str) -> String {
    let mut obj = JsonValue::new_object();
    obj.insert("ok", JsonValue::Bool(false));
    obj.insert("error", JsonValue::String(msg.to_string()));
    obj.to_json_string()
}
