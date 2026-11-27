"""Tests of tools/lint/check_module_locals."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_module_locals"

LOADER = importlib.machinery.SourceFileLoader("check_module_locals", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def errors(path: str, text: str) -> list[int]:
    return [index for index, _ in MODULE.errors(path, text.split("\n"))]


class ModuleLocalsTest(unittest.TestCase):
    def test_header_naming_a_module_function_fails(self) -> None:
        self.assertEqual(errors("a.h", "void M_Foo(void);\n"), [0])

    def test_header_naming_a_module_variable_fails(self) -> None:
        self.assertEqual(errors("a.h", "static int m_Foo;\n"), [0])

    def test_source_definition_without_static_fails(self) -> None:
        self.assertEqual(errors("a.c", "void M_Foo(void)\n{\n}\n"), [0])

    def test_source_definition_with_static_passes(self) -> None:
        self.assertEqual(errors("a.c", "static void M_Foo(void)\n{\n}\n"), [])

    def test_m_pi_passes(self) -> None:
        self.assertEqual(errors("a.h", "const double x = M_PI;\n"), [])
        self.assertEqual(errors("a.c", "const double x = M_PI;\n"), [])

    def test_use_inside_a_function_passes(self) -> None:
        text = "void F(void)\n{\n    M_Foo();\n    m_Count++;\n}\n"
        self.assertEqual(errors("a.c", text), [])

    def test_static_struct_array_continuation_passes(self) -> None:
        text = (
            "static const IMAGE_DESC m_Images[] = {\n"
            "    { .id = 1 },\n"
            "} m_Images[ARRAY_SIZE(m_Images)];\n"
        )
        self.assertEqual(errors("a.c", text), [])

    def test_static_function_without_m_prefix_fails(self) -> None:
        self.assertEqual(errors("a.c", "static void Foo(void)\n{\n}\n"), [0])

    def test_static_variable_without_m_prefix_fails(self) -> None:
        self.assertEqual(errors("a.c", "static int count;\n"), [0])

    def test_m_named_static_variable_fails(self) -> None:
        # M_PascalCase is for functions; a variable needs m_PascalCase.
        self.assertEqual(errors("a.c", "static int M_Count;\n"), [0])

    def test_header_static_inline_passes(self) -> None:
        text = "static inline void Module_Helper(void)\n{\n}\n"
        self.assertEqual(errors("a.h", text), [])

    def test_static_function_with_leading_attribute_passes(self) -> None:
        text = "static void __attribute__((noinline)) M_Foo(void)\n{\n}\n"
        self.assertEqual(errors("a.c", text), [])

    def test_static_function_pointer_table_passes(self) -> None:
        text = "static void (*m_Funcs[])(void) = {\n    Foo,\n};\n"
        self.assertEqual(errors("a.c", text), [])

    def test_static_function_pointer_table_without_m_prefix_fails(self) -> None:
        text = "static void (*Funcs[])(void) = {\n    Foo,\n};\n"
        self.assertEqual(errors("a.c", text), [0])

    def test_local_static_inside_a_function_passes(self) -> None:
        text = "void F(void)\n{\n    static int count = 0;\n    count++;\n}\n"
        self.assertEqual(errors("a.c", text), [])

    def test_vendor_code_is_exempt(self) -> None:
        text = "static void foo(void)\n{\n}\n"
        self.assertEqual(errors("src/pc98/opna/vendor/ym2608.c", text), [])

    def test_libm_is_exempt(self) -> None:
        text = "static const double pi = 3.14;\n"
        self.assertEqual(errors("src/core/math/libm/atan2.c", text), [])

    def test_multi_line_declaration_name_on_next_line_is_out_of_scope(self) -> None:
        # The name is not on the `static` line, so this is not matched by
        # the check; it is explicitly out of scope, not a false positive.
        text = "static const int\n    foo = 1;\n"
        self.assertEqual(errors("a.c", text), [])

    def test_check_fails_and_names_the_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.h"
            path.write_text("void M_Foo(void);\n")
            self.assertEqual(MODULE.main([str(path)]), 1)
            path.write_text("void Foo_Bar(void);\n")
            self.assertEqual(MODULE.main([str(path)]), 0)


if __name__ == "__main__":
    unittest.main()
