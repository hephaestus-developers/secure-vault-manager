use std::path::{Path, PathBuf};

fn copy_dir_all(src: &Path, dst: &Path) {
    if !src.exists() {
        return;
    }
    let _ = std::fs::create_dir_all(dst);
    if let Ok(entries) = std::fs::read_dir(src) {
        for entry in entries.flatten() {
            let path = entry.path();
            let dest_path = dst.join(entry.file_name());
            if path.is_dir() {
                copy_dir_all(&path, &dest_path);
            } else {
                let _ = std::fs::remove_file(&dest_path);
                if let Ok(content) = std::fs::read(&path) {
                    let _ = std::fs::write(&dest_path, content);
                }
            }
        }
    }
}

fn main() {
    cxx_build::bridge("src/bridge.rs")
        .std("c++20")
        .compile("svm_cxxbridge");

    println!("cargo:rerun-if-changed=src/bridge.rs");

    // Copy generated cxxbridge headers (rust/cxx.h, bridge.rs.h) and source to target/cxxbridge
    let out_dir = PathBuf::from(std::env::var("OUT_DIR").unwrap());
    let manifest_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let target_cxxbridge = manifest_dir.join("../../target/cxxbridge");

    // 1. Copy include tree (contains rust/cxx.h and svm-core/src/bridge.rs.h)
    let inc_dir = out_dir.join("cxxbridge/include");
    copy_dir_all(&inc_dir, &target_cxxbridge);

    // 2. Copy bridge.rs.cc source file
    let src_cc = out_dir.join("cxxbridge/sources/svm-core/src/bridge.rs.cc");
    let dest_src_dir = target_cxxbridge.join("svm-core/src");
    let _ = std::fs::create_dir_all(&dest_src_dir);
    if src_cc.exists() {
        let dest_cc = dest_src_dir.join("bridge.rs.cc");
        let _ = std::fs::remove_file(&dest_cc);
        if let Ok(content) = std::fs::read(&src_cc) {
            let _ = std::fs::write(&dest_cc, content);
        }
    }
}
