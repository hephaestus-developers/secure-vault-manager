//! # svm-native-host
//!
//! Tiến trình Native Messaging Host Relay nhị phân độc lập.
//!
//! Đóng vai trò là cầu nối chuyển tiếp tin nhắn giữa WebExtension (giao tiếp qua Stdio
//! với framing 4-byte Little-Endian) và ứng dụng chính `secure-vault-manager` (qua Unix Domain Socket
//! hoặc bộ xử lý `VaultService` nội bộ).

pub mod launcher;
pub mod stdio;

use std::env;
use std::io;
use std::process::ExitCode;

use crate::launcher::NativeHostRouter;
use crate::stdio::{read_native_message, write_native_message};

fn print_usage() {
    println!("svm-native-host v0.1.0 - Secure Vault Manager Native Messaging Host");
    println!("Usage:");
    println!("  svm-native-host                  Run in WebExtension Native Messaging stdio mode");
    println!("  svm-native-host --version        Print version information");
    println!("  svm-native-host --self-test      Run internal framing & protocol self-test");
    println!("  svm-native-host --eval <JSON>    Evaluate a single JSON bridge request and print response");
}

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();

    if args.len() > 1 {
        match args[1].as_str() {
            "--version" | "-v" => {
                println!("svm-native-host 0.1.0");
                return ExitCode::SUCCESS;
            }
            "--help" | "-h" => {
                print_usage();
                return ExitCode::SUCCESS;
            }
            "--self-test" => {
                let mut router = NativeHostRouter::new(None);
                let resp = router
                    .handle_message(r#"{"requestId":"selftest","action":"get_vault_status"}"#);
                if resp.contains(r#""ok":true"#) {
                    println!("SELF-TEST OK: {resp}");
                    return ExitCode::SUCCESS;
                } else {
                    eprintln!("SELF-TEST FAILED: {resp}");
                    return ExitCode::FAILURE;
                }
            }
            "--eval" => {
                let req = args.get(2).map(|s| s.as_str()).unwrap_or("{}");
                let mut router = NativeHostRouter::new(None);
                println!("{}", router.handle_message(req));
                return ExitCode::SUCCESS;
            }
            _ => {
                // Trình duyệt Chrome truyền `chrome-extension://<id>/` hoặc Firefox truyền đường dẫn manifest
                // ở tham số dòng lệnh đầu tiên khi gọi Native Host. Tiếp tục vào chế độ stdio loop.
            }
        }
    }

    let mut stdin = io::stdin().lock();
    let mut stdout = io::stdout().lock();
    let mut router = NativeHostRouter::new(None);

    loop {
        match read_native_message(&mut stdin) {
            Ok(Some(req_json)) => {
                if req_json.trim().is_empty() {
                    continue;
                }
                let resp_json = router.handle_message(&req_json);
                if write_native_message(&mut stdout, &resp_json).is_err() {
                    break;
                }
            }
            Ok(None) => break,
            Err(err) => {
                let err_resp = format!(
                    r#"{{"requestId":"unknown","type":"error","ok":false,"error":{{"code":"STDIO_ERROR","message":"{}"}}}}"#,
                    err.message()
                );
                let _ = write_native_message(&mut stdout, &err_resp);
                break;
            }
        }
    }

    ExitCode::SUCCESS
}
