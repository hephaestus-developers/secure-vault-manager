//! Bộ phân tích cú pháp nhập dữ liệu từ các định dạng SVM Encrypted Backup, Bitwarden JSON, 1Password .1pux (ZIP/JSON), KeePass XML và CSV.

use std::fs;
use std::io::{Cursor, Read};
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
    let raw_bytes = fs::read(src_path).map_err(|e| DomainError::Io(e.to_string()))?;
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
        if password.is_empty() {
            return Err(DomainError::InvalidArg(
                "Password is required to decrypt SVM backup".to_string(),
            ));
        }
        let kek = derive_kek(password, &salt);
        let plain = chacha20_poly1305_decrypt(&kek, &nonce, AAD_BACKUP, &raw_bytes[32..])?;
        let text = String::from_utf8(plain).map_err(|e| DomainError::Parse(e.to_string()))?;
        return parse_json_items(&text);
    }

    // Nếu tệp là .1pux chuẩn (ZIP archive chứa export.data)
    if raw_bytes.len() >= 4 && raw_bytes[0..4] == [0x50, 0x4B, 0x03, 0x04] {
        return parse_onepux_zip(raw_bytes);
    }

    let text = String::from_utf8_lossy(raw_bytes);
    let trimmed = text.trim();

    match format {
        TransferFormat::OnePux => {
            if trimmed.starts_with('{') || trimmed.starts_with('[') {
                parse_onepux_or_json(trimmed)
            } else {
                Err(DomainError::InvalidArg(
                    "Invalid .1pux file: expected ZIP archive or JSON".to_string(),
                ))
            }
        }
        TransferFormat::BitwardenJson | TransferFormat::Svm => {
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

fn score_for_password(password: &str) -> i32 {
    if password.is_empty() {
        0
    } else {
        evaluate_password_strength(password).percent
    }
}

fn parse_onepux_zip(raw_bytes: &[u8]) -> Result<Vec<VaultItem>, DomainError> {
    let reader = Cursor::new(raw_bytes);
    let mut archive = zip::ZipArchive::new(reader)
        .map_err(|e| DomainError::Parse(format!("Invalid .1pux ZIP archive: {e}")))?;

    let mut data_file = archive
        .by_name("export.data")
        .map_err(|e| DomainError::Parse(format!("Missing export.data inside .1pux: {e}")))?;
    let mut json_text = String::new();
    data_file
        .read_to_string(&mut json_text)
        .map_err(|e| DomainError::Parse(format!("Unreadable export.data inside .1pux: {e}")))?;
    parse_onepux_or_json(&json_text)
}

fn parse_onepux_or_json(json_str: &str) -> Result<Vec<VaultItem>, DomainError> {
    let val = JsonValue::parse(json_str).map_err(DomainError::Parse)?;
    let Some(accounts) = val.get("accounts").and_then(|v| v.as_array()) else {
        return parse_json_items(json_str);
    };

    let now = now_ms();
    let mut items = Vec::new();
    for acc in accounts {
        let Some(vaults) = acc.get("vaults").and_then(|v| v.as_array()) else {
            continue;
        };
        for vault in vaults {
            let Some(v_items) = vault.get("items").and_then(|v| v.as_array()) else {
                continue;
            };
            for entry in v_items {
                if let Some(parsed) = parse_onepux_entry(entry, now) {
                    items.push(parsed);
                }
            }
        }
    }
    Ok(items)
}

fn parse_onepux_entry(entry: &JsonValue, now: i64) -> Option<VaultItem> {
    let overview = entry.get("overview");
    let details = entry.get("details");
    let title = overview
        .and_then(|o| o.get_string("title"))
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| "1Password Item".to_string());

    let mut website_url = overview
        .and_then(|o| o.get_string("url"))
        .unwrap_or_default();
    if website_url.is_empty() {
        if let Some(urls) = overview
            .and_then(|o| o.get("urls"))
            .and_then(|u| u.as_array())
        {
            if let Some(first) = urls.first() {
                website_url = first.get_string("url").unwrap_or_default();
            }
        }
    }

    let cat_uuid = entry.get_string("categoryUuid").unwrap_or_default();
    let category = match cat_uuid.as_str() {
        "002" => Category::Card,
        "003" => Category::SecureNote,
        "004" => Category::Identity,
        "102" => Category::Database,
        "109" => Category::Wifi,
        "110" | "111" => Category::Server,
        "114" => Category::SshKey,
        _ => Category::Login,
    };

    let favorite = entry.get_i64_or("favIndex", 0) > 0;
    let trashed = entry.get_string("state").as_deref() == Some("trashed");
    let mut item = VaultItem {
        id: generate_uuid_v4(),
        vault_id: String::new(),
        title,
        category,
        website_url,
        favorite,
        trashed,
        created_at: now,
        updated_at: now,
        ..Default::default()
    };

    if let Some(det) = details {
        item.notes = det.get_string("notesPlain").unwrap_or_default();
        if let Some(login_fields) = det.get("loginFields").and_then(|v| v.as_array()) {
            for lf in login_fields {
                let desig = lf.get_string("designation").unwrap_or_default();
                let val = lf.get_string("value").unwrap_or_default();
                if desig == "username" && item.username.is_empty() {
                    item.username = val;
                } else if desig == "password" && item.password.is_empty() {
                    item.password = val;
                }
            }
        }
        if let Some(sections) = det.get("sections").and_then(|v| v.as_array()) {
            for sec in sections {
                let Some(fields) = sec.get("fields").and_then(|v| v.as_array()) else {
                    continue;
                };
                for f in fields {
                    let label = f.get_string("title").unwrap_or_default();
                    let val_obj = f.get("value");
                    if let Some(totp_val) = val_obj.and_then(|v| v.get_string("totp")) {
                        if !totp_val.is_empty() && item.totp_secret.is_empty() {
                            item.totp_secret = totp_val;
                            item.has_totp = true;
                            continue;
                        }
                    }
                    let text_val = val_obj
                        .and_then(|v| {
                            v.get_string("string")
                                .or_else(|| v.get_string("concealed"))
                                .or_else(|| v.get_string("email"))
                                .or_else(|| v.get_string("url"))
                                .or_else(|| v.get_string("phone"))
                                .or_else(|| v.get_string("totp"))
                        })
                        .unwrap_or_default();
                    if label.is_empty() || text_val.is_empty() {
                        continue;
                    }
                    let is_concealed = val_obj.and_then(|v| v.get("concealed")).is_some();
                    item.custom_fields.push(CustomField {
                        id: generate_uuid_v4(),
                        label,
                        value: text_val,
                        kind: if is_concealed {
                            FieldKind::Concealed
                        } else {
                            FieldKind::Text
                        },
                    });
                }
            }
        }
    }

    item.password_strength_score = score_for_password(&item.password);
    Some(item)
}

fn parse_json_items(json_str: &str) -> Result<Vec<VaultItem>, DomainError> {
    let val = JsonValue::parse(json_str).map_err(DomainError::Parse)?;
    if val.get_bool_or("encrypted", false) && val.get("items").is_none() {
        return Err(DomainError::InvalidArg(
            "Encrypted Bitwarden JSON cannot be imported directly; please export unencrypted JSON"
                .to_string(),
        ));
    }

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
                it.password_strength_score = score_for_password(&it.password);
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
        let bw_type = entry.get_i64_or("type", 0);

        let mut item = VaultItem {
            id: generate_uuid_v4(),
            vault_id: String::new(),
            title,
            notes,
            favorite,
            created_at: now,
            updated_at: now,
            ..Default::default()
        };

        if bw_type == 2 || entry.get("secureNote").is_some() {
            item.category = Category::SecureNote;
        } else if bw_type == 3 || entry.get("card").is_some() {
            item.category = Category::Card;
            if let Some(card) = entry.get("card") {
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
            }
        } else if bw_type == 4 || entry.get("identity").is_some() {
            item.category = Category::Identity;
            if let Some(id_obj) = entry.get("identity") {
                item.username = id_obj
                    .get_string("email")
                    .or_else(|| id_obj.get_string("username"))
                    .unwrap_or_default();
                for (k, label) in [
                    ("firstName", "First Name"),
                    ("lastName", "Last Name"),
                    ("phone", "Phone"),
                    ("address1", "Address"),
                    ("city", "City"),
                    ("country", "Country"),
                    ("company", "Company"),
                ] {
                    if let Some(v) = id_obj.get_string(k).filter(|s| !s.is_empty()) {
                        item.custom_fields.push(CustomField {
                            id: generate_uuid_v4(),
                            label: label.to_string(),
                            value: v,
                            kind: FieldKind::Text,
                        });
                    }
                }
            }
        } else if let Some(ssh) = entry.get("sshKey") {
            item.category = Category::SshKey;
            item.ssh_public_key = ssh.get_string("publicKey").unwrap_or_default();
            item.ssh_fingerprint = ssh.get_string("keyFingerprint").unwrap_or_default();
            let priv_k = ssh.get_string("privateKey").unwrap_or_default();
            if !priv_k.is_empty() {
                item.notes = priv_k;
            }
        } else if let Some(login) = entry.get("login") {
            item.category = Category::Login;
            item.username = login.get_string("username").unwrap_or_default();
            item.password = login.get_string("password").unwrap_or_default();
            item.totp_secret = login.get_string("totp").unwrap_or_default();
            item.has_totp = !item.totp_secret.is_empty();
            if let Some(uris) = login.get("uris").and_then(|u| u.as_array()) {
                for (idx, uri_obj) in uris.iter().enumerate() {
                    let uri = uri_obj.get_string("uri").unwrap_or_default();
                    if idx == 0 {
                        item.website_url = uri;
                    } else if !uri.is_empty() {
                        item.custom_fields.push(CustomField {
                            id: generate_uuid_v4(),
                            label: format!("URL {}", idx + 1),
                            value: uri,
                            kind: FieldKind::Text,
                        });
                    }
                }
            }
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

        item.password_strength_score = score_for_password(&item.password);
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
            vault_id: String::new(),
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

fn strip_xml_history_blocks(xml: &str) -> String {
    let mut cleaned = String::with_capacity(xml.len());
    let mut rest = xml;
    while let Some(start) = rest.find("<History>") {
        cleaned.push_str(&rest[..start]);
        if let Some(end_rel) = rest[start..].find("</History>") {
            rest = &rest[start + end_rel + "</History>".len()..];
        } else {
            rest = "";
            break;
        }
    }
    cleaned.push_str(rest);
    cleaned
}

fn parse_keepass_xml(xml: &str) -> Vec<VaultItem> {
    let sanitized = strip_xml_history_blocks(xml);
    let mut items = Vec::new();
    let now = now_ms();

    for entry_block in sanitized.split("<Entry>").skip(1) {
        let end = entry_block.find("</Entry>").unwrap_or(entry_block.len());
        let block = &entry_block[..end];

        let mut title = String::new();
        let mut username = String::new();
        let mut password = String::new();
        let mut url = String::new();
        let mut notes = String::new();
        let mut totp_secret = String::new();
        let mut custom_fields = Vec::new();

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
                "TOTP Seed" | "otp" => totp_secret = val,
                _ if !key.is_empty() && !val.is_empty() => {
                    let is_protected = sb.contains("Protected=\"True\"");
                    custom_fields.push(CustomField {
                        id: generate_uuid_v4(),
                        label: key,
                        value: val,
                        kind: if is_protected {
                            FieldKind::Concealed
                        } else {
                            FieldKind::Text
                        },
                    });
                }
                _ => {}
            }
        }

        if !title.is_empty() || !username.is_empty() || !password.is_empty() {
            let score = score_for_password(&password);
            items.push(VaultItem {
                id: generate_uuid_v4(),
                vault_id: String::new(),
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
                has_totp: !totp_secret.is_empty(),
                totp_secret,
                custom_fields,
                created_at: now,
                updated_at: now,
                password_strength_score: score,
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
