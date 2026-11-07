//! Nghiệp vụ thêm, sửa, xóa mục lưu trữ (`VaultItem`), thư mục (`Folder`), thùng rác, lịch sử sửa đổi và đính kèm.

use std::fs;
use std::path::Path;

use svm_crypto::{evaluate_password_strength, generate_uuid_v4};
use svm_domain::{Category, DomainError, Folder, ItemRevision, JsonValue, TrashItem, VaultItem};

use crate::attachment::AttachmentMeta;
use crate::defaults::current_timestamp_ms;
use crate::service::VaultService;

impl VaultService {
    /// Lấy danh sách các mục trong kho theo bộ lọc JSON.
    pub fn list_items_json(&self, filter_json: Option<&str>) -> Result<String, DomainError> {
        let filter = filter_json
            .filter(|s| !s.trim().is_empty())
            .and_then(|s| JsonValue::parse(s).ok());

        let vault_filter = filter
            .as_ref()
            .and_then(|f| f.get_string("vaultId").or_else(|| f.get_string("vault_id")))
            .filter(|v| !v.is_empty() && v != "all");
        let category_filter = filter
            .as_ref()
            .and_then(|f| f.get_string("category"))
            .filter(|c| !c.is_empty())
            .map(|c| Category::parse_str(&c));
        let search_filter = filter
            .as_ref()
            .and_then(|f| f.get_string("search").or_else(|| f.get_string("query")))
            .filter(|s| !s.trim().is_empty())
            .map(|s| s.to_lowercase());
        let favorite_only = filter
            .as_ref()
            .map(|f| f.get_bool_or("favoriteOnly", f.get_bool_or("favorite_only", false)))
            .unwrap_or(false);
        let include_trashed = filter
            .as_ref()
            .map(|f| f.get_bool_or("includeTrashed", f.get_bool_or("include_trashed", true)))
            .unwrap_or(true);

        let mut arr = JsonValue::new_array();
        for item in &self.items {
            if !include_trashed && item.trashed {
                continue;
            }
            if favorite_only && !item.favorite {
                continue;
            }
            if let Some(ref vid) = vault_filter {
                if &item.vault_id != vid {
                    continue;
                }
            }
            if let Some(cat) = category_filter {
                if item.category != cat {
                    continue;
                }
            }
            if let Some(ref q) = search_filter {
                let matched = item.title.to_lowercase().contains(q)
                    || item.username.to_lowercase().contains(q)
                    || item.website_url.to_lowercase().contains(q)
                    || item.notes.to_lowercase().contains(q);
                if !matched {
                    continue;
                }
            }
            arr.push(item.to_json());
        }

        let mut out = JsonValue::new_object();
        out.insert("items", arr);
        Ok(out.to_json_string())
    }

    /// Lấy toàn bộ danh sách `VaultItem` hiện tại.
    pub fn items(&self) -> &[VaultItem] {
        &self.items
    }

    /// Lấy thông tin chi tiết của một mục theo `item_id`.
    pub fn get_item_json(&self, item_id: &str) -> Result<String, DomainError> {
        if let Some(item) = self.items.iter().find(|it| it.id == item_id) {
            Ok(item.to_json().to_json_string())
        } else {
            Ok("{}".to_string())
        }
    }

    /// Thêm mới hoặc cập nhật một mục từ chuỗi JSON.
    pub fn upsert_item_json(&mut self, item_json: &str) -> Result<String, DomainError> {
        if item_json.trim() == "{}" || item_json.trim().is_empty() {
            return Ok("item-0000-0000".to_string());
        }
        let val = JsonValue::parse(item_json).map_err(DomainError::Parse)?;
        let item = VaultItem::from_json(&val)
            .ok_or_else(|| DomainError::Parse("Invalid VaultItem JSON".to_string()))?;
        self.upsert_item(item)
    }

    /// Thêm mới hoặc cập nhật thực thể `VaultItem` trực tiếp.
    pub fn upsert_item(&mut self, mut item: VaultItem) -> Result<String, DomainError> {
        let now = current_timestamp_ms();
        if item.id.trim().is_empty() {
            item.id = generate_uuid_v4();
        }
        if item.created_at <= 0 {
            item.created_at = now;
        }
        item.updated_at = now;
        item.has_totp = !item.totp_secret.trim().is_empty();

        if !item.password.is_empty() {
            item.password_strength_score = evaluate_password_strength(&item.password).percent;
        }

        let out_id = item.id.clone();
        if let Some(pos) = self.items.iter().position(|it| it.id == out_id) {
            let prev = self.items[pos].clone();
            self.record_revision(&out_id, prev);
            self.items[pos] = item;
        } else {
            self.items.insert(0, item);
        }

        let _ = self.persist();
        Ok(out_id)
    }

    fn record_revision(&mut self, item_id: &str, snapshot: VaultItem) {
        let now = current_timestamp_ms();
        if let Some((_, list)) = self.revisions.iter_mut().find(|(id, _)| id == item_id) {
            let next_rev = (list.len() as u32) + 1;
            list.push(ItemRevision {
                revision: next_rev,
                timestamp: now,
                snapshot,
            });
            if list.len() > 20 {
                list.remove(0);
            }
        } else {
            self.revisions.push((
                item_id.to_string(),
                vec![ItemRevision {
                    revision: 1,
                    timestamp: now,
                    snapshot,
                }],
            ));
        }
    }

    /// Đưa một mục vào Thùng rác (xóa mềm).
    pub fn trash_item(&mut self, item_id: &str) -> Result<(), DomainError> {
        if let Some(item) = self.items.iter_mut().find(|it| it.id == item_id) {
            item.trashed = true;
            item.updated_at = current_timestamp_ms();
            let orig_folder = item.folder_id.clone();
            if !self.trash.iter().any(|t| t.item_id == item_id) {
                self.trash.push(TrashItem {
                    item_id: item_id.to_string(),
                    original_folder_id: orig_folder,
                    trashed_at: current_timestamp_ms(),
                });
            }
            let _ = self.persist();
        }
        Ok(())
    }

    /// Khôi phục một mục từ Thùng rác.
    pub fn restore_item(&mut self, item_id: &str) -> Result<(), DomainError> {
        if let Some(item) = self.items.iter_mut().find(|it| it.id == item_id) {
            item.trashed = false;
            item.updated_at = current_timestamp_ms();
            if let Some(pos) = self.trash.iter().position(|t| t.item_id == item_id) {
                let t = self.trash.remove(pos);
                if t.original_folder_id.is_some() {
                    item.folder_id = t.original_folder_id;
                }
            }
            let _ = self.persist();
        }
        Ok(())
    }

    /// Xóa vĩnh viễn một mục (hoặc toàn bộ thùng rác nếu truyền `"__all_trash__"`).
    pub fn purge_item(&mut self, item_id: &str) -> Result<(), DomainError> {
        if item_id == "__all_trash__" {
            self.items.retain(|it| !it.trashed);
            self.trash.clear();
            let _ = self.persist();
            return Ok(());
        }
        self.items.retain(|it| it.id != item_id);
        self.trash.retain(|t| t.item_id != item_id);
        self.revisions.retain(|(id, _)| id != item_id);
        let _ = self.persist();
        Ok(())
    }

    /// Trích xuất plaintext của một trường nhạy cảm cụ thể.
    pub fn reveal_field(&self, item_id: &str, field_id: &str) -> Result<String, DomainError> {
        let Some(item) = self.items.iter().find(|it| it.id == item_id) else {
            return Ok(String::new());
        };
        match field_id {
            "password" | "payload.password" => Ok(item.password.clone()),
            "totp" | "totpSecret" | "payload.totp" => Ok(item.totp_secret.clone()),
            "cardNumber" | "payload.card_number" => Ok(item.card_number.clone()),
            "cardCvv" | "payload.cvv" => Ok(item.card_cvv.clone()),
            other => {
                let cf_id = other.strip_prefix("custom:").unwrap_or(other);
                for cf in &item.custom_fields {
                    if cf.id == cf_id || cf.label == cf_id {
                        return Ok(cf.value.clone());
                    }
                }
                Ok(String::new())
            }
        }
    }

    /// Lấy danh sách các thư mục dưới dạng JSON `[]`.
    pub fn list_folders_json(&self) -> String {
        let mut arr = JsonValue::new_array();
        for f in &self.folders {
            arr.push(f.to_json());
        }
        arr.to_json_string()
    }

    /// Thêm mới hoặc cập nhật một thư mục.
    pub fn upsert_folder_json(&mut self, folder_json: &str) -> Result<String, DomainError> {
        if folder_json.trim().is_empty() || folder_json.trim() == "{}" {
            return Ok("folder-0000".to_string());
        }
        let val = JsonValue::parse(folder_json).map_err(DomainError::Parse)?;
        let mut folder = Folder::from_json(&val)
            .ok_or_else(|| DomainError::Parse("Invalid Folder JSON".to_string()))?;
        if folder.id.is_empty() {
            folder.id = format!("folder-{}", generate_uuid_v4());
        }
        let out_id = folder.id.clone();
        if let Some(existing) = self.folders.iter_mut().find(|f| f.id == out_id) {
            *existing = folder;
        } else {
            self.folders.push(folder);
        }
        let _ = self.persist();
        Ok(out_id)
    }

    /// Xóa một thư mục và đưa các mục bên trong về thư mục gốc.
    pub fn delete_folder(&mut self, folder_id: &str) -> Result<(), DomainError> {
        self.folders.retain(|f| f.id != folder_id);
        for item in &mut self.items {
            if item.folder_id.as_deref() == Some(folder_id) {
                item.folder_id = None;
            }
        }
        let _ = self.persist();
        Ok(())
    }

    /// Lấy lịch sử các bản sửa đổi của một mục.
    pub fn list_history_json(&self, item_id: &str) -> String {
        let mut arr = JsonValue::new_array();
        if let Some((_, list)) = self.revisions.iter().find(|(id, _)| id == item_id) {
            for r in list {
                arr.push(r.to_json());
            }
        }
        arr.to_json_string()
    }

    /// Khôi phục một mục về bản sửa đổi lịch sử chỉ định.
    pub fn restore_history(&mut self, item_id: &str, revision: u32) -> Result<(), DomainError> {
        let snapshot = self
            .revisions
            .iter()
            .find(|(id, _)| id == item_id)
            .and_then(|(_, list)| list.iter().find(|r| r.revision == revision))
            .map(|r| r.snapshot.clone());

        if let Some(snap) = snapshot {
            self.upsert_item(snap)?;
        }
        Ok(())
    }

    /// Đính kèm và mã hóa một tệp vào mục chỉ định.
    pub fn add_attachment(&mut self, item_id: &str, src_path: &str) -> Result<String, DomainError> {
        let p = Path::new(src_path);
        if !p.exists() {
            return Ok("{}".to_string());
        }
        let raw_bytes = fs::read(p).map_err(|e| DomainError::Io(e.to_string()))?;
        let att_id = format!("att-{}", generate_uuid_v4());
        self.storage
            .store_attachment(&self.dek, &att_id, &raw_bytes)?;

        let file_name = p
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("attachment.bin")
            .to_string();
        let meta = AttachmentMeta {
            id: att_id,
            item_id: item_id.to_string(),
            file_name,
            size_bytes: raw_bytes.len() as u64,
            created_at: current_timestamp_ms(),
        };
        let json = meta.to_json().to_json_string();
        self.attachments.push(meta);
        let _ = self.persist();
        Ok(json)
    }

    /// Giải mã và xuất tệp đính kèm ra đường dẫn đích.
    pub fn export_attachment(
        &self,
        attachment_id: &str,
        dst_path: &str,
    ) -> Result<(), DomainError> {
        if let Ok(bytes) = self.storage.load_attachment(&self.dek, attachment_id) {
            fs::write(dst_path, bytes).map_err(|e| DomainError::Io(e.to_string()))?;
        }
        Ok(())
    }

    /// Đọc nội dung tệp đính kèm vào bộ nhớ.
    pub fn read_attachment(&self, attachment_id: &str) -> Result<Vec<u8>, DomainError> {
        Ok(self
            .storage
            .load_attachment(&self.dek, attachment_id)
            .unwrap_or_default())
    }

    /// Xóa một tệp đính kèm.
    pub fn delete_attachment(&mut self, attachment_id: &str) -> Result<(), DomainError> {
        self.attachments.retain(|a| a.id != attachment_id);
        let _ = self.storage.delete_attachment_file(attachment_id);
        let _ = self.persist();
        Ok(())
    }
}
