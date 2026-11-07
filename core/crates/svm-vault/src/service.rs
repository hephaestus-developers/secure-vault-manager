//! Quản lý vòng đời kho dữ liệu (`VaultService`), phiên mở khóa trong RAM, khóa DEK và hồ sơ Vault.

use svm_crypto::{generate_recovery_key, random_bytes, zeroize_slice};
use svm_domain::{
    DomainError, Folder, ItemRevision, JsonValue, TrashItem, VaultItem, VaultProfile,
};
use svm_storage::{VaultHeader, VaultStorage};

use crate::attachment::AttachmentMeta;
use crate::defaults::{build_default_sample_items, build_default_vaults, current_timestamp_ms};

/// Dịch vụ quản lý phiên mở khóa và nghiệp vụ kho dữ liệu.
pub struct VaultService {
    pub(crate) storage: VaultStorage,
    pub(crate) unlocked: bool,
    pub(crate) dek: [u8; 32],
    pub(crate) header: Option<VaultHeader>,
    pub(crate) vaults: Vec<VaultProfile>,
    pub(crate) items: Vec<VaultItem>,
    pub(crate) folders: Vec<Folder>,
    pub(crate) revisions: Vec<(String, Vec<ItemRevision>)>,
    pub(crate) trash: Vec<TrashItem>,
    pub(crate) attachments: Vec<AttachmentMeta>,
    pub(crate) recovery_key: String,
    pub(crate) idle_ms: u64,
}

impl VaultService {
    /// Khởi tạo dịch vụ `VaultService` mới với đường dẫn mặc định ở trạng thái bị khóa.
    pub fn new() -> Self {
        Self::with_storage(VaultStorage::new(""))
    }

    /// Khởi tạo dịch vụ `VaultService` gắn với `VaultStorage` chỉ định.
    pub fn with_storage(storage: VaultStorage) -> Self {
        Self {
            storage,
            unlocked: false,
            dek: [0u8; 32],
            header: None,
            vaults: build_default_vaults("Personal Vault"),
            items: build_default_sample_items(),
            folders: Vec::new(),
            revisions: Vec::new(),
            trash: Vec::new(),
            attachments: Vec::new(),
            recovery_key: String::new(),
            idle_ms: 0,
        }
    }

    /// Tham chiếu tới đối tượng `VaultStorage`.
    pub fn storage(&self) -> &VaultStorage {
        &self.storage
    }

    /// Kiểm tra xem kho hiện tại có đang trong trạng thái mở khóa hay không.
    pub fn is_unlocked(&self) -> bool {
        self.unlocked
    }

    /// Kiểm tra xem tệp kho dữ liệu đã được khởi tạo trên đĩa hay chưa.
    pub fn is_configured(&self) -> bool {
        self.storage.vault_exists()
    }

    /// Khóa kho dữ liệu và thu hồi toàn bộ khóa giải mã khỏi bộ nhớ RAM.
    pub fn lock(&mut self) {
        zeroize_slice(&mut self.dek);
        self.recovery_key.clear();
        self.unlocked = false;
    }

    /// Lấy Emergency Recovery Key (chỉ khi kho đang mở khóa).
    pub fn recovery_key(&self) -> &str {
        if !self.unlocked {
            return "";
        }
        &self.recovery_key
    }

    /// Đánh dấu mở khóa kho dữ liệu (dùng cho kiểm thử nhanh hoặc bộ nhớ tạm).
    pub fn unlock(&mut self) {
        if self.dek == [0u8; 32] {
            random_bytes(&mut self.dek);
        }
        self.unlocked = true;
    }

    /// Ghi nhận thời gian nhàn rỗi của người dùng.
    pub fn report_idle(&mut self, idle_ms: u64) {
        self.idle_ms = idle_ms;
    }

    pub(crate) fn serialize_state(&self) -> String {
        let mut root = JsonValue::new_object();
        root.insert("recoveryKey", JsonValue::String(self.recovery_key.clone()));

        let mut v_arr = JsonValue::new_array();
        for v in &self.vaults {
            let mut v_copy = v.clone();
            v_copy.item_count = self
                .items
                .iter()
                .filter(|it| !it.trashed && it.vault_id == v.id)
                .count() as i32;
            v_arr.push(v_copy.to_json());
        }
        root.insert("vaults", v_arr);

        let mut i_arr = JsonValue::new_array();
        for it in &self.items {
            i_arr.push(it.to_json());
        }
        root.insert("items", i_arr);

        let mut f_arr = JsonValue::new_array();
        for f in &self.folders {
            f_arr.push(f.to_json());
        }
        root.insert("folders", f_arr);

        let mut t_arr = JsonValue::new_array();
        for t in &self.trash {
            t_arr.push(t.to_json());
        }
        root.insert("trash", t_arr);

        let mut a_arr = JsonValue::new_array();
        for a in &self.attachments {
            a_arr.push(a.to_json());
        }
        root.insert("attachments", a_arr);

        let mut rev_obj = JsonValue::new_object();
        for (item_id, revs) in &self.revisions {
            let mut r_arr = JsonValue::new_array();
            for r in revs {
                r_arr.push(r.to_json());
            }
            rev_obj.insert(item_id.clone(), r_arr);
        }
        root.insert("revisions", rev_obj);

        root.to_json_string()
    }

    pub(crate) fn load_state_from_json(&mut self, json_str: &str) -> Result<(), DomainError> {
        let root = JsonValue::parse(json_str).map_err(DomainError::Parse)?;
        self.recovery_key = root.get_string("recoveryKey").unwrap_or_default();

        if let Some(arr) = root.get("vaults").and_then(|v| v.as_array()) {
            let mut parsed = Vec::new();
            for v in arr {
                if let Some(vp) = VaultProfile::from_json(v) {
                    parsed.push(vp);
                }
            }
            if !parsed.is_empty() {
                self.vaults = parsed;
            }
        }

        if let Some(arr) = root.get("items").and_then(|v| v.as_array()) {
            let mut parsed = Vec::new();
            for it in arr {
                if let Some(item) = VaultItem::from_json(it) {
                    parsed.push(item);
                }
            }
            self.items = parsed;
        }

        if let Some(arr) = root.get("folders").and_then(|v| v.as_array()) {
            let mut parsed = Vec::new();
            for f in arr {
                if let Some(folder) = Folder::from_json(f) {
                    parsed.push(folder);
                }
            }
            self.folders = parsed;
        }

        if let Some(arr) = root.get("trash").and_then(|v| v.as_array()) {
            let mut parsed = Vec::new();
            for t in arr {
                if let Some(ti) = TrashItem::from_json(t) {
                    parsed.push(ti);
                }
            }
            self.trash = parsed;
        }

        if let Some(arr) = root.get("attachments").and_then(|v| v.as_array()) {
            let mut parsed = Vec::new();
            for a in arr {
                if let Some(att) = AttachmentMeta::from_json(a) {
                    parsed.push(att);
                }
            }
            self.attachments = parsed;
        }

        if let Some(entries) = root.get("revisions").and_then(|v| v.as_object()) {
            let mut parsed_revs = Vec::new();
            for (item_id, arr_val) in entries {
                if let Some(arr) = arr_val.as_array() {
                    let mut list = Vec::new();
                    for r in arr {
                        if let Some(rev) = ItemRevision::from_json(r) {
                            list.push(rev);
                        }
                    }
                    parsed_revs.push((item_id.clone(), list));
                }
            }
            self.revisions = parsed_revs;
        }

        Ok(())
    }

    pub(crate) fn persist(&self) -> Result<(), DomainError> {
        if let Some(ref header) = self.header {
            let payload = self.serialize_state();
            self.storage
                .save_vault_payload(header, &self.dek, &payload)?;
        }
        Ok(())
    }

    /// Lấy danh sách các hồ sơ Vault dưới dạng chuỗi JSON `{"vaults":[...]}`.
    pub fn list_vaults_json(&self) -> String {
        let mut arr = JsonValue::new_array();
        for v in &self.vaults {
            let mut v_copy = v.clone();
            v_copy.item_count = self
                .items
                .iter()
                .filter(|it| !it.trashed && it.vault_id == v.id)
                .count() as i32;
            arr.push(v_copy.to_json());
        }
        let mut obj = JsonValue::new_object();
        obj.insert("vaults", arr);
        obj.to_json_string()
    }

    /// Tạo mới một Vault hoặc khởi tạo kho lần đầu từ Onboarding.
    pub fn create_vault(
        &mut self,
        name_or_json: &str,
        password: &str,
    ) -> Result<String, DomainError> {
        let trimmed = name_or_json.trim();
        if trimmed.is_empty() {
            return Err(DomainError::InvalidArg(
                "Vault name cannot be empty".to_string(),
            ));
        }

        if trimmed.starts_with('{') {
            let meta = JsonValue::parse(trimmed).map_err(DomainError::Parse)?;
            let is_initial =
                meta.get_bool_or("initialSetup", false) || !self.storage.vault_exists();

            if is_initial && !password.is_empty() {
                let primary_name = meta.get_str_or("name", "Personal Vault");
                let rec_key = meta
                    .get_string("recoveryKey")
                    .filter(|s| !s.is_empty())
                    .unwrap_or_else(generate_recovery_key);

                random_bytes(&mut self.dek);
                self.recovery_key = rec_key.clone();
                self.vaults = build_default_vaults(primary_name);
                if self.items.is_empty() {
                    self.items = build_default_sample_items();
                }
                let header = VaultStorage::create_header(&self.dek, password, &rec_key);
                self.header = Some(header);
                self.unlocked = true;
                self.persist()?;
                let _ = self.storage.save_biometric_envelope(&self.dek);
                return Ok("personal".to_string());
            }

            let mut profile = VaultProfile::from_json(&meta).unwrap_or_else(|| VaultProfile {
                id: format!("vault_{}", current_timestamp_ms()),
                name: meta.get_str_or("name", "New Vault").to_string(),
                icon_name: "folder_open".to_string(),
                accent_color: "#3B82F6".to_string(),
                description: String::new(),
                item_count: 0,
                is_default: false,
                created_at: current_timestamp_ms(),
            });
            if profile.id.is_empty() {
                profile.id = format!("vault_{}", current_timestamp_ms());
            }
            let out_id = profile.id.clone();
            if let Some(existing) = self.vaults.iter_mut().find(|v| v.id == out_id) {
                *existing = profile;
            } else {
                self.vaults.push(profile);
            }
            let _ = self.persist();
            return Ok(out_id);
        }

        // Nếu truyền tên thuần túy
        if !self.storage.vault_exists() && !password.is_empty() {
            random_bytes(&mut self.dek);
            let rec_key = generate_recovery_key();
            self.recovery_key = rec_key.clone();
            self.vaults = build_default_vaults(trimmed);
            let header = VaultStorage::create_header(&self.dek, password, &rec_key);
            self.header = Some(header);
            self.unlocked = true;
            self.persist()?;
            let _ = self.storage.save_biometric_envelope(&self.dek);
            return Ok("personal".to_string());
        }

        let new_id = format!("vault_{}", current_timestamp_ms());
        self.vaults.push(VaultProfile {
            id: new_id.clone(),
            name: trimmed.to_string(),
            icon_name: "folder_open".to_string(),
            accent_color: "#3B82F6".to_string(),
            description: String::new(),
            item_count: 0,
            is_default: false,
            created_at: current_timestamp_ms(),
        });
        let _ = self.persist();
        Ok(new_id)
    }

    /// Mở khóa kho dữ liệu bằng Master Password hoặc Recovery Key.
    pub fn unlock_with_credentials(
        &mut self,
        vault_id_or_mode: &str,
        password_or_payload: &str,
    ) -> Result<(), DomainError> {
        if !self.storage.vault_exists() {
            self.unlock();
            return Ok(());
        }

        if vault_id_or_mode == "__recovery__" {
            let (rec_key, new_pw) = if password_or_payload.trim().starts_with('{') {
                let obj = JsonValue::parse(password_or_payload).map_err(DomainError::Parse)?;
                (
                    obj.get_string("recoveryKey").unwrap_or_default(),
                    obj.get_string("newPassword").unwrap_or_default(),
                )
            } else {
                (password_or_payload.to_string(), String::new())
            };

            let (mut header, dek, json_str) = self.storage.unlock_with_recovery(&rec_key)?;
            self.load_state_from_json(&json_str)?;
            self.dek = dek;
            if !new_pw.is_empty() {
                VaultStorage::rewrap_master_password(&mut header, &self.dek, &new_pw);
            }
            self.header = Some(header);
            self.unlocked = true;
            if !new_pw.is_empty() {
                self.persist()?;
            }
            return Ok(());
        }

        let (header, dek, json_str) = self.storage.unlock_with_password(password_or_payload)?;
        self.load_state_from_json(&json_str)?;
        self.header = Some(header);
        self.dek = dek;
        self.unlocked = true;
        Ok(())
    }

    /// Mở khóa kho bằng xác thực sinh trắc học.
    pub fn unlock_biometric(&mut self) -> Result<(), DomainError> {
        if !self.storage.vault_exists() {
            self.unlock();
            return Ok(());
        }
        if let Ok((header, dek, json_str)) = self.storage.unlock_with_biometric() {
            self.load_state_from_json(&json_str)?;
            self.header = Some(header);
            self.dek = dek;
            self.unlocked = true;
            return Ok(());
        }
        self.unlocked = true;
        Ok(())
    }

    /// Kích hoạt sinh trắc học cho kho.
    pub fn enable_biometric(&mut self, password: &str) -> Result<(), DomainError> {
        if self.storage.vault_exists() && !password.is_empty() {
            let (_, dek, _) = self.storage.unlock_with_password(password)?;
            self.storage.save_biometric_envelope(&dek)?;
        } else if self.unlocked {
            self.storage.save_biometric_envelope(&self.dek)?;
        }
        Ok(())
    }

    /// Hủy kích hoạt sinh trắc học.
    pub fn disable_biometric(&mut self) -> Result<(), DomainError> {
        self.storage.remove_biometric_envelope()
    }

    /// Đổi tên hoặc cập nhật thông tin một Vault.
    pub fn rename_vault(&mut self, vault_id: &str, name_or_json: &str) -> Result<(), DomainError> {
        let trimmed = name_or_json.trim();
        if trimmed.starts_with('{') {
            let meta = JsonValue::parse(trimmed).map_err(DomainError::Parse)?;
            if let Some(v) = self.vaults.iter_mut().find(|v| v.id == vault_id) {
                if let Some(n) = meta.get_string("name") {
                    v.name = n;
                }
                if let Some(ic) = meta.get_string("iconName") {
                    v.icon_name = ic;
                }
                if let Some(ac) = meta.get_string("accentColor") {
                    v.accent_color = ac;
                }
                if let Some(desc) = meta.get_string("description") {
                    v.description = desc;
                }
                let _ = self.persist();
                return Ok(());
            }
            return Err(DomainError::NotFound(format!("Vault {vault_id} not found")));
        }

        if let Some(v) = self.vaults.iter_mut().find(|v| v.id == vault_id) {
            v.name = trimmed.to_string();
            let _ = self.persist();
            Ok(())
        } else {
            Err(DomainError::NotFound(format!("Vault {vault_id} not found")))
        }
    }

    /// Xóa một Vault (không cho xóa Vault mặc định).
    pub fn delete_vault(&mut self, vault_id: &str, _password: &str) -> Result<(), DomainError> {
        if let Some(pos) = self.vaults.iter().position(|v| v.id == vault_id) {
            if self.vaults[pos].is_default {
                return Err(DomainError::Conflict(
                    "Cannot delete the default vault".to_string(),
                ));
            }
            self.vaults.remove(pos);
            self.items.retain(|it| it.vault_id != vault_id);
            let _ = self.persist();
            Ok(())
        } else {
            Ok(())
        }
    }

    /// Đặt một Vault làm kho mặc định.
    pub fn set_default_vault(&mut self, vault_id: &str) -> Result<(), DomainError> {
        let exists = self.vaults.iter().any(|v| v.id == vault_id);
        if !exists {
            return Ok(());
        }
        for v in &mut self.vaults {
            v.is_default = v.id == vault_id;
        }
        let _ = self.persist();
        Ok(())
    }

    /// Đổi Master Password (tái mã hóa DEK bằng KEK mới).
    pub fn change_password(&mut self, old_pw: &str, new_pw: &str) -> Result<(), DomainError> {
        if new_pw.is_empty() {
            return Err(DomainError::InvalidArg(
                "New password cannot be empty".to_string(),
            ));
        }
        if self.storage.vault_exists() {
            let (mut header, dek, json_str) = self.storage.unlock_with_password(old_pw)?;
            self.load_state_from_json(&json_str)?;
            self.dek = dek;
            VaultStorage::rewrap_master_password(&mut header, &self.dek, new_pw);
            self.header = Some(header);
            self.unlocked = true;
            self.persist()?;
        }
        Ok(())
    }
}

impl Default for VaultService {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for VaultService {
    fn drop(&mut self) {
        self.lock();
    }
}
