//! # item
//!
//! Thực thể cốt lõi `VaultItem` và `VaultProfile` đại diện cho hồ sơ kho và mục dữ liệu bí mật.

use crate::category::Category;
use crate::custom_field::CustomField;
use crate::json::JsonValue;

/// Hồ sơ một kho lưu trữ độc lập (Vault Profile).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VaultProfile {
    /// Định danh duy nhất của Vault (ví dụ: "personal", "work", hoặc UUID).
    pub id: String,
    /// Tên hiển thị của Vault.
    pub name: String,
    /// Tên biểu tượng của Vault.
    pub icon_name: String,
    /// Mô tả ngắn về mục đích sử dụng của Vault.
    pub description: String,
    /// Số lượng mục lưu trữ hiện có trong Vault.
    pub item_count: i32,
    /// Cờ đánh dấu Vault mặc định.
    pub is_default: bool,
    /// Dấu thời gian tạo (Unix timestamp ms).
    pub created_at: i64,
}

impl VaultProfile {
    /// Tuần tự hóa `VaultProfile` sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("name", JsonValue::String(self.name.clone()));
        obj.insert("iconName", JsonValue::String(self.icon_name.clone()));
        obj.insert("description", JsonValue::String(self.description.clone()));
        obj.insert("itemCount", JsonValue::Number(self.item_count as f64));
        obj.insert("isDefault", JsonValue::Bool(self.is_default));
        obj.insert("createdAt", JsonValue::Number(self.created_at as f64));
        obj
    }

    /// Khôi phục `VaultProfile` từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let id = val.get_string("id")?;
        let name = val
            .get_string("name")
            .unwrap_or_else(|| "Untitled Vault".to_string());
        Some(Self {
            id,
            name,
            icon_name: val
                .get_string("iconName")
                .or_else(|| val.get_string("icon_name"))
                .unwrap_or_else(|| "vault_personal".to_string()),
            description: val.get_string("description").unwrap_or_default(),
            item_count: val.get_i64_or("itemCount", val.get_i64_or("item_count", 0)) as i32,
            is_default: val.get_bool_or("isDefault", val.get_bool_or("is_default", false)),
            created_at: val.get_i64_or("createdAt", val.get_i64_or("created_at", 1700000000000)),
        })
    }
}

/// Thực thể mục kho dữ liệu cơ bản chung cho toàn bộ 9 danh mục nghiệp vụ.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VaultItem {
    /// Định danh UUID phiên bản 4 duy nhất của mục.
    pub id: String,
    /// Định danh Vault chứa mục này (ví dụ: "personal", "work").
    pub vault_id: String,
    /// Tiêu đề hoặc tên gợi nhớ của mục dữ liệu.
    pub title: String,
    /// Danh mục chuyên biệt thuộc @ref Category.
    pub category: Category,
    /// Tên đăng nhập / Email / Chủ thẻ / Tài khoản.
    pub username: String,
    /// Mật khẩu / Passphrase / Secret chính.
    pub password: String,
    /// Địa chỉ Website URL / Host / Domain.
    pub website_url: String,
    /// Ghi chú an toàn đính kèm.
    pub notes: String,
    /// Khóa bí mật TOTP Base32 (nếu có).
    pub totp_secret: String,
    /// Cờ đánh dấu mục yêu thích (Favorite/Pinned).
    pub favorite: bool,
    /// Cờ đánh dấu mục đã lưu trữ (Archived).
    pub archived: bool,
    /// Cờ đánh dấu mục đã bị chuyển vào Thùng rác (xóa mềm).
    pub trashed: bool,
    /// Cờ đánh dấu có kích hoạt TOTP.
    pub has_totp: bool,
    /// Cờ đánh dấu có Passkey liên kết.
    pub has_passkey: bool,
    /// Định danh Credential ID (Base64URL) của Passkey.
    pub passkey_credential_id: String,
    /// Tên miền Relying Party ID (RP ID) của Passkey.
    pub passkey_rp_id: String,
    /// Mã thuật toán COSE của Passkey (mặc định `-7` cho ES256).
    pub passkey_algorithm: i32,
    /// Định danh User Handle (Base64URL) của Passkey.
    pub passkey_user_handle: String,
    /// Bộ đếm số lần ký xác thực Passkey (`signCount`).
    pub passkey_sign_count: u32,
    /// Cờ đánh dấu khóa SSH đang nạp vào Agent.
    pub is_ssh_loaded: bool,
    /// Khóa công khai OpenSSH.
    pub ssh_public_key: String,
    /// Dấu vân tay SHA256 của khóa SSH.
    pub ssh_fingerprint: String,
    /// Số thẻ thanh toán hoặc số định danh.
    pub card_number: String,
    /// Ngày hết hạn thẻ (MM/YY).
    pub card_expiry: String,
    /// Mã bảo mật CVV/CVC.
    pub card_cvv: String,
    /// Máy chủ cơ sở dữ liệu (Host:Port).
    pub db_host: String,
    /// Cổng kết nối cơ sở dữ liệu.
    pub db_port: i32,
    /// Tên cơ sở dữ liệu.
    pub db_name: String,
    /// Tên mạng Wi-Fi (SSID).
    pub wifi_ssid: String,
    /// Chuẩn bảo mật Wi-Fi (WPA2/WPA3).
    pub wifi_security: String,
    /// Danh sách các trường tùy biến do người dùng mở rộng.
    pub custom_fields: Vec<CustomField>,
    /// Dấu thời gian tạo mục (Unix timestamp tính bằng mili-giây).
    pub created_at: i64,
    /// Dấu thời gian cập nhật gần nhất (Unix timestamp tính bằng mili-giây).
    pub updated_at: i64,
    /// Điểm đánh giá độ mạnh mật khẩu (0..100).
    pub password_strength_score: i32,
}

impl Default for VaultItem {
    fn default() -> Self {
        Self {
            id: String::new(),
            vault_id: "personal".to_string(),
            title: String::new(),
            category: Category::Login,
            username: String::new(),
            password: String::new(),
            website_url: String::new(),
            notes: String::new(),
            totp_secret: String::new(),
            favorite: false,
            archived: false,
            trashed: false,
            has_totp: false,
            has_passkey: false,
            passkey_credential_id: String::new(),
            passkey_rp_id: String::new(),
            passkey_algorithm: 0,
            passkey_user_handle: String::new(),
            passkey_sign_count: 0,
            is_ssh_loaded: false,
            ssh_public_key: String::new(),
            ssh_fingerprint: String::new(),
            card_number: String::new(),
            card_expiry: String::new(),
            card_cvv: String::new(),
            db_host: String::new(),
            db_port: 5432,
            db_name: String::new(),
            wifi_ssid: String::new(),
            wifi_security: "WPA2/WPA3".to_string(),
            custom_fields: Vec::new(),
            created_at: 0,
            updated_at: 0,
            password_strength_score: 85,
        }
    }
}

impl VaultItem {
    /// Khởi tạo nhanh một mục thuộc danh mục `Category::Login`.
    pub fn new_login(
        title: impl Into<String>,
        username: impl Into<String>,
        password: impl Into<String>,
        website_url: impl Into<String>,
    ) -> Self {
        Self {
            title: title.into(),
            category: Category::Login,
            username: username.into(),
            password: password.into(),
            website_url: website_url.into(),
            ..Default::default()
        }
    }

    /// Chuyển đổi `VaultItem` sang đối tượng `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("vaultId", JsonValue::String(self.vault_id.clone()));
        obj.insert("title", JsonValue::String(self.title.clone()));
        obj.insert(
            "category",
            JsonValue::String(self.category.as_str().to_string()),
        );
        obj.insert(
            "categoryIndex",
            JsonValue::Number(self.category.to_index() as f64),
        );
        obj.insert("username", JsonValue::String(self.username.clone()));
        obj.insert("password", JsonValue::String(self.password.clone()));
        obj.insert("websiteUrl", JsonValue::String(self.website_url.clone()));
        obj.insert("notes", JsonValue::String(self.notes.clone()));
        obj.insert("totpSecret", JsonValue::String(self.totp_secret.clone()));
        obj.insert("isFavorite", JsonValue::Bool(self.favorite));
        obj.insert("isArchived", JsonValue::Bool(self.archived));
        obj.insert("isDeleted", JsonValue::Bool(self.trashed));
        obj.insert(
            "hasTotp",
            JsonValue::Bool(self.has_totp || !self.totp_secret.is_empty()),
        );
        obj.insert("hasPasskey", JsonValue::Bool(self.has_passkey));
        obj.insert(
            "passkeyCredentialId",
            JsonValue::String(self.passkey_credential_id.clone()),
        );
        obj.insert("passkeyRpId", JsonValue::String(self.passkey_rp_id.clone()));
        obj.insert(
            "passkeyAlgorithm",
            JsonValue::Number(self.passkey_algorithm as f64),
        );
        obj.insert(
            "passkeyUserHandle",
            JsonValue::String(self.passkey_user_handle.clone()),
        );
        obj.insert(
            "passkeySignCount",
            JsonValue::Number(self.passkey_sign_count as f64),
        );
        obj.insert("isSshLoaded", JsonValue::Bool(self.is_ssh_loaded));
        obj.insert(
            "sshPublicKey",
            JsonValue::String(self.ssh_public_key.clone()),
        );
        obj.insert(
            "sshFingerprint",
            JsonValue::String(self.ssh_fingerprint.clone()),
        );
        obj.insert("cardNumber", JsonValue::String(self.card_number.clone()));
        obj.insert("cardExpiry", JsonValue::String(self.card_expiry.clone()));
        obj.insert("cardCvv", JsonValue::String(self.card_cvv.clone()));
        obj.insert("dbHost", JsonValue::String(self.db_host.clone()));
        obj.insert("dbPort", JsonValue::Number(self.db_port as f64));
        obj.insert("dbName", JsonValue::String(self.db_name.clone()));
        obj.insert("wifiSsid", JsonValue::String(self.wifi_ssid.clone()));
        obj.insert(
            "wifiSecurity",
            JsonValue::String(self.wifi_security.clone()),
        );

        let mut cf_arr = JsonValue::new_array();
        for cf in &self.custom_fields {
            cf_arr.push(cf.to_json());
        }
        obj.insert("customFields", cf_arr);

        obj.insert("createdAt", JsonValue::Number(self.created_at as f64));
        obj.insert("updatedAt", JsonValue::Number(self.updated_at as f64));
        obj.insert(
            "passwordStrengthScore",
            JsonValue::Number(self.password_strength_score as f64),
        );
        obj
    }

    /// Chuyển đổi `VaultItem` sang JSON tóm tắt dùng cho danh sách (`svm_items_list`),
    /// ẩn toàn bộ trường nhạy cảm (`password`, `totpSecret`, `cardNumber`, `cardCvv`, `notes`, `customFields`).
    pub fn to_summary_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("id", JsonValue::String(self.id.clone()));
        obj.insert("vaultId", JsonValue::String(self.vault_id.clone()));
        obj.insert("title", JsonValue::String(self.title.clone()));
        obj.insert(
            "category",
            JsonValue::String(self.category.as_str().to_string()),
        );
        obj.insert(
            "categoryIndex",
            JsonValue::Number(self.category.to_index() as f64),
        );
        obj.insert("username", JsonValue::String(self.username.clone()));
        obj.insert("websiteUrl", JsonValue::String(self.website_url.clone()));
        obj.insert("isFavorite", JsonValue::Bool(self.favorite));
        obj.insert("isArchived", JsonValue::Bool(self.archived));
        obj.insert("isDeleted", JsonValue::Bool(self.trashed));
        obj.insert(
            "hasTotp",
            JsonValue::Bool(self.has_totp || !self.totp_secret.is_empty()),
        );
        obj.insert("hasPasskey", JsonValue::Bool(self.has_passkey));
        obj.insert(
            "passkeyCredentialId",
            JsonValue::String(self.passkey_credential_id.clone()),
        );
        obj.insert("passkeyRpId", JsonValue::String(self.passkey_rp_id.clone()));
        obj.insert(
            "passkeyAlgorithm",
            JsonValue::Number(self.passkey_algorithm as f64),
        );
        obj.insert(
            "passkeyUserHandle",
            JsonValue::String(self.passkey_user_handle.clone()),
        );
        obj.insert(
            "passkeySignCount",
            JsonValue::Number(self.passkey_sign_count as f64),
        );
        obj.insert("isSshLoaded", JsonValue::Bool(self.is_ssh_loaded));
        obj.insert(
            "sshPublicKey",
            JsonValue::String(self.ssh_public_key.clone()),
        );
        obj.insert(
            "sshFingerprint",
            JsonValue::String(self.ssh_fingerprint.clone()),
        );
        obj.insert("cardExpiry", JsonValue::String(self.card_expiry.clone()));
        obj.insert("dbHost", JsonValue::String(self.db_host.clone()));
        obj.insert("dbPort", JsonValue::Number(self.db_port as f64));
        obj.insert("dbName", JsonValue::String(self.db_name.clone()));
        obj.insert("wifiSsid", JsonValue::String(self.wifi_ssid.clone()));
        obj.insert(
            "wifiSecurity",
            JsonValue::String(self.wifi_security.clone()),
        );
        obj.insert("createdAt", JsonValue::Number(self.created_at as f64));
        obj.insert("updatedAt", JsonValue::Number(self.updated_at as f64));
        obj.insert(
            "passwordStrengthScore",
            JsonValue::Number(self.password_strength_score as f64),
        );
        obj
    }

    /// Khôi phục `VaultItem` từ đối tượng `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let id = val.get_string("id").unwrap_or_default();
        let vault_id = val
            .get_string("vaultId")
            .or_else(|| val.get_string("vault_id"))
            .unwrap_or_else(|| "personal".to_string());
        let title = val.get_string("title").unwrap_or_default();

        let category = if let Some(cat_str) = val.get("category").and_then(|v| v.as_str()) {
            Category::parse_str(cat_str)
        } else if let Some(cat_idx) = val
            .get("categoryIndex")
            .or_else(|| val.get("category"))
            .and_then(|v| v.as_i64())
        {
            Category::parse_str(&cat_idx.to_string())
        } else {
            Category::Login
        };

        let mut custom_fields = Vec::new();
        if let Some(arr) = val
            .get("customFields")
            .or_else(|| val.get("custom_fields"))
            .and_then(|v| v.as_array())
        {
            for cf_val in arr {
                if let Some(cf) = CustomField::from_json(cf_val) {
                    custom_fields.push(cf);
                }
            }
        }

        let totp_secret = val
            .get_string("totpSecret")
            .or_else(|| val.get_string("totp_secret"))
            .unwrap_or_default();
        let has_totp = val.get_bool_or("hasTotp", !totp_secret.is_empty());

        Some(Self {
            id,
            vault_id,
            title,
            category,
            username: val.get_string("username").unwrap_or_default(),
            password: val.get_string("password").unwrap_or_default(),
            website_url: val
                .get_string("websiteUrl")
                .or_else(|| val.get_string("website_url"))
                .unwrap_or_default(),
            notes: val.get_string("notes").unwrap_or_default(),
            totp_secret,
            favorite: val.get_bool_or("isFavorite", val.get_bool_or("favorite", false)),
            archived: val.get_bool_or("isArchived", val.get_bool_or("archived", false)),
            trashed: val.get_bool_or("isDeleted", val.get_bool_or("trashed", false)),
            has_totp,
            has_passkey: val.get_bool_or("hasPasskey", false),
            passkey_credential_id: val.get_string("passkeyCredentialId").unwrap_or_default(),
            passkey_rp_id: val.get_string("passkeyRpId").unwrap_or_default(),
            passkey_algorithm: val.get_i64_or("passkeyAlgorithm", 0) as i32,
            passkey_user_handle: val.get_string("passkeyUserHandle").unwrap_or_default(),
            passkey_sign_count: val.get_i64_or("passkeySignCount", 0).max(0) as u32,
            is_ssh_loaded: val.get_bool_or("isSshLoaded", false),
            ssh_public_key: val.get_string("sshPublicKey").unwrap_or_default(),
            ssh_fingerprint: val.get_string("sshFingerprint").unwrap_or_default(),
            card_number: val.get_string("cardNumber").unwrap_or_default(),
            card_expiry: val.get_string("cardExpiry").unwrap_or_default(),
            card_cvv: val.get_string("cardCvv").unwrap_or_default(),
            db_host: val.get_string("dbHost").unwrap_or_default(),
            db_port: val.get_i64_or("dbPort", 5432) as i32,
            db_name: val.get_string("dbName").unwrap_or_default(),
            wifi_ssid: val.get_string("wifiSsid").unwrap_or_default(),
            wifi_security: val
                .get_string("wifiSecurity")
                .unwrap_or_else(|| "WPA2/WPA3".to_string()),
            custom_fields,
            created_at: val.get_i64_or("createdAt", 0),
            updated_at: val.get_i64_or("updatedAt", 0),
            password_strength_score: val.get_i64_or("passwordStrengthScore", 85) as i32,
        })
    }
}

/// Bản ghi lịch sử sửa đổi của một `VaultItem`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ItemRevision {
    /// Số thứ tự bản sửa đổi (bắt đầu từ 1).
    pub revision: u32,
    /// Dấu thời gian tạo bản sửa đổi (Unix ms).
    pub timestamp: i64,
    /// Bản chụp trạng thái `VaultItem` tại thời điểm trước khi ghi đè.
    pub snapshot: VaultItem,
}

impl ItemRevision {
    /// Chuyển đổi `ItemRevision` sang `JsonValue`.
    pub fn to_json(&self) -> JsonValue {
        let mut obj = JsonValue::new_object();
        obj.insert("revision", JsonValue::Number(self.revision as f64));
        obj.insert("timestamp", JsonValue::Number(self.timestamp as f64));
        obj.insert("snapshot", self.snapshot.to_json());
        obj
    }

    /// Khôi phục `ItemRevision` từ `JsonValue`.
    pub fn from_json(val: &JsonValue) -> Option<Self> {
        let revision = val.get_i64_or("revision", 1) as u32;
        let timestamp = val.get_i64_or("timestamp", 0);
        let snapshot = VaultItem::from_json(val.get("snapshot")?)?;
        Some(Self {
            revision,
            timestamp,
            snapshot,
        })
    }
}
