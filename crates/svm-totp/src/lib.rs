//! # svm-totp
//!
//! Mô-đun tính toán mã xác thực hai lớp TOTP theo chuẩn RFC 6238, HOTP theo RFC 4226,
//! phân tích cú pháp URI `otpauth://` và giải mã ảnh mã QR.

pub mod otpauth;
pub mod qr;
pub mod totp;

pub use otpauth::{parse_otpauth_uri, OtpAuthConfig};
pub use qr::{decode_qr_luma, decode_qr_rgba};
pub use totp::generate_totp;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_totp_rfc6238_consistency() {
        let (code1, rem1) = generate_totp("JBSWY3DPEHPK3PXP", 1_700_000_000);
        let (code2, rem2) = generate_totp("JBSWY3DPEHPK3PXP", 1_700_000_005);
        assert_eq!(code1.len(), 6);
        assert_eq!(code1, code2);
        assert_eq!(rem1, 10);
        assert_eq!(rem2, 5);

        // RFC 6238 official test vector:
        // Secret "12345678901234567890" in Base32 = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ" (32 chars)
        let (rfc_code_59, _) = generate_totp("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 59);
        assert_eq!(rfc_code_59, "287082");

        let (rfc_code_1111111109, _) =
            generate_totp("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 1_111_111_109);
        assert_eq!(rfc_code_1111111109, "081804");

        let (rfc_code_1234567890, _) =
            generate_totp("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 1_234_567_890);
        assert_eq!(rfc_code_1234567890, "005924");

        let (rfc_code_2000000000, _) =
            generate_totp("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 2_000_000_000);
        assert_eq!(rfc_code_2000000000, "279037");
    }

    #[test]
    fn test_parse_otpauth_uri() {
        let uri = "otpauth://totp/GitHub:octocat?secret=JBSWY3DPEHPK3PXP&issuer=GitHub&digits=6&period=30";
        let cfg = parse_otpauth_uri(uri).expect("valid uri");
        assert_eq!(cfg.issuer, "GitHub");
        assert_eq!(cfg.account, "octocat");
        assert_eq!(cfg.secret, "JBSWY3DPEHPK3PXP");
        assert_eq!(cfg.digits, 6);
        assert_eq!(cfg.period, 30);
    }
}
