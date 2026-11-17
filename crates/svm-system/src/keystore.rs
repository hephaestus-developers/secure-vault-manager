//! Tích hợp kho bí mật của hệ điều hành (OS Keystore / Secret Service / Keychain / Credential Manager).

use keyring::{Entry, Error as KeyringError};
use svm_domain::DomainError;

const KEYSTORE_SERVICE: &str = "io.hephaestus.secure-vault-manager";

/// Kiểm tra xem OS Keystore (Secret Service trên Linux, Keychain trên macOS,
/// Credential Manager trên Windows) có sẵn sàng lưu trữ bí mật hay không.
pub fn is_os_keystore_available() -> bool {
    let Ok(entry) = Entry::new(KEYSTORE_SERVICE, "__svm_probe__") else {
        return false;
    };
    match entry.get_password() {
        Ok(_) | Err(KeyringError::NoEntry) => true,
        Err(_) => false,
    }
}

/// Lưu một chuỗi bí mật vào OS Keystore theo định danh tài khoản.
pub fn keystore_set_secret(account: &str, secret: &str) -> Result<(), DomainError> {
    let entry = Entry::new(KEYSTORE_SERVICE, account)
        .map_err(|e| DomainError::Crypto(format!("OS Keystore init failed: {e}")))?;
    entry
        .set_password(secret)
        .map_err(|e| DomainError::Crypto(format!("OS Keystore unavailable or write failed: {e}")))
}

/// Đọc chuỗi bí mật từ OS Keystore.
pub fn keystore_get_secret(account: &str) -> Result<String, DomainError> {
    let entry = Entry::new(KEYSTORE_SERVICE, account)
        .map_err(|e| DomainError::Crypto(format!("OS Keystore init failed: {e}")))?;
    entry.get_password().map_err(|e| match e {
        KeyringError::NoEntry => {
            DomainError::NotFound("Biometric key not found in OS Keystore".to_string())
        }
        other => DomainError::Crypto(format!("OS Keystore read failed: {other}")),
    })
}

/// Xóa chuỗi bí mật khỏi OS Keystore.
pub fn keystore_delete_secret(account: &str) -> Result<(), DomainError> {
    let entry = Entry::new(KEYSTORE_SERVICE, account)
        .map_err(|e| DomainError::Crypto(format!("OS Keystore init failed: {e}")))?;
    match entry.delete_credential() {
        Ok(()) | Err(KeyringError::NoEntry) => Ok(()),
        Err(other) => Err(DomainError::Io(format!(
            "OS Keystore delete failed: {other}"
        ))),
    }
}
