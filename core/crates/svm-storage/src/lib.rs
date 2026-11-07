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

        let _ = fs::remove_dir_all(&dir);
    }
}
