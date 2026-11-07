//! # svm-totp
//!
//! Mô-đun tính toán mã xác thực hai lớp TOTP theo chuẩn RFC 6238, HOTP theo RFC 4226,
//! phân tích cú pháp URI `otpauth://` và giải mã ảnh mã QR.

pub mod otpauth;
pub mod totp;

pub use otpauth::{parse_otpauth_uri, OtpAuthConfig};
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
