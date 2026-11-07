//! Dữ liệu hồ sơ Vault mặc định và các mục mẫu khởi tạo ban đầu cho kho dữ liệu.

use std::time::{SystemTime, UNIX_EPOCH};

use svm_domain::{Category, CustomField, FieldKind, VaultItem, VaultProfile};

/// Lấy dấu thời gian Unix Epoch hiện tại tính bằng mili-giây.
pub fn current_timestamp_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .unwrap_or(1_700_000_000_000)
}

/// Tạo danh sách hồ sơ Vault mặc định (`personal` và `work`).
pub fn build_default_vaults(primary_name: &str) -> Vec<VaultProfile> {
    let now = current_timestamp_ms();
    let name = if primary_name.trim().is_empty() {
        "Personal Vault"
    } else {
        primary_name.trim()
    };
    vec![
        VaultProfile {
            id: "personal".to_string(),
            name: name.to_string(),
            icon_name: "folder_open".to_string(),
            accent_color: "#3B82F6".to_string(),
            description: "Primary personal credentials & logins".to_string(),
            item_count: 2,
            is_default: true,
            created_at: now,
        },
        VaultProfile {
            id: "work".to_string(),
            name: "Work Vault".to_string(),
            icon_name: "folder_empty".to_string(),
            accent_color: "#8B5CF6".to_string(),
            description: "Company, cloud & server credentials".to_string(),
            item_count: 4,
            is_default: false,
            created_at: now,
        },
    ]
}

/// Tạo danh sách các mục mẫu ban đầu thuộc các danh mục chuẩn.
pub fn build_default_sample_items() -> Vec<VaultItem> {
    let now = current_timestamp_ms();
    let day_ms = 86_400_000i64;

    let git = VaultItem {
        id: "item-001".to_string(),
        vault_id: "personal".to_string(),
        title: "GitHub (Work Account)".to_string(),
        category: Category::Login,
        username: "octocat@hephaestus.io".to_string(),
        password: "ghp_9284Fja9028Fh2q8fh0283fh90q".to_string(),
        website_url: "https://github.com/login".to_string(),
        notes: "Main engineering organization account with SSH & 2FA enabled.".to_string(),
        totp_secret: "JBSWY3DPEHPK3PXP".to_string(),
        favorite: true,
        has_totp: true,
        custom_fields: vec![
            CustomField {
                id: "cf-git-1".to_string(),
                label: "Personal Access Token".to_string(),
                value: "ghp_98418fha09283fhkjashf9283".to_string(),
                kind: FieldKind::Concealed,
            },
            CustomField {
                id: "cf-git-2".to_string(),
                label: "Organization Team".to_string(),
                value: "Core Infrastructure & Security".to_string(),
                kind: FieldKind::Text,
            },
            CustomField {
                id: "cf-git-3".to_string(),
                label: "Hardware Key (FIDO2) Required".to_string(),
                value: "true".to_string(),
                kind: FieldKind::Boolean,
            },
            CustomField {
                id: "cf-git-4".to_string(),
                label: "Token Expiration Date".to_string(),
                value: "2026-12-31".to_string(),
                kind: FieldKind::Date,
            },
        ],
        created_at: now - 10 * day_ms,
        updated_at: now - 2 * day_ms,
        password_strength_score: 92,
        ..Default::default()
    };

    let aws = VaultItem {
        id: "item-002".to_string(),
        vault_id: "work".to_string(),
        title: "AWS Console (Production)".to_string(),
        category: Category::Login,
        username: "devops-admin@corp.internal".to_string(),
        password: "K8s!Aws#Secure2026$Production".to_string(),
        website_url: "https://aws.amazon.com/console".to_string(),
        totp_secret: "HXDMVJECJJWSRB3H".to_string(),
        favorite: true,
        has_totp: true,
        custom_fields: vec![
            CustomField {
                id: "cf-aws-1".to_string(),
                label: "AWS Account ID".to_string(),
                value: "8491-0294-8172".to_string(),
                kind: FieldKind::Text,
            },
            CustomField {
                id: "cf-aws-2".to_string(),
                label: "IAM Role Name".to_string(),
                value: "DevOpsAdministratorAccess".to_string(),
                kind: FieldKind::Text,
            },
        ],
        created_at: now - 15 * day_ms,
        updated_at: now - 5 * day_ms,
        password_strength_score: 96,
        ..Default::default()
    };

    let ssh = VaultItem {
        id: "item-003".to_string(),
        vault_id: "work".to_string(),
        title: "id_ed25519 (Prod Bastion)".to_string(),
        category: Category::SshKey,
        username: "root@bastion.infra.net".to_string(),
        password: "Passphrase#SSH#2026".to_string(),
        ssh_fingerprint: "SHA256:7mP0e5K2q9N8w1X4v6Z3t7Y0u5W2r8O1p4Q7s".to_string(),
        ssh_public_key:
            "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIExampleProdKeyhephaestus root@bastion".to_string(),
        is_ssh_loaded: true,
        created_at: now - 20 * day_ms,
        updated_at: now - 10 * day_ms,
        password_strength_score: 88,
        ..Default::default()
    };

    let db = VaultItem {
        id: "item-004".to_string(),
        vault_id: "work".to_string(),
        title: "Postgres Cloud Cluster".to_string(),
        category: Category::Database,
        username: "postgres_superadmin".to_string(),
        password: "PgSQL#SecurePass!Cluster2026".to_string(),
        db_host: "pg-primary.prod.svc.cluster.local:5432".to_string(),
        db_port: 5432,
        db_name: "vault_core_db".to_string(),
        created_at: now - 7 * day_ms,
        updated_at: now - day_ms,
        password_strength_score: 94,
        ..Default::default()
    };

    let wifi = VaultItem {
        id: "item-005".to_string(),
        vault_id: "work".to_string(),
        title: "Hephaestus Studio Wi-Fi".to_string(),
        category: Category::Wifi,
        wifi_ssid: "Hephaestus_5GHz_HighSec".to_string(),
        password: "IronForge#Wifi#2026".to_string(),
        created_at: now - 40 * day_ms,
        updated_at: now - 30 * day_ms,
        password_strength_score: 90,
        ..Default::default()
    };

    let card = VaultItem {
        id: "item-006".to_string(),
        vault_id: "personal".to_string(),
        title: "Corporate Visa Platinum".to_string(),
        category: Category::Card,
        username: "HAI PHAM NGOC".to_string(),
        card_number: "4111 2222 3333 4444".to_string(),
        card_expiry: "12/28".to_string(),
        card_cvv: "888".to_string(),
        created_at: now - 25 * day_ms,
        updated_at: now - 15 * day_ms,
        password_strength_score: 85,
        ..Default::default()
    };

    vec![git, aws, ssh, db, wifi, card]
}
