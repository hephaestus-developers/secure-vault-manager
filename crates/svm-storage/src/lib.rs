//! # svm-storage
//!
//! Mô-đun quản lý lưu trữ tệp nhị phân mã hóa `vault.svm`, tệp companion sinh trắc học
//! `biometric.env`, ghi nguyên tử `.tmp -> rename`, cấu hình `settings.json` và tệp đính kèm.

pub mod atomic;
pub mod header;
pub mod vault_file;

pub use atomic::{atomic_write_file, resolve_default_data_dir};
pub use header::VaultHeader;
pub use vault_file::VaultStorage;

#[cfg(test)]
mod tests {
    use std::fs;
    use std::path::PathBuf;
    use svm_crypto::random_bytes;

    use super::*;

    #[test]
    fn test_storage_encrypt_unlock_and_recovery() {
        let dir = format!("/tmp/svm_test_storage_{}", svm_crypto::generate_uuid_v4());
        let storage = VaultStorage::new(&dir);

        let mut dek = [0u8; 32];
        random_bytes(&mut dek);
        let rec_key = "SVM-ABCD-EFGH-JKLM-NPQR";
        let mut header = VaultStorage::create_header(&dek, "MasterPass123!", rec_key);

        let payload = "{\"vaults\":[],\"items\":[]}";
        storage
            .save_vault_payload(&header, &dek, payload)
            .expect("save payload");

        // Wrong password must fail
        assert!(storage.unlock_with_password("WrongPass").is_err());

        // Correct password must succeed
        let (_, unlocked_dek, unlocked_json) = storage
            .unlock_with_password("MasterPass123!")
            .expect("unlock with master");
        assert_eq!(unlocked_dek, dek);
        assert_eq!(unlocked_json, payload);

        // Recovery key must succeed
        let (_, rec_dek, rec_json) = storage
            .unlock_with_recovery("svm-abcd-efgh-jklm-npqr")
            .expect("unlock with recovery");
        assert_eq!(rec_dek, dek);
        assert_eq!(rec_json, payload);

        // Change master password
        VaultStorage::rewrap_master_password(&mut header, &dek, "NewMasterPass999!");
        storage
            .save_vault_payload(&header, &dek, payload)
            .expect("save rewrapped");
        assert!(storage.unlock_with_password("MasterPass123!").is_err());
        assert!(storage.unlock_with_password("NewMasterPass999!").is_ok());

        // Rotate recovery key
        let new_rec_key = "ABCD-EFGH-JKLM-NPQR-STUV-WXYZ";
        VaultStorage::rewrap_recovery_key(&mut header, &dek, new_rec_key);
        storage
            .save_vault_payload(&header, &dek, payload)
            .expect("save rewrapped recovery key");
        assert!(storage
            .unlock_with_recovery("SECURE-VAULT-RECOVERY-KEY-1")
            .is_err());
        assert!(storage.unlock_with_recovery(new_rec_key).is_ok());

        // Streaming 64KB chunked attachment roundtrip (~150KB > 2 chunks)
        let src_att = PathBuf::from(&dir).join("big_secret.bin");
        let dst_att = PathBuf::from(&dir).join("big_secret_restored.bin");
        let original_data: Vec<u8> = (0..150_000).map(|i| (i % 251) as u8).collect();
        fs::write(&src_att, &original_data).expect("write src");

        let written = storage
            .store_attachment_file(&dek, "att-chunked-1", &src_att)
            .expect("store chunked");
        assert_eq!(written, original_data.len() as u64);

        storage
            .export_attachment_file(&dek, "att-chunked-1", &dst_att)
            .expect("export chunked");
        assert_eq!(fs::read(&dst_att).expect("read restored"), original_data);

        // Truncated attachment must fail AEAD/stream verification
        let bin_path = PathBuf::from(&dir).join("attachments/att-chunked-1.bin");
        let mut enc_bytes = fs::read(&bin_path).expect("read enc");
        enc_bytes.truncate(enc_bytes.len() - 4);
        fs::write(&bin_path, &enc_bytes).expect("truncate");
        assert!(storage.load_attachment(&dek, "att-chunked-1").is_err());

        let _ = fs::remove_dir_all(&dir);
    }
}
