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

        let summary_list = svc.list_items_json(None).expect("list items");
        assert!(summary_list.contains("GitLab Enterprise"));
        assert!(!summary_list.contains("GitLab#SuperSecret!99"));

        let detail_json = svc.get_item_json(&item_id).expect("get item detail");
        assert!(detail_json.contains("GitLab#SuperSecret!99"));

        let revealed = svc
            .reveal_field(&item_id, "password")
            .expect("reveal password");
        assert_eq!(revealed, "GitLab#SuperSecret!99");

        // Lock and re-unlock
        svc.lock();
        assert!(!svc.is_unlocked());
        assert!(svc.list_items_json(None).is_err());
        assert!(svc.upsert_item(VaultItem::default()).is_err());
        assert!(svc.delete_vault("personal", "StrongMaster#2026").is_err());

        svc.unlock_with_credentials("personal", "StrongMaster#2026")
            .expect("unlock");
        assert!(svc.is_unlocked());
        assert!(svc.list_vaults_json().contains("\"configured\":true"));

        // Tạo thêm vault phụ để kiểm tra xóa vault cần đúng mật khẩu
        let work_id = svc
            .create_vault("Work Vault", "StrongMaster#2026")
            .expect("create work vault");
        assert!(svc.delete_vault(&work_id, "WrongPassword").is_err());
        svc.delete_vault(&work_id, "StrongMaster#2026")
            .expect("delete vault with right password");

        // Kiểm tra auto-lock khi vượt quá idle threshold
        svc.apply_settings_json(r#"{"auto_lock_minutes":1,"auto_empty_trash":false}"#);
        assert!(!svc.report_idle(30_000));
        assert!(svc.is_unlocked());
        assert!(svc.report_idle(60_000));
        assert!(!svc.is_unlocked());

        let _ = fs::remove_dir_all(&dir);
    }
}
