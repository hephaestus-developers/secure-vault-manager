//! # svm-vault
//!
//! Mô-đun nghiệp vụ quản lý vòng đời kho dữ liệu, phiên mở khóa trong RAM,
//! cơ chế tự động khóa (Auto-Lock), lịch sử sửa đổi (Revisions), tệp đính kèm
//! và các thao tác thêm, sửa, xóa mục lưu trữ.

pub mod attachment;
pub mod defaults;
pub mod items;
pub mod service;

pub use attachment::AttachmentMeta;
pub use service::VaultService;

#[cfg(test)]
mod tests {
    use std::fs;
    use svm_crypto::generate_uuid_v4;
    use svm_domain::VaultItem;
    use svm_storage::VaultStorage;

    use super::*;

    #[test]
    fn test_vault_service_lifecycle_and_crud() {
        let dir = format!("/tmp/svm_test_vault_svc_{}", generate_uuid_v4());
        let mut svc = VaultService::with_storage(VaultStorage::new(&dir));

        // Kho chưa được khởi tạo thì không được phép mở khóa bằng bất kỳ mật khẩu nào
        assert!(!svc.is_configured());
        assert!(svc.unlock_with_credentials("personal", "anything").is_err());
        assert!(!svc.is_unlocked());

        let id = svc
            .create_vault("Personal Vault", "StrongMaster#2026")
            .expect("create vault");
        assert_eq!(id, "personal");
        assert!(svc.is_unlocked());

        // Upsert new item
        let new_item = VaultItem {
            title: "GitLab Enterprise".to_string(),
            username: "hai@hephaestus.io".to_string(),
            password: "GitLab#SuperSecret!99".to_string(),
            ..VaultItem::default()
        };
        let item_id = svc.upsert_item(new_item).expect("upsert item");

        let revealed = svc
            .reveal_field(&item_id, "password")
            .expect("reveal password");
        assert_eq!(revealed, "GitLab#SuperSecret!99");

        // Lock and re-unlock
        svc.lock();
        assert!(!svc.is_unlocked());
        svc.unlock_with_credentials("personal", "StrongMaster#2026")
            .expect("unlock");
        assert!(svc.is_unlocked());

        let _ = fs::remove_dir_all(&dir);
    }
}
