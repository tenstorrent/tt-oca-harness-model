#!/usr/bin/env python3
"""Apply C++20 compatibility patches to the CSML submodule (idempotent).

Handles both the pre- and post-4ae324e csml_report.h layouts.  Safe to run
multiple times; exits 0 when no patch is needed.
"""
import pathlib
import sys

PATCHED = (
    "template<class First, class Second, class... Rest>\n"
    "inline std::string form_report_string(First first, Second second, Rest... rest)\n"
    "{\n"
    "  return form_report_string(first) + form_report_string(second, rest...);\n"
    "}"
)

OLD_REPORT_BLOCK = (
    "template<class... va_args>\n"
    "std::string form_report_string(std::string arg1, va_args... args);\n"
    "\n"
    "template<class T, class U>\n"
    "inline std::string form_report_string(T arg1, U arg2)\n"
    "{\n"
    "   return form_report_string(arg1) + form_report_string(arg2);\n"
    "}\n"
    "\n"
    "template<class T, class... va_args>\n"
    "inline std::string form_report_string(T arg1, va_args... args)\n"
    "{\n"
    "  return form_report_string(arg1) + form_report_string(args...);\n"
    "}\n"
    "\n"
    "template<class... va_args>\n"
    "inline std::string form_report_string(std::string arg1, va_args... args)\n"
    "{\n"
    "  return arg1 + form_report_string(args...);\n"
    "}"
)

NEW_REPORT_BLOCK = (
    "template<class... va_args>\n"
    "std::string form_report_string(std::string arg1, va_args... args);\n"
    "\n"
    "template<class T, class... va_args>\n"
    "inline std::string form_report_string(T arg1, va_args... args)\n"
    "{\n"
    "  return form_report_string(arg1) + form_report_string(args...);\n"
    "}\n"
    "\n"
    "template<class... va_args>\n"
    "inline std::string form_report_string(std::string arg1, va_args... args)\n"
    "{\n"
    "  return arg1 + form_report_string(args...);\n"
    "}"
)


def patch_csml_report(path: pathlib.Path) -> None:
    src = path.read_text()
    if PATCHED in src:
        print("csml_report.h: already patched, skipping")
        return
    if OLD_REPORT_BLOCK in src:
        path.write_text(src.replace(OLD_REPORT_BLOCK, PATCHED))
        print("csml_report.h: patched (legacy layout)")
        return
    if NEW_REPORT_BLOCK in src:
        path.write_text(src.replace(NEW_REPORT_BLOCK, PATCHED))
        print("csml_report.h: patched (current CSML layout)")
        return
    print("csml_report.h: no ambiguous overload block found — assuming upstream fix, skipping")


def patch_csml_register(path: pathlib.Path) -> None:
    src = path.read_text()
    if 'pragma clang diagnostic ignored "-Wstack-exhausted"' in src:
        print("csml_register.h: pragma already applied, skipping")
        return
    before = "template <class T, unsigned int Quantity>\nclass csml_reg_vector\n{"
    after_end = "};\n\ntemplate <class T, unsigned int Quantity1, unsigned int Quantity2>"
    if before not in src:
        print("csml_register.h: layout changed, skipping")
        return
    guarded_before = (
        "#if defined(__clang__)\n"
        "#pragma clang diagnostic push\n"
        "#pragma clang diagnostic ignored \"-Wstack-exhausted\"\n"
        "#endif\n\n"
        + before
    )
    guarded_end = (
        "};\n\n"
        "#if defined(__clang__)\n"
        "#pragma clang diagnostic pop\n"
        "#endif\n\n"
        "template <class T, unsigned int Quantity1, unsigned int Quantity2>"
    )
    src = src.replace(before, guarded_before)
    src = src.replace(after_end, guarded_end, 1)
    path.write_text(src)
    print("csml_register.h: patched")


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    patch_csml_report(root / "sep/utils/csml/inc/csml_report.h")
    patch_csml_register(root / "sep/utils/csml/inc/csml_register.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
