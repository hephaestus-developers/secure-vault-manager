//! # svm-transfer
//!
//! Mô-đun phân tích cú pháp tệp nhập và xuất dữ liệu tương thích với các trình quản lý
//! mật khẩu phổ biến (1Password `.1pux`, Bitwarden JSON/CSV, KeePass XML, và SVM native format).

pub mod exporters;
pub mod format;
pub mod importers;

pub use exporters::{export_items_to_bytes, export_items_to_file};
pub use format::TransferFormat;
pub use importers::{parse_import_bytes, parse_import_file};

#[cfg(test)]
mod tests {
    use svm_domain::{Category, VaultItem};

    use super::*;

    #[test]
    fn test_csv_export_and_import_roundtrip() {
        let item = VaultItem {
            id: "test-1".to_string(),
            title: "GitHub, Inc.".to_string(),
            category: Category::Login,
            username: "octocat".to_string(),
            password: "Pass\"123".to_string(),
            website_url: "https://github.com".to_string(),
            notes: "Line 1\nLine 2".to_string(),
            totp_secret: "JBSWY3DPEHPK3PXP".to_string(),
            ..Default::default()
        };

        let bytes = export_items_to_bytes(TransferFormat::Csv, &[item], "").expect("export csv");
        let imported = parse_import_bytes(TransferFormat::Csv, &bytes, "").expect("import csv");
        assert_eq!(imported.len(), 1);
        assert_eq!(imported[0].title, "GitHub, Inc.");
        assert_eq!(imported[0].username, "octocat");
        assert_eq!(imported[0].password, "Pass\"123");
        assert_eq!(imported[0].totp_secret, "JBSWY3DPEHPK3PXP");
    }

    #[test]
    fn test_encrypted_svm_backup_roundtrip() {
        let item = VaultItem {
            id: "test-2".to_string(),
            title: "Production DB".to_string(),
            category: Category::Database,
            username: "admin".to_string(),
            password: "SecretDBPassword!".to_string(),
            ..Default::default()
        };

        let encrypted =
            export_items_to_bytes(TransferFormat::Svm, &[item], "BackupKey#1").expect("export");
        let restored =
            parse_import_bytes(TransferFormat::Svm, &encrypted, "BackupKey#1").expect("import");
        assert_eq!(restored.len(), 1);
        assert_eq!(restored[0].title, "Production DB");
    }
}
