// Copyright 2024 The Dredd Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DREDD_LOG_FAILED_FILES_DIAGNOSTIC_CONSUMER_H
#define DREDD_LOG_FAILED_FILES_DIAGNOSTIC_CONSUMER_H

#include <set>
#include <string>

#include "clang/Basic/Diagnostic.h"

// This diagnostic consumer logs the names of all files that exhibit errors.
class LogFailedFilesDiagnosticConsumer : public clang::DiagnosticConsumer {
 public:
  void clear() override {}

  void BeginSourceFile(const clang::LangOptions& lang_opts,
                       const clang::Preprocessor* preprocessor) override {
    (void)lang_opts;
    (void)preprocessor;
  }

  void EndSourceFile() override {}

  void finish() override {}

  [[nodiscard]] bool IncludeInDiagnosticCounts() const override { return true; }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level diag_level,
                        const clang::Diagnostic& info) override;

  [[nodiscard]] const std::set<std::string>& GetFilesWithErrors() const {
    return files_with_errors;
  }

 private:
  // Any file that exhibits an error will have its name stored in this set.
  std::set<std::string> files_with_errors;
};

#endif  // DREDD_LOG_FAILED_FILES_DIAGNOSTIC_CONSUMER_H
