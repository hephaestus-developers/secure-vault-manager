//! Tiện ích ghi tệp nguyên tử (`.tmp` -> `rename`) và phân giải thư mục dữ liệu hệ điều hành.

use std::fs;
use std::path::Path;

use svm_domain::DomainError;

/// Xác định thư mục dữ liệu mặc định của người dùng trên hệ điều hành.
pub fn resolve_default_data_dir() -> String {
    if let Ok(env_dir) = std::env::var("SVM_DATA_DIR") {
        if !env_dir.trim().is_empty() {
            return env_dir;
        }
    }
    if let Ok(home) = std::env::var("HOME") {
        if !home.trim().is_empty() {
            return format!("{home}/.local/share/hephaestus/secure-vault-manager");
        }
    }
    "/tmp/hephaestus-svm-data".to_string()
}

/// Ghi nguyên tử (Atomic Write) dữ liệu ra tệp thông qua tệp tạm `.tmp` rồi đổi tên.
pub fn atomic_write_file(path: &Path, bytes: &[u8]) -> Result<(), DomainError> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|e| DomainError::Io(e.to_string()))?;
    }
    let tmp_path = path.with_extension("tmp");
    fs::write(&tmp_path, bytes).map_err(|e| DomainError::Io(e.to_string()))?;
    fs::rename(&tmp_path, path).map_err(|e| DomainError::Io(e.to_string()))?;
    Ok(())
}
