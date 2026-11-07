//! Bộ phân tích cú pháp nhập dữ liệu từ các định dạng SVM Encrypted Backup, Bitwarden JSON, 1Password JSON, KeePass XML và CSV.

use std::fs;
use std::time::{SystemTime, UNIX_EPOCH};

use svm_crypto::{
    chacha20_poly1305_decrypt, derive_kek, evaluate_password_strength, generate_uuid_v4,
};
use svm_domain::{Category, CustomField, DomainError, FieldKind, JsonValue, VaultItem};

use crate::format::{TransferFormat, AAD_BACKUP, BACKUP_MAGIC};

fn now_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .unwrap_or(1_700_000_000_000)
}

/// Phân tích tệp nguồn và trích xuất danh sách `VaultItem` để xem trước hoặc nhập vào kho.
pub fn parse_import_file(
    format: TransferFormat,
    src_path: &str,
    password: &str,
) -> Result<Vec<VaultItem>, DomainError> {
    let raw_bytes = match fs::read(src_path) {
        Ok(b) => b,
        Err(_) => return Ok(Vec::new()),
    };
    parse_import_bytes(format, &raw_bytes, password)
}

/// Phân tích mảng byte dữ liệu nguồn theo định dạng `TransferFormat`.
pub fn parse_import_bytes(
    format: TransferFormat,
    raw_bytes: &[u8],
    password: &str,
) -> Result<Vec<VaultItem>, DomainError> {
    if raw_bytes.is_empty() {
        return Ok(Vec::new());
    }

    // Nếu tệp bắt đầu bằng magic SVMB (Encrypted Backup)
    if raw_bytes.len() > 32 && &raw_bytes[0..4] == BACKUP_MAGIC {
        let mut salt = [0u8; 16];
        salt.copy_from_slice(&raw_bytes[4..20]);
        let mut nonce = [0u8; 12];
        nonce.copy_from_slice(&raw_bytes[20..32]);
        let pw = if password.is_empty() {
            "svm-default-backup-key"
        } else {
            password
        };
        let kek = derive_kek(pw, &salt);
        let plain = chacha20_poly1305_decrypt(&kek, &nonce, AAD_BACKUP, &raw_bytes[32..])?;
        let text = String::from_utf8(plain).map_err(|e| DomainError::Parse(e.to_string()))?;
        return parse_json_items(&text);
    }

    let text = String::from_utf8_lossy(raw_bytes);
    let trimmed = text.trim();

    match format {
        TransferFormat::BitwardenJson | TransferFormat::OnePux | TransferFormat::Svm => {
            if trimmed.starts_with('{') || trimmed.starts_with('[') {
                parse_json_items(trimmed)
            } else if trimmed.starts_with('<') {
                Ok(parse_keepass_xml(trimmed))
            } else {
                Ok(parse_csv_items(trimmed))
            }
        }
        TransferFormat::KeePassXml => {
            if trimmed.starts_with('<') {
                Ok(parse_keepass_xml(trimmed))
            } else if trimmed.starts_with('{') {
                parse_json_items(trimmed)
            } else {
                Ok(parse_csv_items(trimmed))
            }
        }
        TransferFormat::Csv => {
            if trimmed.starts_with('{') || trimmed.starts_with('[') {
                parse_json_items(trimmed)
            } else {
                Ok(parse_csv_items(trimmed))
            }
        }
    }
}

fn parse_json_items(json_str: &str) -> Result<Vec<VaultItem>, DomainError> {
    let val = JsonValue::parse(json_str).map_err(DomainError::Parse)?;
    let arr_slice = if let Some(arr) = val.as_array() {
        arr
    } else if let Some(arr) = val.get("items").and_then(|v| v.as_array()) {
        arr
    } else {
        return Ok(Vec::new());
    };

    let mut items = Vec::new();
    let now = now_ms();

    for entry in arr_slice {
        // Kiểm tra nếu là định dạng VaultItem trực tiếp của SVM
        if entry.get("vaultId").is_some() || entry.get("websiteUrl").is_some() {
            if let Some(mut it) = VaultItem::from_json(entry) {
                if it.id.is_empty() {
                    it.id = generate_uuid_v4();
                }
                items.push(it);
                continue;
            }
        }

        // Hỗ trợ chuẩn Bitwarden JSON & 1Password JSON
        let title = entry
            .get_string("name")
            .or_else(|| entry.get_string("title"))
            .unwrap_or_else(|| "Imported Item".to_string());
        let notes = entry.get_string("notes").unwrap_or_default();
        let favorite = entry.get_bool_or("favorite", false);

        let mut item = VaultItem {
            id: generate_uuid_v4(),
            vault_id: "personal".to_string(),
            title,
            notes,
            favorite,
            created_at: now,
            updated_at: now,
            ..Default::default()
        };

        if let Some(login) = entry.get("login") {
            item.category = Category::Login;
            item.username = login.get_string("username").unwrap_or_default();
            item.password = login.get_string("password").unwrap_or_default();
            item.totp_secret = login.get_string("totp").unwrap_or_default();
            item.has_totp = !item.totp_secret.is_empty();
            if let Some(uris) = login.get("uris").and_then(|u| u.as_array()) {
                if let Some(first_uri) = uris.first() {
                    item.website_url = first_uri.get_string("uri").unwrap_or_default();
                }
            }
        } else if let Some(card) = entry.get("card") {
            item.category = Category::Card;
            item.username = card.get_string("cardholderName").unwrap_or_default();
            item.card_number = card.get_string("number").unwrap_or_default();
            let exp_m = card.get_string("expMonth").unwrap_or_default();
            let exp_y = card.get_string("expYear").unwrap_or_default();
            if !exp_m.is_empty() && !exp_y.is_empty() {
                let short_y = if exp_y.len() == 4 {
                    &exp_y[2..]
                } else {
                    &exp_y
                };
                item.card_expiry = format!("{exp_m:0>2}/{short_y}");
            }
            item.card_cvv = card.get_string("code").unwrap_or_default();
        } else {
            item.username = entry.get_string("username").unwrap_or_default();
            item.password = entry.get_string("password").unwrap_or_default();
            item.website_url = entry.get_string("url").unwrap_or_default();
        }

        if let Some(fields) = entry.get("fields").and_then(|f| f.as_array()) {
            for f in fields {
                let label = f.get_string("name").unwrap_or_default();
                let value = f.get_string("value").unwrap_or_default();
                let ftype = f.get_i64_or("type", 0);
                let kind = if ftype == 1 {
                    FieldKind::Concealed
                } else if ftype == 2 {
                    FieldKind::Boolean
                } else {
                    FieldKind::Text
                };
                item.custom_fields.push(CustomField {
                    id: generate_uuid_v4(),
                    label,
                    value,
                    kind,
                });
            }
        }

        if !item.password.is_empty() {
            item.password_strength_score = evaluate_password_strength(&item.password).percent;
        }
        items.push(item);
    }

    Ok(items)
}

fn parse_csv_items(csv_text: &str) -> Vec<VaultItem> {
    let rows = parse_csv_rows(csv_text);
    if rows.is_empty() {
        return Vec::new();
    }

    let headers: Vec<String> = rows[0]
        .iter()
        .map(|h| h.trim().to_ascii_lowercase())
        .collect();

    let find_col = |names: &[&str]| -> Option<usize> {
        for &n in names {
            if let Some(idx) = headers.iter().position(|h| h == n || h.contains(n)) {
                return Some(idx);
            }
        }
        None
    };

    let idx_title = find_col(&["title", "name", "account"]).unwrap_or(0);
    let idx_url = find_col(&["url", "website", "login_uri"]);
    let idx_user = find_col(&["username", "user", "login_username", "email"]);
    let idx_pass = find_col(&["password", "pass", "login_password", "secret"]);
    let idx_notes = find_col(&["notes", "note", "extra", "comments"]);
    let idx_totp = find_col(&["totp", "otp", "login_totp"]);
    let idx_cat = find_col(&["category", "type"]);

    let now = now_ms();
    let mut items = Vec::new();

    for row in rows.iter().skip(1) {
        if row.iter().all(|c| c.trim().is_empty()) {
            continue;
        }
        let get_cell = |opt_idx: Option<usize>| -> String {
            opt_idx
                .and_then(|i| row.get(i))
                .map(|s| s.trim().to_string())
                .unwrap_or_default()
        };

        let title = row
            .get(idx_title)
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .unwrap_or_else(|| "Imported Credential".to_string());
        let password = get_cell(idx_pass);
        let totp_secret = get_cell(idx_totp);
        let category_str = get_cell(idx_cat);
        let category = if category_str.is_empty() {
            Category::Login
        } else {
            Category::parse_str(&category_str)
        };

        let score = if password.is_empty() {
            80
        } else {
            evaluate_password_strength(&password).percent
        };

        items.push(VaultItem {
            id: generate_uuid_v4(),
            vault_id: "personal".to_string(),
            title,
            category,
            username: get_cell(idx_user),
            password,
            website_url: get_cell(idx_url),
            notes: get_cell(idx_notes),
            has_totp: !totp_secret.is_empty(),
            totp_secret,
            created_at: now,
            updated_at: now,
            password_strength_score: score,
            ..Default::default()
        });
    }

    items
}

fn parse_csv_rows(input: &str) -> Vec<Vec<String>> {
    let mut rows = Vec::new();
    let mut current_row = Vec::new();
    let mut current_cell = String::new();
    let mut in_quotes = false;
    let mut chars = input.chars().peekable();

    while let Some(ch) = chars.next() {
        if in_quotes {
            if ch == '"' {
                if chars.peek() == Some(&'"') {
                    current_cell.push('"');
                    let _ = chars.next();
                } else {
                    in_quotes = false;
                }
            } else {
                current_cell.push(ch);
            }
        } else {
            match ch {
                '"' => in_quotes = true,
                ',' => {
                    current_row.push(std::mem::take(&mut current_cell));
                }
                '\r' => {
                    if chars.peek() == Some(&'\n') {
                        let _ = chars.next();
                    }
                    current_row.push(std::mem::take(&mut current_cell));
                    rows.push(std::mem::take(&mut current_row));
                }
                '\n' => {
                    current_row.push(std::mem::take(&mut current_cell));
                    rows.push(std::mem::take(&mut current_row));
                }
                c => current_cell.push(c),
            }
        }
    }
    if !current_cell.is_empty() || !current_row.is_empty() {
        current_row.push(current_cell);
        rows.push(current_row);
    }
    rows
}

fn parse_keepass_xml(xml: &str) -> Vec<VaultItem> {
    let mut items = Vec::new();
    let now = now_ms();

    for entry_block in xml.split("<Entry>").skip(1) {
        let end = entry_block.find("</Entry>").unwrap_or(entry_block.len());
        let block = &entry_block[..end];

        let mut title = String::new();
        let mut username = String::new();
        let mut password = String::new();
        let mut url = String::new();
        let mut notes = String::new();

        for str_block in block.split("<String>").skip(1) {
            let s_end = str_block.find("</String>").unwrap_or(str_block.len());
            let sb = &str_block[..s_end];
            let key = extract_xml_tag(sb, "Key");
            let val = extract_xml_tag(sb, "Value");
            match key.as_str() {
                "Title" => title = val,
                "UserName" => username = val,
                "Password" => password = val,
                "URL" => url = val,
                "Notes" => notes = val,
                _ => {}
            }
        }

        if !title.is_empty() || !username.is_empty() {
            items.push(VaultItem {
                id: generate_uuid_v4(),
                vault_id: "personal".to_string(),
                title: if title.is_empty() {
                    "KeePass Entry".to_string()
                } else {
                    title
                },
                category: Category::Login,
                username,
                password,
                website_url: url,
                notes,
                created_at: now,
                updated_at: now,
                password_strength_score: 85,
                ..Default::default()
            });
        }
    }

    items
}

fn extract_xml_tag(block: &str, tag: &str) -> String {
    let open = format!("<{tag}");
    let close = format!("</{tag}>");
    if let Some(start_tag) = block.find(&open) {
        if let Some(gt_rel) = block[start_tag..].find('>') {
            let content_start = start_tag + gt_rel + 1;
            if let Some(end_rel) = block[content_start..].find(&close) {
                return block[content_start..content_start + end_rel]
                    .replace("&amp;", "&")
                    .replace("&lt;", "<")
                    .replace("&gt;", ">")
                    .replace("&quot;", "\"")
                    .replace("&apos;", "'");
            }
        }
    }
    String::new()
}
