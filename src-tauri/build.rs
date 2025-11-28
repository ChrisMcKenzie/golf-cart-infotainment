fn main() {
    tauri_build::build();
    
    // Build AASDK C wrapper
    build_aasdk_wrapper();
    
    // Note: Using manual bindings in src/aasdk_bindings.rs for now
    // Can generate with bindgen later if needed
}

fn build_aasdk_wrapper() {
    use std::env;
    use std::path::PathBuf;

    // Get the absolute path of the project root (src-tauri directory)
    let manifest_dir = env::var("CARGO_MANIFEST_DIR")
        .expect("CARGO_MANIFEST_DIR should be set during build");
    let manifest_path = PathBuf::from(&manifest_dir);
    
    let wrapper_dir = manifest_path.join("aasdk-wrapper");
    let aasdk_build_dir = wrapper_dir.join("build");
    let wrapper_source = wrapper_dir.join("aasdk_c.cpp");
    let wrapper_header = wrapper_dir.join("aasdk_c.h");

    // Check if AASDK wrapper files exist
    if !wrapper_source.exists() || !wrapper_header.exists() {
        println!("cargo:warning=AASDK wrapper files not found, skipping wrapper build");
        return;
    }

    // Check if AASDK is built
    let aasdk_lib = aasdk_build_dir.join("lib").join("libaasdk.so");
    let aasdk_lib_path = aasdk_lib.clone();
    
    if !aasdk_lib.exists() {
        println!("cargo:warning=AASDK library not found at {:?}, skipping wrapper build", aasdk_lib);
        println!("cargo:warning=Please build AASDK first: cd aasdk-wrapper && ./build_aasdk.sh");
        return;
    }

    // Store paths for rerun-if-changed before moving them
    let wrapper_source_path = wrapper_source.to_string_lossy().to_string();
    let wrapper_header_path = wrapper_header.to_string_lossy().to_string();
    let aasdk_lib_path_str = aasdk_lib_path.to_string_lossy().to_string();

    // Get system include paths for dependencies using pkg-config
    let mut build = cc::Build::new();
    build
        .cpp(true)
        .file(&wrapper_source)
        .include(&wrapper_dir.join("aasdk").join("include"))
        .include(&aasdk_build_dir)
        .std("c++14")
        .flag("-fPIC");
    
    // Add libusb include path
    if let Ok(output) = std::process::Command::new("pkg-config")
        .args(&["--cflags-only-I", "libusb-1.0"])
        .output()
    {
        let output_str = String::from_utf8_lossy(&output.stdout);
        for line in output_str.lines() {
            if let Some(path) = line.trim().strip_prefix("-I") {
                build.include(path);
            }
        }
    }
    
    // Always add /usr/include for system headers
    build.include("/usr/include");
    
    // Add Boost include paths if available
    if let Ok(output) = std::process::Command::new("pkg-config")
        .args(&["--cflags-only-I", "boost"])
        .output()
    {
        let output_str = String::from_utf8_lossy(&output.stdout);
        for line in output_str.lines() {
            if let Some(path) = line.trim().strip_prefix("-I") {
                build.include(path);
            }
        }
    }
    
    build.compile("aasdk_c");

    // Tell Cargo where to find the AASDK libraries
    let lib_dir = aasdk_build_dir.join("lib");
    let lib_dir_absolute = lib_dir.canonicalize()
        .unwrap_or_else(|_| lib_dir.clone());
    let lib_dir_str = lib_dir_absolute.to_string_lossy().to_string();
    
    // Fix rpath in libaasdk.so so it can find libaasdk_proto.so
    // This is needed because libaasdk_proto.so is a transitive dependency
    let aasdk_so = lib_dir_absolute.join("libaasdk.so");
    if aasdk_so.exists() {
        // Try to fix rpath using patchelf if available
        match std::process::Command::new("patchelf")
            .arg("--set-rpath")
            .arg(&lib_dir_str)
            .arg(&aasdk_so)
            .output()
        {
            Ok(output) => {
                if output.status.success() {
                    println!("cargo:warning=Successfully set rpath in libaasdk.so");
                } else {
                    let err = String::from_utf8_lossy(&output.stderr);
                    println!("cargo:warning=patchelf failed: {}", err);
                    println!("cargo:warning=You may need to set LD_LIBRARY_PATH at runtime or install patchelf");
                }
            }
            Err(_) => {
                // patchelf not available - try to use chrpath as alternative
                if let Ok(output) = std::process::Command::new("chrpath")
                    .arg("-r")
                    .arg(&lib_dir_str)
                    .arg(&aasdk_so)
                    .output()
                {
                    if output.status.success() {
                        println!("cargo:warning=Successfully set rpath in libaasdk.so using chrpath");
                    }
                } else {
                    println!("cargo:warning=Neither patchelf nor chrpath available. Libraries may not find dependencies.");
                    println!("cargo:warning=Install patchelf: sudo apt install patchelf");
                    println!("cargo:warning=Or use run-dev.sh script which sets LD_LIBRARY_PATH");
                }
            }
        }
    }
    
    // Add rpath so the binary can find the libraries at runtime (using absolute path)
    // The rpath is set on the binary so it can find libaasdk.so directly
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir_str);
    println!("cargo:rustc-link-search=native={}", lib_dir_absolute.display());
    println!("cargo:rustc-link-lib=aasdk");
    println!("cargo:rustc-link-lib=aasdk_proto");
    
    // Link against required system libraries
    println!("cargo:rustc-link-lib=boost_system");
    println!("cargo:rustc-link-lib=boost_log");
    println!("cargo:rustc-link-lib=usb-1.0");
    println!("cargo:rustc-link-lib=protobuf");
    println!("cargo:rustc-link-lib=crypto");
    println!("cargo:rustc-link-lib=ssl");

    // Rebuild if wrapper files change
    println!("cargo:rerun-if-changed={}", wrapper_source_path);
    println!("cargo:rerun-if-changed={}", wrapper_header_path);
    println!("cargo:rerun-if-changed={}", aasdk_lib_path_str);
}
