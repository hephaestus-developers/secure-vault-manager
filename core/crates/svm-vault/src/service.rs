//! Quản lý vòng đời kho dữ liệu (`VaultService`), phiên mở khóa trong RAM, khóa DEK và hồ sơ Vault.

use svm_crypto::{
    generate_recovery_key, mem_lock, mem_unlock, random_bytes, zeroize_slice, zeroize_string,
};
use svm_domain::{DomainError, ItemRevision, JsonValue, TrashItem, VaultItem, VaultProfile};
use svm_storage::{VaultHeader, VaultStorage};

use crate::attachment::AttachmentMeta;
use crate::defaults::{build_default_vaults, current_timestamp_ms};

fn zeroize_vault_item(item: &mut VaultItem) {
    zeroize_string(&mut item.id);
    zeroize_string(&mut item.vault_id);
    zeroize_string(&mut item.title);
    zeroize_string(&mut item.username);
    zeroize_string(&mut item.password);
    zeroize_string(&mut item.website_url);
    zeroize_string(&mut item.notes);
    zeroize_string(&mut item.totp_secret);
    zeroize_string(&mut item.passkey_credential_id);
    zeroize_string(&mut item.passkey_rp_id);
    zeroize_string(&mut item.passkey_user_handle);
    item.passkey_algorithm = 0;
    item.passkey_sign_count = 0;
    zeroize_string(&mut item.ssh_public_key);
    zeroize_string(&mut item.ssh_fingerprint);
    zeroize_string(&mut item.card_number);
    zeroize_string(&mut item.card_expiry);
    zeroize_string(&mut item.card_cvv);
    zeroize_string(&mut item.db_host);
    zeroize_string(&mut item.db_name);
    zeroize_string(&mut item.wifi_ssid);
    zeroize_string(&mut item.wifi_security);
    for cf in &mut item.custom_fields {
        zeroize_string(&mut cf.id);
        zeroize_string(&mut cf.label);
        zeroize_string(&mut cf.value);
    }
    item.custom_fields.clear();
}

/// Dịch vụ quản lý phiên mở khóa và nghiệp vụ kho dữ liệu.
pub struct VaultService {
    pub(crate) storage: VaultStorage,
    pub(crate) unlocked: bool,
    pub(crate) dek: [u8; 32],
    pub(crate) header: Option<VaultHeader>,
    pub(crate) vaults: Vec<VaultProfile>,
    pub(crate) items: Vec<VaultItem>,
    pub(crate) revisions: Vec<(String, Vec<ItemRevision>)>,
    pub(crate) trash: Vec<TrashItem>,
    pub(crate) attachments: Vec<AttachmentMeta>,
    pub(crate) recovery_key: String,
    pub(crate) idle_ms: u64,
    pub(crate) auto_lock_minutes: u32,
    pub(crate) auto_empty_trash: bool,
    pub(crate) ssh_prompt_approval: bool,
    pub(crate) favicon_fetch_enabled: bool,
}

impl VaultService {
    /// Khởi tạo dịch vụ `VaultService` mới với đường dẫn mặc định ở trạng thái bị khóa.
    pub fn new() -> Self {
        Self::with_storage(VaultStorage::new(""))
    }

    /// Khởi tạo dịch vụ `VaultService` gắn với `VaultStorage` chỉ định.
    pub fn with_storage(storage: VaultStorage) -> Self {
        let initial_settings = storage.load_settings();
        let mut svc = Self {
            storage,
            unlocked: false,
            dek: [0u8; 32],
            header: None,
            vaults: Vec::new(),
            items: Vec::new(),
            revisions: Vec::new(),
            trash: Vec::new(),
            attachments: Vec::new(),
            recovery_key: String::new(),
            idle_ms: 0,
            auto_lock_minutes: 15,
            auto_empty_trash: false,
            ssh_prompt_approval: true,
            favicon_fetch_enabled: false,
        };
        svc.apply_settings_json(&initial_settings);
        svc
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

    /// Khóa kho dữ liệu, ghi đè 0x00 và thu hồi toàn bộ bí mật cùng khóa DEK khỏi RAM.
    pub fn lock(&mut self) {
        for item in &mut self.items {
            zeroize_vault_item(item);
        }
        self.items.clear();
        self.items.shrink_to_fit();

        for (_, revs) in &mut self.revisions {
            for rev in revs.iter_mut() {
                zeroize_vault_item(&mut rev.snapshot);
            }
        }
        self.revisions.clear();
        self.revisions.shrink_to_fit();

        for t in &mut self.trash {
            zeroize_string(&mut t.item_id);
        }
        self.trash.clear();
        self.trash.shrink_to_fit();

        for v in &mut self.vaults {
            zeroize_string(&mut v.id);
            zeroize_string(&mut v.name);
            zeroize_string(&mut v.description);
        }
        self.vaults.clear();
        self.vaults.shrink_to_fit();

        for a in &mut self.attachments {
            zeroize_string(&mut a.id);
            zeroize_string(&mut a.item_id);
            zeroize_string(&mut a.file_name);
        }
        self.attachments.clear();
        self.attachments.shrink_to_fit();

        zeroize_string(&mut self.recovery_key);
        self.header = None;
        mem_unlock(&self.dek);
        zeroize_slice(&mut self.dek);
        self.idle_ms = 0;
        self.unlocked = false;
    }

    /// Xóa toàn bộ dữ liệu trong RAM, sao lưu an toàn kho cũ và đặt lại trạng thái chưa cấu hình.
    pub fn reset_vault(&mut self) -> Result<Option<std::path::PathBuf>, DomainError> {
        self.lock();
        self.storage.backup_and_reset_vault()
    }

    /// Lấy Emergency Recovery Key (chỉ khi kho đang mở khóa).
    pub fn recovery_key(&self) -> &str {
        if !self.unlocked {
            return "";
        }
        &self.recovery_key
    }

    /// Trả về danh sách hồ sơ Vault đang quản lý.
    pub fn vaults(&self) -> &[VaultProfile] {
        &self.vaults
    }

    /// Lấy định danh của Vault mặc định (hoặc Vault đầu tiên nếu chưa đánh dấu).
    pub fn default_vault_id(&self) -> String {
        self.vaults
            .iter()
            .find(|v| v.is_default)
            .or_else(|| self.vaults.first())
            .map(|v| v.id.clone())
            .unwrap_or_else(|| "personal".to_string())
    }

    /// Cập nhật các ngưỡng cấu hình bảo mật từ chuỗi JSON AppSettings.
    pub fn apply_settings_json(&mut self, json_str: &str) {
        if let Ok(obj) = JsonValue::parse(json_str) {
            if let Some(mins) = obj.get("auto_lock_minutes").and_then(|v| v.as_i64()) {
                self.auto_lock_minutes = mins.max(0) as u32;
            }
            if obj.get("auto_empty_trash").is_some() {
                self.auto_empty_trash = obj.get_bool_or("auto_empty_trash", false);
            }
            if obj.get("ssh_prompt_approval").is_some() {
                self.ssh_prompt_approval = obj.get_bool_or("ssh_prompt_approval", true);
            }
            if obj.get("favicon_fetch_enabled").is_some() {
                self.favicon_fetch_enabled = obj.get_bool_or("favicon_fetch_enabled", false);
            }
        }
    }

    /// Trạng thái chính sách yêu cầu xác nhận trước khi ký yêu cầu SSH.
    pub fn ssh_prompt_approval(&self) -> bool {
        self.ssh_prompt_approval
    }

    /// Trạng thái cho phép tải Favicon website từ xa (opt-in, mặc định tắt để bảo vệ quyền riêng tư).
    pub fn favicon_fetch_enabled(&self) -> bool {
        self.favicon_fetch_enabled
    }

    /// Ghi nhận thời gian nhàn rỗi của người dùng; tự động khóa nếu vượt ngưỡng cấu hình.
    /// Trả về `true` nếu vừa kích hoạt khóa kho do nhàn rỗi.
    pub fn report_idle(&mut self, idle_ms: u64) -> bool {
        self.idle_ms = idle_ms;
        if self.unlocked && self.auto_lock_minutes > 0 {
            let threshold_ms = (self.auto_lock_minutes as u64).saturating_mul(60_000);
            if idle_ms >= threshold_ms {
                self.lock();
                return true;
            }
        }
        false
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
            let mut payload = self.serialize_state();
            let res = self.storage.save_vault_payload(header, &self.dek, &payload);
            zeroize_string(&mut payload);
            res?;
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
        obj.insert("configured", JsonValue::Bool(self.storage.vault_exists()));
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
                mem_lock(&self.dek);
                self.recovery_key = rec_key.clone();
                self.vaults = build_default_vaults(primary_name);
                self.items.clear();
                let header = VaultStorage::create_header(&self.dek, password, &rec_key);
                self.header = Some(header);
                self.unlocked = true;
                self.persist()?;
                if meta.get_bool_or("enableBiometric", false) {
                    let _ = self.storage.save_biometric_envelope(&self.dek);
                }
                return Ok("personal".to_string());
            }

            let mut profile = VaultProfile::from_json(&meta).unwrap_or_else(|| VaultProfile {
                id: format!("vault_{}", current_timestamp_ms()),
                name: meta.get_str_or("name", "New Vault").to_string(),
                icon_name: "vault_personal".to_string(),
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
            self.persist()?;
            return Ok(out_id);
        }

        // Nếu truyền tên thuần túy
        if !self.storage.vault_exists() && !password.is_empty() {
            random_bytes(&mut self.dek);
            mem_lock(&self.dek);
            let rec_key = generate_recovery_key();
            self.recovery_key = rec_key.clone();
            self.vaults = build_default_vaults(trimmed);
            self.items.clear();
            let header = VaultStorage::create_header(&self.dek, password, &rec_key);
            self.header = Some(header);
            self.unlocked = true;
            self.persist()?;
            return Ok("personal".to_string());
        }

        let new_id = format!("vault_{}", current_timestamp_ms());
        self.vaults.push(VaultProfile {
            id: new_id.clone(),
            name: trimmed.to_string(),
            icon_name: "vault_personal".to_string(),
            description: String::new(),
            item_count: 0,
            is_default: false,
            created_at: current_timestamp_ms(),
        });
        self.persist()?;
        Ok(new_id)
    }

    /// Mở khóa kho dữ liệu bằng Master Password hoặc Recovery Key.
    pub fn unlock_with_credentials(
        &mut self,
        vault_id_or_mode: &str,
        password_or_payload: &str,
    ) -> Result<(), DomainError> {
        if !self.storage.vault_exists() {
            return Err(DomainError::NotFound(
                "Vault has not been initialized".to_string(),
            ));
        }

        if vault_id_or_mode == "__recovery__" {
            let (mut rec_key, mut new_pw) = if password_or_payload.trim().starts_with('{') {
                let obj = JsonValue::parse(password_or_payload).map_err(DomainError::Parse)?;
                (
                    obj.get_string("recoveryKey").unwrap_or_default(),
                    obj.get_string("newPassword").unwrap_or_default(),
                )
            } else {
                (password_or_payload.to_string(), String::new())
            };

            let res = self.storage.unlock_with_recovery(&rec_key);
            zeroize_string(&mut rec_key);
            let (mut header, mut dek, mut json_str) = res?;
            let load_res = self.load_state_from_json(&json_str);
            zeroize_string(&mut json_str);
            load_res?;
            self.dek = dek;
            zeroize_slice(&mut dek);
            mem_lock(&self.dek);
            let rewrapped = !new_pw.is_empty();
            if rewrapped {
                VaultStorage::rewrap_master_password(&mut header, &self.dek, &new_pw);
            }
            zeroize_string(&mut new_pw);
            self.header = Some(header);
            self.unlocked = true;
            if rewrapped {
                self.persist()?;
            }
            return Ok(());
        }

        let (header, mut dek, mut json_str) =
            self.storage.unlock_with_password(password_or_payload)?;
        let load_res = self.load_state_from_json(&json_str);
        zeroize_string(&mut json_str);
        load_res?;
        self.header = Some(header);
        self.dek = dek;
        zeroize_slice(&mut dek);
        mem_lock(&self.dek);
        self.unlocked = true;
        if self.auto_empty_trash {
            let _ = self.purge_expired_trash(30);
        }
        Ok(())
    }

    /// Mở khóa kho bằng xác thực sinh trắc học / hệ điều hành thật.
    pub fn unlock_biometric(&mut self) -> Result<(), DomainError> {
        if !self.storage.vault_exists() {
            return Err(DomainError::NotFound(
                "Vault has not been initialized".to_string(),
            ));
        }
        if !self.storage.biometric_file_path().exists() {
            return Err(DomainError::NotFound(
                "Biometric unlock is not enabled".to_string(),
            ));
        }
        if !svm_system::authenticate_os_user("Unlock Secure Vault Manager") {
            return Err(DomainError::Crypto(
                "Biometric or OS authentication failed or cancelled".to_string(),
            ));
        }

        let (header, mut dek, mut json_str) = self.storage.unlock_with_biometric()?;
        let load_res = self.load_state_from_json(&json_str);
        zeroize_string(&mut json_str);
        load_res?;
        self.header = Some(header);
        self.dek = dek;
        zeroize_slice(&mut dek);
        mem_lock(&self.dek);
        self.unlocked = true;
        if self.auto_empty_trash {
            let _ = self.purge_expired_trash(30);
        }
        Ok(())
    }

    /// Kích hoạt sinh trắc học cho kho.
    pub fn enable_biometric(&mut self, password: &str) -> Result<(), DomainError> {
        if self.storage.vault_exists() && !password.is_empty() {
            let (_, mut dek, mut json_str) = self.storage.unlock_with_password(password)?;
            zeroize_string(&mut json_str);
            let res = self.storage.save_biometric_envelope(&dek);
            zeroize_slice(&mut dek);
            res?;
            return Ok(());
        }
        if self.unlocked && self.dek != [0u8; 32] {
            self.storage.save_biometric_envelope(&self.dek)?;
            return Ok(());
        }
        Err(DomainError::Locked)
    }

    /// Hủy kích hoạt sinh trắc học.
    pub fn disable_biometric(&mut self) -> Result<(), DomainError> {
        self.storage.remove_biometric_envelope()
    }

    /// Kiểm tra xem kho mật khẩu đã được cài đặt khóa mở bằng sinh trắc học hay chưa.
    pub fn is_biometric_enabled(&self) -> bool {
        self.storage.biometric_file_path().exists()
    }

    /// Đổi tên hoặc cập nhật thông tin một Vault.
    pub fn rename_vault(&mut self, vault_id: &str, name_or_json: &str) -> Result<(), DomainError> {
        if !self.unlocked {
            return Err(DomainError::Locked);
        }
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
                if let Some(desc) = meta.get_string("description") {
                    v.description = desc;
                }
                self.persist()?;
                return Ok(());
            }
            return Err(DomainError::NotFound(format!("Vault {vault_id} not found")));
        }

        if let Some(v) = self.vaults.iter_mut().find(|v| v.id == vault_id) {
            v.name = trimmed.to_string();
            self.persist()?;
            Ok(())
        } else {
            Err(DomainError::NotFound(format!("Vault {vault_id} not found")))
        }
    }

    /// Xóa một Vault (không cho xóa Vault mặc định).
    pub fn delete_vault(&mut self, vault_id: &str, password: &str) -> Result<(), DomainError> {
        if !password.is_empty() && self.storage.vault_exists() {
            let (_, mut dek, mut json_str) = self.storage.unlock_with_password(password)?;
            zeroize_string(&mut json_str);
            zeroize_slice(&mut dek);
        } else if !self.unlocked {
            return Err(DomainError::Locked);
        }

        if let Some(pos) = self.vaults.iter().position(|v| v.id == vault_id) {
            if self.vaults[pos].is_default {
                return Err(DomainError::Conflict(
                    "Cannot delete the default vault".to_string(),
                ));
            }
            self.vaults.remove(pos);
            self.items.retain(|it| it.vault_id != vault_id);
            self.persist()?;
            Ok(())
        } else {
            Err(DomainError::NotFound(format!("Vault {vault_id} not found")))
        }
    }

    /// Đặt một Vault làm kho mặc định.
    pub fn set_default_vault(&mut self, vault_id: &str) -> Result<(), DomainError> {
        if !self.unlocked {
            return Err(DomainError::Locked);
        }
        let exists = self.vaults.iter().any(|v| v.id == vault_id);
        if !exists {
            return Err(DomainError::NotFound(format!("Vault {vault_id} not found")));
        }
        for v in &mut self.vaults {
            v.is_default = v.id == vault_id;
        }
        self.persist()?;
        Ok(())
    }

    /// Đổi Master Password (tái mã hóa DEK bằng KEK mới).
    pub fn change_password(&mut self, old_pw: &str, new_pw: &str) -> Result<(), DomainError> {
        if new_pw.is_empty() {
            return Err(DomainError::InvalidArg(
                "New password cannot be empty".to_string(),
            ));
        }
        if !self.storage.vault_exists() {
            return Err(DomainError::NotFound(
                "Vault has not been initialized".to_string(),
            ));
        }
        let (mut header, mut dek, mut json_str) = self.storage.unlock_with_password(old_pw)?;
        let load_res = self.load_state_from_json(&json_str);
        zeroize_string(&mut json_str);
        load_res?;
        self.dek = dek;
        zeroize_slice(&mut dek);
        VaultStorage::rewrap_master_password(&mut header, &self.dek, new_pw);
        self.header = Some(header);
        mem_lock(&self.dek);
        self.unlocked = true;
        self.persist()?;
        Ok(())
    }

    /// Tạo lại và xoay Recovery Key mới (tái mã hóa DEK với KEK của recovery key mới).
    pub fn rotate_recovery_key(&mut self, master_pw: &str) -> Result<String, DomainError> {
        if !self.storage.vault_exists() {
            return Err(DomainError::NotFound(
                "Vault has not been initialized".to_string(),
            ));
        }
        let new_key = generate_recovery_key();
        let (mut header, mut dek, mut json_str) = self.storage.unlock_with_password(master_pw)?;
        let load_res = self.load_state_from_json(&json_str);
        zeroize_string(&mut json_str);
        load_res?;
        self.dek = dek;
        zeroize_slice(&mut dek);
        VaultStorage::rewrap_recovery_key(&mut header, &self.dek, &new_key);
        self.header = Some(header);
        self.recovery_key = new_key.clone();
        mem_lock(&self.dek);
        self.unlocked = true;
        self.persist()?;
        Ok(new_key)
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
