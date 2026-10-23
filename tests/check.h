// Minimal test helpers. Every check prints exactly one line:
//   "  PASS: <name>"  or  "  FAIL: <name>: expected <a> got <b>"
// tools/test-core.sh counts those lines. main() returns check_result().
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static int g_checks_failed = 0;

inline void check_report(bool ok, const std::string& name, const std::string& detail = "") {
    if (ok) std::cout << "  PASS: " << name << "\n";
    else { std::cout << "  FAIL: " << name << (detail.empty() ? "" : ": " + detail) << "\n"; g_checks_failed++; }
}
template <class A, class B>
inline void check_eq(const A& got, const B& want, const std::string& name) {
    std::ostringstream d; d << "expected [" << want << "] got [" << got << "]";
    check_report(got == want, name, d.str());
}
inline void check_near(double got, double want, const std::string& name, double eps = 0.01) {
    std::ostringstream d; d << "expected " << want << " got " << got;
    check_report(std::fabs(got - want) < eps, name, d.str());
}
inline void check_true(bool cond, const std::string& name, const std::string& detail = "") {
    check_report(cond, name, detail);
}
inline int check_result() { return g_checks_failed ? 1 : 0; }

// Scratch dir for this binary (set by tools/test-core.sh); never write anywhere else.
inline std::string scratch_dir() {
    const char* t = std::getenv("SHEATH_TMP");
    return t ? t : ".deps/tmp/unknown";
}
inline std::string fixture_dir() {
    const char* s = std::getenv("SWORD_PATH");
    return s ? s : ".deps/fixtures/sword";
}
// Runs a shell command, returns its stdout.
inline std::string run_cmd(const std::string& cmd) {
    std::string out; char buf[4096];
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}
inline std::string collapse_ws(const std::string& s) {
    std::string o; bool sp = false;
    for (unsigned char c : s) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') { sp = true; continue; }
        if (sp && !o.empty()) o += ' ';
        sp = false; o += (char)c;
    }
    return o;
}
