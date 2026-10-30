#pragma once
/*
  FakeShellIO.h — ShellIO for laptop tests (pio test -e native).

  feed("strip red\n")  → input the shell will read
  lines                → every output line, captured
  has("text")          → true if any output line contains "text"
  clock                → fake microsecond clock (+100 per micros() call)
  quietOn / quietCalls → what the shell did with setQuiet()

  Include <Shell.h> BEFORE this file.
*/
#include <string>
#include <vector>
#include <ShellCore.h>

class FakeShellIO : public ShellIO {
public:
  std::string              input;
  size_t                   pos = 0;
  std::vector<std::string> lines;
  uint32_t                 clock = 0;
  bool                     quietOn = false;
  int                      quietCalls = 0;

  void feed(const char* s) { input += s; }

  int available() override { return (int)(input.size() - pos); }
  int read() override { return pos < input.size() ? (unsigned char)input[pos++] : -1; }
  void writeLine(const char* line) override { lines.push_back(line); }
  uint32_t micros() override { clock += 100; return clock; }
  void setQuiet(bool q) override { quietOn = q; quietCalls++; }

  bool has(const char* part) const {
    for (const std::string& l : lines) if (l.find(part) != std::string::npos) return true;
    return false;
  }
  void clear() { lines.clear(); }
};
