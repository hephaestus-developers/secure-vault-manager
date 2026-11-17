//! # external_installer
//!
//! Mô-đun quản lý cài đặt External Extensions theo chuẩn Alternative Distribution của Chromium.
//! Cho phép ứng dụng Desktop tự động đăng ký extension đã xuất bản trên Chrome Web Store / Edge Add-ons
//! vào thư mục `External Extensions` (Linux/macOS) hoặc Windows Registry, kích hoạt prompt cài đặt
//! tự động cho người dùng mà không cần Load unpacked.

use std::fs;
use std::path::Path;
use svm_domain::DomainError;

/// ID chính thức của extension Secure Vault Manager trên Chrome Web Store / Store.
pub const STORE_CHROME_EXTENSION_ID: &str = "jfgmpgpijiloaglplchmjomeboglioec";

/// URL dịch vụ cập nhật tự động của Chrome Web Store (CRX v2/v3).
pub const CHROME_WEBSTORE_UPDATE_URL: &str = "https://clients2.google.com/service/update2/crx";

/// URL dịch vụ cập nhật tự động của Microsoft Edge Add-ons.
pub const EDGE_UPDATE_URL: &str = "https://edge.microsoft.com/extensionproxy/service/update2/crx";

/// Trình quản lý cài đặt External Extension cho các trình duyệt Chromium.
pub struct ExternalExtensionInstaller;

impl ExternalExtensionInstaller {
    /// Cài đặt cấu hình External Extension cho tất cả trình duyệt Chromium phát hiện trên máy.
    pub fn install_external_extensions() -> Result<Vec<String>, DomainError> {
        let mut installed_paths = Vec::new();
        let home = std::env::var("HOME")
            .or_else(|_| std::env::var("USERPROFILE"))
            .unwrap_or_else(|_| "/tmp".to_string());

        let extension_json = format!(
            "{{\n  \"external_update_url\": \"{}\"\n}}\n",
            CHROME_WEBSTORE_UPDATE_URL
        );
        let filename = format!("{STORE_CHROME_EXTENSION_ID}.json");

        let candidate_dirs = [
            // Linux Chromium family
            format!("{home}/.config/google-chrome/External Extensions"),
            format!("{home}/.config/chromium/External Extensions"),
            format!("{home}/.config/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/.config/microsoft-edge/External Extensions"),
            format!("{home}/.var/app/com.google.Chrome/config/google-chrome/External Extensions"),
            format!("{home}/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/.var/app/com.microsoft.Edge/config/microsoft-edge/External Extensions"),
            format!("{home}/snap/chromium/current/.config/chromium/External Extensions"),
            // macOS Chromium family
            format!("{home}/Library/Application Support/Google/Chrome/External Extensions"),
            format!("{home}/Library/Application Support/Chromium/External Extensions"),
            format!("{home}/Library/Application Support/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/Library/Application Support/Microsoft Edge/External Extensions"),
        ];

        for dir_str in &candidate_dirs {
            let p = Path::new(dir_str);
            if let Some(parent) = p.parent() {
                if parent.exists() && fs::create_dir_all(p).is_ok() {
                    let ext_file = p.join(&filename);
                    if fs::write(&ext_file, extension_json.as_bytes()).is_ok() {
                        installed_paths.push(ext_file.to_string_lossy().to_string());
                    }
                }
            }
        }

        #[cfg(windows)]
        {
            register_windows_external_extensions(&mut installed_paths);
        }

        Ok(installed_paths)
    }

    /// Gỡ bỏ cấu hình External Extension khỏi các trình duyệt.
    pub fn uninstall_external_extensions() -> Result<Vec<String>, DomainError> {
        let mut removed_paths = Vec::new();
        let home = std::env::var("HOME")
            .or_else(|_| std::env::var("USERPROFILE"))
            .unwrap_or_else(|_| "/tmp".to_string());
        let filename = format!("{STORE_CHROME_EXTENSION_ID}.json");

        let candidate_dirs = [
            format!("{home}/.config/google-chrome/External Extensions"),
            format!("{home}/.config/chromium/External Extensions"),
            format!("{home}/.config/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/.config/microsoft-edge/External Extensions"),
            format!("{home}/.var/app/com.google.Chrome/config/google-chrome/External Extensions"),
            format!("{home}/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/.var/app/com.microsoft.Edge/config/microsoft-edge/External Extensions"),
            format!("{home}/snap/chromium/current/.config/chromium/External Extensions"),
            format!("{home}/Library/Application Support/Google/Chrome/External Extensions"),
            format!("{home}/Library/Application Support/Chromium/External Extensions"),
            format!("{home}/Library/Application Support/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/Library/Application Support/Microsoft Edge/External Extensions"),
        ];

        for dir_str in &candidate_dirs {
            let ext_file = Path::new(dir_str).join(&filename);
            if ext_file.exists() && fs::remove_file(&ext_file).is_ok() {
                removed_paths.push(ext_file.to_string_lossy().to_string());
            }
        }

        #[cfg(windows)]
        {
            unregister_windows_external_extensions(&mut removed_paths);
        }

        Ok(removed_paths)
    }

    /// Kiểm tra xem External Extension đã được cài đặt vào ít nhất một vị trí hợp lệ hay chưa.
    pub fn is_external_extension_installed() -> bool {
        let home = std::env::var("HOME")
            .or_else(|_| std::env::var("USERPROFILE"))
            .unwrap_or_else(|_| "/tmp".to_string());
        let filename = format!("{STORE_CHROME_EXTENSION_ID}.json");

        let candidate_dirs = [
            format!("{home}/.config/google-chrome/External Extensions"),
            format!("{home}/.config/chromium/External Extensions"),
            format!("{home}/.config/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/.config/microsoft-edge/External Extensions"),
            format!("{home}/Library/Application Support/Google/Chrome/External Extensions"),
            format!("{home}/Library/Application Support/Chromium/External Extensions"),
            format!("{home}/Library/Application Support/BraveSoftware/Brave-Browser/External Extensions"),
            format!("{home}/Library/Application Support/Microsoft Edge/External Extensions"),
        ];

        for dir_str in &candidate_dirs {
            let ext_file = Path::new(dir_str).join(&filename);
            if ext_file.exists() {
                return true;
            }
        }

        #[cfg(windows)]
        {
            if is_windows_external_extension_registered() {
                return true;
            }
        }

        false
    }
}

#[cfg(windows)]
fn register_windows_external_extensions(installed_paths: &mut Vec<String>) {
    use windows_sys::Win32::Foundation::ERROR_SUCCESS;
    use windows_sys::Win32::System::Registry::{
        RegCloseKey, RegCreateKeyExW, RegSetValueExW, HKEY_CURRENT_USER, KEY_WRITE, REG_SZ,
    };

    let entries = [
        (
            format!(r"Software\Google\Chrome\Extensions\{STORE_CHROME_EXTENSION_ID}"),
            CHROME_WEBSTORE_UPDATE_URL,
        ),
        (
            format!(r"Software\Chromium\Extensions\{STORE_CHROME_EXTENSION_ID}"),
            CHROME_WEBSTORE_UPDATE_URL,
        ),
        (
            format!(r"Software\BraveSoftware\Brave-Browser\Extensions\{STORE_CHROME_EXTENSION_ID}"),
            CHROME_WEBSTORE_UPDATE_URL,
        ),
        (
            format!(r"Software\Microsoft\Edge\Extensions\{STORE_CHROME_EXTENSION_ID}"),
            CHROME_WEBSTORE_UPDATE_URL,
        ),
    ];

    for (subkey, update_url) in entries {
        let subkey_w: Vec<u16> = subkey.encode_utf16().chain(std::iter::once(0)).collect();
        let val_name_w: Vec<u16> = "update_url"
            .encode_utf16()
            .chain(std::iter::once(0))
            .collect();
        let val_w: Vec<u16> = update_url
            .encode_utf16()
            .chain(std::iter::once(0))
            .collect();
        let mut hkey = 0 as _;
        let rc = unsafe {
            RegCreateKeyExW(
                HKEY_CURRENT_USER,
                subkey_w.as_ptr(),
                0,
                core::ptr::null(),
                0,
                KEY_WRITE,
                core::ptr::null(),
                &mut hkey,
                core::ptr::null_mut(),
            )
        };
        if rc == ERROR_SUCCESS && !hkey.is_null() {
            let byte_len = (val_w.len() * 2) as u32;
            unsafe {
                RegSetValueExW(
                    hkey,
                    val_name_w.as_ptr(),
                    0,
                    REG_SZ,
                    val_w.as_ptr() as *const u8,
                    byte_len,
                );
                RegCloseKey(hkey);
            }
            installed_paths.push(format!(r"HKCU\{subkey}\update_url"));
        }
    }
}

#[cfg(windows)]
fn unregister_windows_external_extensions(removed_paths: &mut Vec<String>) {
    use windows_sys::Win32::Foundation::ERROR_SUCCESS;
    use windows_sys::Win32::System::Registry::{RegDeleteKeyW, HKEY_CURRENT_USER};

    let entries = [
        format!(r"Software\Google\Chrome\Extensions\{STORE_CHROME_EXTENSION_ID}"),
        format!(r"Software\Chromium\Extensions\{STORE_CHROME_EXTENSION_ID}"),
        format!(r"Software\BraveSoftware\Brave-Browser\Extensions\{STORE_CHROME_EXTENSION_ID}"),
        format!(r"Software\Microsoft\Edge\Extensions\{STORE_CHROME_EXTENSION_ID}"),
    ];

    for subkey in entries {
        let subkey_w: Vec<u16> = subkey.encode_utf16().chain(std::iter::once(0)).collect();
        let rc = unsafe { RegDeleteKeyW(HKEY_CURRENT_USER, subkey_w.as_ptr()) };
        if rc == ERROR_SUCCESS {
            removed_paths.push(format!(r"HKCU\{subkey}"));
        }
    }
}

#[cfg(windows)]
fn is_windows_external_extension_registered() -> bool {
    use windows_sys::Win32::Foundation::ERROR_SUCCESS;
    use windows_sys::Win32::System::Registry::{
        RegCloseKey, RegOpenKeyExW, HKEY_CURRENT_USER, KEY_READ,
    };

    let subkey = format!(r"Software\Google\Chrome\Extensions\{STORE_CHROME_EXTENSION_ID}");
    let subkey_w: Vec<u16> = subkey.encode_utf16().chain(std::iter::once(0)).collect();
    let mut hkey = 0 as _;
    let rc = unsafe { RegOpenKeyExW(HKEY_CURRENT_USER, subkey_w.as_ptr(), 0, KEY_READ, &mut hkey) };
    if rc == ERROR_SUCCESS && !hkey.is_null() {
        unsafe {
            RegCloseKey(hkey);
        }
        true
    } else {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_external_installer_constants() {
        assert_eq!(
            STORE_CHROME_EXTENSION_ID,
            "jfgmpgpijiloaglplchmjomeboglioec"
        );
        assert!(CHROME_WEBSTORE_UPDATE_URL.contains("service/update2/crx"));
        assert!(EDGE_UPDATE_URL.contains("service/update2/crx"));
    }

    #[test]
    fn test_install_and_uninstall_external_extensions() {
        let res = ExternalExtensionInstaller::install_external_extensions();
        assert!(res.is_ok());

        let unres = ExternalExtensionInstaller::uninstall_external_extensions();
        assert!(unres.is_ok());
    }
}
