//! Tiện ích ghi tệp nguyên tử (`.tmp` -> `rename`) với phân quyền bảo mật (`0o700`/`0o600`)
//! và phân giải thư mục dữ liệu hệ điều hành.

use std::fs;
#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;
use std::path::Path;

use svm_domain::DomainError;

/// Xác định thư mục dữ liệu mặc định của người dùng trên hệ điều hành.
pub fn resolve_default_data_dir() -> String {
    if let Ok(env_dir) = std::env::var("SVM_DATA_DIR") {
        if !env_dir.trim().is_empty() {
            return env_dir;
        }
    }
    #[cfg(target_os = "windows")]
    {
        if let Ok(appdata) = std::env::var("APPDATA") {
            if !appdata.trim().is_empty() {
                return format!("{appdata}\\Hephaestus\\SecureVaultManager");
            }
        }
        if let Ok(profile) = std::env::var("USERPROFILE") {
            if !profile.trim().is_empty() {
                return format!("{profile}\\AppData\\Roaming\\Hephaestus\\SecureVaultManager");
            }
        }
        return std::env::temp_dir()
            .join("hephaestus-svm-data")
            .to_string_lossy()
            .to_string();
    }
    #[cfg(target_os = "macos")]
    {
        if let Ok(home) = std::env::var("HOME") {
            if !home.trim().is_empty() {
                return format!("{home}/Library/Application Support/Hephaestus/SecureVaultManager");
            }
        }
        return "/tmp/hephaestus-svm-data".to_string();
    }
    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    {
        if let Ok(xdg) = std::env::var("XDG_DATA_HOME") {
            if !xdg.trim().is_empty() {
                return format!("{xdg}/hephaestus/secure-vault-manager");
            }
        }
        if let Ok(home) = std::env::var("HOME") {
            if !home.trim().is_empty() {
                return format!("{home}/.local/share/hephaestus/secure-vault-manager");
            }
        }
        "/tmp/hephaestus-svm-data".to_string()
    }
}

/// Đảm bảo thư mục tồn tại với quyền `0o700` trên Unix.
pub fn ensure_secure_dir(dir: &Path) -> Result<(), DomainError> {
    fs::create_dir_all(dir).map_err(|e| DomainError::Io(e.to_string()))?;
    #[cfg(unix)]
    {
        let _ = fs::set_permissions(dir, fs::Permissions::from_mode(0o700));
    }
    Ok(())
}

/// Ghi nguyên tử (Atomic Write) dữ liệu ra tệp thông qua tệp tạm `.tmp` với quyền `0o600`.
pub fn atomic_write_file(path: &Path, bytes: &[u8]) -> Result<(), DomainError> {
    if let Some(parent) = path.parent() {
        ensure_secure_dir(parent)?;
    }
    let tmp_path = path.with_extension("tmp");
    fs::write(&tmp_path, bytes).map_err(|e| DomainError::Io(e.to_string()))?;
    #[cfg(unix)]
    {
        let _ = fs::set_permissions(&tmp_path, fs::Permissions::from_mode(0o600));
    }
    fs::rename(&tmp_path, path).map_err(|e| DomainError::Io(e.to_string()))?;
    Ok(())
}
