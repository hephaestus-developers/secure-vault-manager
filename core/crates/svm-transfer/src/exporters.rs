//! Bộ kết xuất dữ liệu kho (`VaultItem`) ra các định dạng tệp sao lưu mã hóa SVM, JSON và CSV.

use std::fs;

use svm_crypto::{chacha20_poly1305_encrypt, derive_kek, random_bytes};
use svm_domain::{DomainError, JsonValue, VaultItem};

use crate::format::{TransferFormat, AAD_BACKUP, BACKUP_MAGIC};

/// Xuất danh sách `VaultItem` ra tệp trên đĩa theo định dạng `TransferFormat`.
pub fn export_items_to_file(
    format: TransferFormat,
    items: &[VaultItem],
    dst_path: &str,
    password: &str,
) -> Result<(), DomainError> {
    let bytes = export_items_to_bytes(format, items, password)?;
    if let Some(parent) = std::path::Path::new(dst_path).parent() {
        if !parent.as_os_str().is_empty() {
            let _ = fs::create_dir_all(parent);
        }
    }
    fs::write(dst_path, bytes).map_err(|e| DomainError::Io(e.to_string()))
}

/// Xuất danh sách `VaultItem` thành mảng byte theo định dạng `TransferFormat`.
pub fn export_items_to_bytes(
    format: TransferFormat,
    items: &[VaultItem],
    password: &str,
) -> Result<Vec<u8>, DomainError> {
    match format {
        TransferFormat::Svm => {
            let mut arr = JsonValue::new_array();
            for it in items {
                arr.push(it.to_json());
            }
            let mut root = JsonValue::new_object();
            root.insert("items", arr);
            let json_bytes = root.to_json_string().into_bytes();

            let mut salt = [0u8; 16];
            let mut nonce = [0u8; 12];
            random_bytes(&mut salt);
            random_bytes(&mut nonce);
            let pw = if password.is_empty() {
                "svm-default-backup-key"
            } else {
                password
            };
            let kek = derive_kek(pw, &salt);
            let ct = chacha20_poly1305_encrypt(&kek, &nonce, AAD_BACKUP, &json_bytes);

            let mut out = Vec::with_capacity(4 + 16 + 12 + ct.len());
            out.extend_from_slice(BACKUP_MAGIC);
            out.extend_from_slice(&salt);
            out.extend_from_slice(&nonce);
            out.extend_from_slice(&ct);
            Ok(out)
        }
        TransferFormat::Csv => {
            let mut csv = String::from("title,category,username,password,url,totp,notes\n");
            for it in items {
                if it.trashed {
                    continue;
                }
                csv.push_str(&escape_csv_cell(&it.title));
                csv.push(',');
                csv.push_str(&escape_csv_cell(it.category.as_str()));
                csv.push(',');
                csv.push_str(&escape_csv_cell(&it.username));
                csv.push(',');
                csv.push_str(&escape_csv_cell(&it.password));
                csv.push(',');
                csv.push_str(&escape_csv_cell(&it.website_url));
                csv.push(',');
                csv.push_str(&escape_csv_cell(&it.totp_secret));
                csv.push(',');
                csv.push_str(&escape_csv_cell(&it.notes));
                csv.push('\n');
            }
            Ok(csv.into_bytes())
        }
        _ => {
            let mut arr = JsonValue::new_array();
            for it in items {
                if it.trashed {
                    continue;
                }
                arr.push(it.to_json());
            }
            let mut root = JsonValue::new_object();
            root.insert("encrypted", JsonValue::Bool(false));
            root.insert("items", arr);
            Ok(root.to_json_string().into_bytes())
        }
    }
}

fn escape_csv_cell(s: &str) -> String {
    if s.contains(',') || s.contains('"') || s.contains('\n') || s.contains('\r') {
        format!("\"{}\"", s.replace('"', "\"\""))
    } else {
        s.to_string()
    }
}
