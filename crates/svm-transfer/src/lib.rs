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

    #[test]
    fn test_onepux_zip_and_bitwarden_and_keepass_importers() {
        use std::io::{Cursor, Write};
        use zip::write::SimpleFileOptions;
        use zip::ZipWriter;

        // 1. Real .1pux ZIP archive
        let export_data = r#"{
            "accounts": [{
                "vaults": [{
                    "items": [{
                        "categoryUuid": "001",
                        "overview": { "title": "1P Corp Login", "url": "https://corp.local" },
                        "details": {
                            "loginFields": [
                                { "designation": "username", "value": "alice@corp" },
                                { "designation": "password", "value": "CorrectHorse!99" }
                            ],
                            "notesPlain": "Corporate account",
                            "sections": [{ "fields": [{ "title": "OTP", "value": { "totp": "JBSWY3DPEHPK3PXP" } }] }]
                        }
                    }]
                }]
            }]
        }"#;
        let mut buf = Cursor::new(Vec::new());
        {
            let mut zip_w = ZipWriter::new(&mut buf);
            zip_w
                .start_file("export.data", SimpleFileOptions::default())
                .expect("start_file");
            zip_w.write_all(export_data.as_bytes()).expect("write");
            zip_w.finish().expect("finish");
        }
        let zip_bytes = buf.into_inner();
        let parsed_1p =
            parse_import_bytes(TransferFormat::OnePux, &zip_bytes, "").expect("parse 1pux");
        assert_eq!(parsed_1p.len(), 1);
        assert_eq!(parsed_1p[0].title, "1P Corp Login");
        assert_eq!(parsed_1p[0].username, "alice@corp");
        assert_eq!(parsed_1p[0].totp_secret, "JBSWY3DPEHPK3PXP");

        // 2. Bitwarden encrypted rejection & card parsing
        assert!(
            parse_import_bytes(TransferFormat::BitwardenJson, br#"{"encrypted":true}"#, "")
                .is_err()
        );
        let bw_card = br#"{"items":[{"type":3,"name":"Corp Visa","card":{"cardholderName":"Alice","number":"4111222233334444","expMonth":"09","expYear":"2029","code":"999"}}]}"#;
        let parsed_bw =
            parse_import_bytes(TransferFormat::BitwardenJson, bw_card, "").expect("parse bw");
        assert_eq!(parsed_bw[0].category, Category::Card);
        assert_eq!(parsed_bw[0].card_expiry, "09/29");

        // 3. KeePass XML skips <History> entries and computes real strength score
        let kp_xml = br#"<KeePassFile><Root><Group><Entry>
            <String><Key>Title</Key><Value>Active Entry</Value></String>
            <String><Key>UserName</Key><Value>bob</Value></String>
            <String><Key>Password</Key><Value>123</Value></String>
            <History><Entry><String><Key>Title</Key><Value>Old Revision</Value></String></Entry></History>
        </Entry></Group></Root></KeePassFile>"#;
        let parsed_kp =
            parse_import_bytes(TransferFormat::KeePassXml, kp_xml, "").expect("parse kp");
        assert_eq!(parsed_kp.len(), 1);
        assert_eq!(parsed_kp[0].title, "Active Entry");
        assert!(parsed_kp[0].password_strength_score < 60);
    }
}
