#include "CommandNormalizer.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <pathcch.h>
#include <regex>
#include <shlwapi.h>
#include <sstream>
#include <windows.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "pathcch.lib")

namespace uboot {

// Helper: Convert UTF-8 to UTF-16
std::wstring CommandNormalizer::Utf8ToUtf16(const std::string &utf8) {
  if (utf8.empty())
    return L"";

  int requiredSize =
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  if (requiredSize <= 0)
    return L"";

  std::wstring utf16(requiredSize - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], requiredSize);

  return utf16;
}

// Helper: Convert UTF-16 to UTF-8
std::string CommandNormalizer::Utf16ToUtf8(const std::wstring &utf16) {
  if (utf16.empty())
    return "";

  int requiredSize = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr,
                                         0, nullptr, nullptr);
  if (requiredSize <= 0)
    return "";

  std::string utf8(requiredSize - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], requiredSize,
                      nullptr, nullptr);

  return utf8;
}

// Helper: Check if character is a quote (regular or smart)
bool CommandNormalizer::IsQuoteCharacter(wchar_t c) {
  return c == L'"' || c == L'\'' || IsSmartQuote(c);
}

// Helper: Check if character is a smart/intelligent quote
bool CommandNormalizer::IsSmartQuote(wchar_t c) {
  return c == L'\u2018' || c == L'\u2019' || // Single quotes
         c == L'\u201C' || c == L'\u201D';   // Double quotes
}

// Helper: Trim leading and trailing whitespace
std::wstring CommandNormalizer::TrimWhitespace(const std::wstring &input) {
  auto start = input.begin();
  while (start != input.end() && iswspace(*start)) {
    ++start;
  }

  auto end = input.end();
  do {
    --end;
  } while (std::distance(start, end) > 0 && iswspace(*end));

  return std::wstring(start, end + 1);
}

// Helper: Check if file exists and is executable
bool CommandNormalizer::IsExecutableFile(const std::wstring &path) {
  DWORD attrs = GetFileAttributesW(path.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  // Check if it's a file (not a directory)
  return !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// Step 1: Sanitize various whitespace characters
std::wstring CommandNormalizer::SanitizeWhitespace(const std::wstring &input) {
  std::wstring result = input;

  // Replace common problematic whitespace with regular spaces
  for (auto &c : result) {
    switch (c) {
    case L'\u00A0': // Non-breaking space
    case L'\u2000': // En quad
    case L'\u2001': // Em quad
    case L'\u2002': // En space
    case L'\u2003': // Em space
    case L'\u2004': // Three-per-em space
    case L'\u2005': // Four-per-em space
    case L'\u2006': // Six-per-em space
    case L'\u2007': // Figure space
    case L'\u2008': // Punctuation space
    case L'\u2009': // Thin space
    case L'\u200A': // Hair space
    case L'\u202F': // Narrow no-break space
    case L'\u205F': // Medium mathematical space
    case L'\t':     // Tab
      c = L' ';
      break;
    case L'\r': // Carriage return
    case L'\n': // Newline
      c = L' ';
      break;
    }
  }

  // Collapse multiple spaces into single spaces
  std::wstring collapsed;
  bool lastWasSpace = false;
  for (wchar_t c : result) {
    if (c == L' ') {
      if (!lastWasSpace) {
        collapsed += c;
        lastWasSpace = true;
      }
    } else {
      collapsed += c;
      lastWasSpace = false;
    }
  }

  return TrimWhitespace(collapsed);
}

// Step 2: Expand environment variables
std::wstring
CommandNormalizer::ExpandEnvironmentVariables(const std::wstring &input) {
  // First, try simple expansion up to a reasonable size
  wchar_t expanded[32768];
  DWORD result =
      ExpandEnvironmentStringsW(input.c_str(), expanded, _countof(expanded));

  if (result > 0 && result <= _countof(expanded)) {
    return std::wstring(expanded);
  }

  // If expansion fails, return original (safer than trying dynamic allocation)
  return input;
}

// Step 3: Tolerant command-line parsing
void CommandNormalizer::ParseCommandLine(const std::wstring &commandLine,
                                         std::wstring &outExecutable,
                                         std::wstring &outArguments) {
  outExecutable.clear();
  outArguments.clear();

  if (commandLine.empty()) {
    return;
  }

  std::wstring trimmed = TrimWhitespace(commandLine);
  size_t pos = 0;

  // Check if starts with a quote
  if (pos < trimmed.length() && IsQuoteCharacter(trimmed[pos])) {
    wchar_t quoteChar = trimmed[pos];
    pos++; // Skip opening quote

    // Find closing quote (may be mismatched, so be tolerant)
    size_t endQuote = pos;
    while (endQuote < trimmed.length() && trimmed[endQuote] != quoteChar) {
      endQuote++;
    }

    if (endQuote < trimmed.length()) {
      // Successfully found closing quote
      outExecutable = trimmed.substr(pos, endQuote - pos);
      pos = endQuote + 1; // Skip closing quote
    } else {
      // Mismatched quote - take until space or end
      endQuote = pos;
      while (endQuote < trimmed.length() && trimmed[endQuote] != L' ') {
        endQuote++;
      }
      outExecutable = trimmed.substr(pos, endQuote - pos);
      pos = endQuote;
    }
  } else {
    // Unquoted executable: read until first space
    size_t endExe = pos;
    while (endExe < trimmed.length() && trimmed[endExe] != L' ') {
      endExe++;
    }
    outExecutable = trimmed.substr(pos, endExe - pos);
    pos = endExe;
  }

  // Remaining is arguments
  if (pos < trimmed.length()) {
    outArguments = TrimWhitespace(trimmed.substr(pos));
  }
}

// Step 4: Canonicalize path
std::wstring
CommandNormalizer::CanonicalizePath(const std::wstring &path,
                                    const std::wstring &workingDir) {
  if (path.empty()) {
    return path;
  }

  std::wstring resolved = path;

  // Helper lambda for absolute path check
  auto IsAbsolutePath = [](const std::wstring &p) -> bool {
    if (p.length() >= 3 && iswalpha(p[0]) && p[1] == L':' &&
        (p[2] == L'\\' || p[2] == L'/'))
      return true;
    if (p.length() >= 2 && p[0] == L'\\' && p[1] == L'\\')
      return true;
    return false;
  };

  // If path is relative and we have a working directory, make it absolute
  if (!IsAbsolutePath(path) && !workingDir.empty()) {
    wchar_t combined[MAX_PATH + 1];
    if (PathCombineW(combined, workingDir.c_str(), path.c_str())) {
      resolved = combined;
    }
  }

  // Use PathCchCanonicalizeEx for Windows 7+
  wchar_t canonical[MAX_PATH + 1];
  HRESULT hr = PathCchCanonicalizeEx(canonical, _countof(canonical),
                                     resolved.c_str(), 0);
  if (SUCCEEDED(hr)) {
    resolved = canonical;
  } else {
    // Fallback: use GetFullPathNameW
    wchar_t fullPath[MAX_PATH + 1];
    DWORD len = GetFullPathNameW(resolved.c_str(), _countof(fullPath), fullPath,
                                 nullptr);
    if (len > 0 && len < _countof(fullPath)) {
      resolved = fullPath;
    }
  }

  return resolved;
}

// Helper: Try to peel one wrapper layer
bool CommandNormalizer::TryPeelSingleWrapper(const std::wstring &executable,
                                             const std::wstring &arguments,
                                             std::wstring &outExecutable,
                                             std::wstring &outArguments,
                                             std::wstring &outNote) {
  std::wstring exeName = executable;

  // Normalize to lowercase for comparison
  std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);

  // Remove path if present
  size_t lastSlash = exeName.find_last_of(L"\\/");
  if (lastSlash != std::wstring::npos) {
    exeName = exeName.substr(lastSlash + 1);
  }

  // --- cmd.exe /c <command> ---
  if (exeName == L"cmd.exe" || exeName == L"cmd") {
    if (arguments.find(L"/c") == 0 || arguments.find(L"/C") == 0) {
      size_t cmdStart = 2; // Skip "/c"
      while (cmdStart < arguments.length() && iswspace(arguments[cmdStart])) {
        cmdStart++;
      }
      if (cmdStart < arguments.length()) {
        std::wstring wrappedCmd = arguments.substr(cmdStart);
        ParseCommandLine(wrappedCmd, outExecutable, outArguments);
        outNote = L"Peeled: cmd.exe /c wrapper";
        return true;
      }
    } else if (arguments.find(L"/k") == 0 || arguments.find(L"/K") == 0) {
      size_t cmdStart = 2;
      while (cmdStart < arguments.length() && iswspace(arguments[cmdStart])) {
        cmdStart++;
      }
      if (cmdStart < arguments.length()) {
        std::wstring wrappedCmd = arguments.substr(cmdStart);
        ParseCommandLine(wrappedCmd, outExecutable, outArguments);
        outNote = L"Peeled: cmd.exe /k wrapper";
        return true;
      }
    }
  }

  // --- powershell.exe -Command <command> or -EncodedCommand <base64> ---
  if (exeName == L"powershell.exe" || exeName == L"powershell") {
    std::wstring args = arguments;

    // Try -Command
    size_t cmdPos = args.find(L"-Command");
    if (cmdPos == std::wstring::npos) {
      cmdPos = args.find(L"-command");
    }

    // Try -EncodedCommand (more sneaky; we decode but don't execute)
    size_t encPos = args.find(L"-EncodedCommand");
    if (encPos == std::wstring::npos) {
      encPos = args.find(L"-encodedCommand");
    }

    if (cmdPos != std::wstring::npos) {
      size_t cmdStart = cmdPos + 8; // len("-Command")
      while (cmdStart < args.length() && iswspace(args[cmdStart])) {
        cmdStart++;
      }
      if (cmdStart < args.length()) {
        std::wstring wrappedCmd = args.substr(cmdStart);
        ParseCommandLine(wrappedCmd, outExecutable, outArguments);
        outNote = L"Peeled: powershell.exe -Command wrapper";
        return true;
      }
    }

    if (encPos != std::wstring::npos) {
      // Base64 encoded command: we extract it but don't execute
      // This is for tracking purposes only
      size_t cmdStart = encPos + 15; // len("-EncodedCommand")
      while (cmdStart < args.length() && iswspace(args[cmdStart])) {
        cmdStart++;
      }
      if (cmdStart < args.length()) {
        size_t cmdEnd = cmdStart;
        while (cmdEnd < args.length() && !iswspace(args[cmdEnd])) {
          cmdEnd++;
        }
        outArguments = args.substr(cmdStart, cmdEnd - cmdStart);
        outExecutable = L"powershell.exe";
        outNote = L"Found: powershell.exe -EncodedCommand (base64 extracted, "
                  L"not decoded/executed)";
        return false; // Don't peel further; this is a terminal point
      }
    }
  }

  // --- wscript.exe / cscript.exe <script> ---
  if (exeName == L"wscript.exe" || exeName == L"wscript" ||
      exeName == L"cscript.exe" || exeName == L"cscript") {
    ParseCommandLine(arguments, outExecutable, outArguments);
    outNote = L"Peeled: " + exeName + L" wrapper";
    return true;
  }

  // --- rundll32.exe <dll> <export> [args] ---
  if (exeName == L"rundll32.exe" || exeName == L"rundll32") {
    // rundll32.exe module.dll,Export [args]
    // Extract the first argument as the DLL
    size_t firstTokenEnd = 0;
    bool inQuotes = false;
    while (firstTokenEnd < arguments.length()) {
      wchar_t c = arguments[firstTokenEnd];
      if (IsQuoteCharacter(c)) {
        inQuotes = !inQuotes;
      } else if (c == L' ' && !inQuotes) {
        break;
      }
      firstTokenEnd++;
    }

    if (firstTokenEnd < arguments.length()) {
      outExecutable = arguments.substr(0, firstTokenEnd);
      outArguments = TrimWhitespace(arguments.substr(firstTokenEnd));
      outNote = L"Peeled: rundll32.exe wrapper";
      return true;
    }
  }

  // --- mshta.exe <html> ---
  if (exeName == L"mshta.exe" || exeName == L"mshta") {
    ParseCommandLine(arguments, outExecutable, outArguments);
    outNote = L"Peeled: mshta.exe wrapper";
    return true;
  }

  // --- regsvr32.exe <dll> ---
  if (exeName == L"regsvr32.exe" || exeName == L"regsvr32") {
    // Extract DLL argument
    std::wstring trimmedArgs = TrimWhitespace(arguments);

    // Skip flags like /s /i:<string>
    size_t tokenStart = 0;
    while (tokenStart < trimmedArgs.length()) {
      if (trimmedArgs[tokenStart] == L'/') {
        // Skip flag
        size_t tokenEnd = tokenStart + 1;
        while (tokenEnd < trimmedArgs.length() &&
               trimmedArgs[tokenEnd] != L' ') {
          tokenEnd++;
        }
        tokenStart = tokenEnd;
        while (tokenStart < trimmedArgs.length() &&
               iswspace(trimmedArgs[tokenStart])) {
          tokenStart++;
        }
      } else {
        break;
      }
    }

    if (tokenStart < trimmedArgs.length()) {
      size_t tokenEnd = tokenStart;
      while (tokenEnd < trimmedArgs.length() && trimmedArgs[tokenEnd] != L' ') {
        tokenEnd++;
      }
      outExecutable = trimmedArgs.substr(tokenStart, tokenEnd - tokenStart);
      outArguments = TrimWhitespace(trimmedArgs.substr(tokenEnd));
      outNote = L"Peeled: regsvr32.exe wrapper";
      return true;
    }
  }

  return false;
}

// Step 5: Peel wrappers (multi-pass, max 3 iterations)
void CommandNormalizer::PeelWrappers(std::wstring &executable,
                                     std::wstring &arguments,
                                     std::vector<std::wstring> &notes) {
  const int MAX_ITERATIONS = 3;

  for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
    std::wstring newExecutable, newArguments, note;

    if (TryPeelSingleWrapper(executable, arguments, newExecutable, newArguments,
                             note)) {
      executable = newExecutable;
      arguments = newArguments;
      notes.push_back(note);
    } else {
      // No wrapper found or cannot peel further (e.g., powershell
      // -EncodedCommand)
      if (!note.empty()) {
        notes.push_back(note);
      }
      break;
    }
  }
}

// Main entry point
NormalizationResult
CommandNormalizer::Normalize(const std::string &rawCommand,
                             const std::string &workingDir) {
  NormalizationResult result;
  result.originalCommand = rawCommand;
  result.isComplete = true;

  if (rawCommand.empty()) {
    result.resolveNotes = "Empty command";
    result.isComplete = false;
    return result;
  }

  // Convert to UTF-16 for processing
  std::wstring command = Utf8ToUtf16(rawCommand);
  std::wstring workDir = workingDir.empty() ? L"" : Utf8ToUtf16(workingDir);

  // Step 1: Sanitize whitespace
  command = SanitizeWhitespace(command);
  result.resolveNotes = "Sanitized whitespace. ";

  // Step 2: Expand environment variables
  std::wstring expanded = ExpandEnvironmentVariables(command);
  if (expanded != command) {
    result.resolveNotes += "Expanded environment variables. ";
    command = expanded;
  }

  // Step 3: Parse command line
  std::wstring executable, arguments;
  ParseCommandLine(command, executable, arguments);
  result.resolveNotes += "Parsed command line. ";

  // Step 4: Canonicalize path
  std::vector<std::wstring> peelNotes;
  executable = CanonicalizePath(executable, workDir);
  result.resolveNotes += "Canonicalized path. ";

  // Step 5: Peel wrappers
  PeelWrappers(executable, arguments, peelNotes);
  for (const auto &note : peelNotes) {
    result.resolveNotes += Utf16ToUtf8(note) + " ";
  }

  // Check if final executable exists and is valid
  if (!IsExecutableFile(executable)) {
    result.isComplete = false;
    result.resolveNotes +=
        "Warning: Resolved path does not exist or is not a file. ";
  }

  // Convert back to UTF-8
  result.resolvedPath = Utf16ToUtf8(executable);
  result.arguments = Utf16ToUtf8(arguments);

  return result;
}

} // namespace uboot
