//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//! Pack profiles into reports.

use anyhow::{anyhow, Result};
use std::fs::{read_dir, remove_file, File};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use zip::write::FileOptions;
use zip::ZipWriter;

pub fn pack_report(profile: &Path, report: &Path) -> Result<()> {
    // TODO: Allow multiple profiles to be queued for upload.
    let mut report = PathBuf::from(report);
    report.push("report.zip");

    // Remove the current report file if exists.
    remove_file(&report).ok();

    let report = File::create(report)?;
    let options = FileOptions::default();
    let mut zip = ZipWriter::new(report);

    read_dir(profile)?
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|e| e.is_file())
        .try_for_each(|e| -> Result<()> {
            let filename = e
                .file_name()
                .and_then(|f| f.to_str())
                .ok_or_else(|| anyhow!("Malformed profile path: {}", e.display()))?;
            zip.start_file(filename, options)?;
            let mut f = File::open(e)?;
            let mut buffer = Vec::new();
            f.read_to_end(&mut buffer)?;
            zip.write_all(&*buffer)?;
            Ok(())
        })?;
    zip.finish()?;
    Ok(())
}
