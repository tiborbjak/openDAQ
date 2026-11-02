#!/usr/bin/env python

import opendaq_test
import opendaq
import opendaq.errors
from opendaq.errors import NotFoundError, ErrorCode
import builtins
import pathlib
import re
import unittest

CATEGORIES = {
    'OpenDaqError': 0x01, 'FunctionBlockError': 0x02, 'ModuleManagerError': 0x03,
    'SchedulerError': 0x04, 'CoreObjectsError': 0x06, 'LoggerError': 0x07,
    'DeviceError': 0x08, 'SignalError': 0x0A, 'UtilityError': 0x0C,
    'ReaderError': 0x0D, 'ComponentError': 0x0E, 'NativeError': 0x10,
}


def error_type(code):
    return (int(code) >> 16) & 0xFF


def category_classes():
    return {type_byte: getattr(opendaq.errors, name) for name, type_byte in CATEGORIES.items()}


def code_classes():
    skip = {opendaq.DaqError, *category_classes().values()}
    return [cls for cls in vars(opendaq.errors).values()
            if isinstance(cls, type) and issubclass(cls, opendaq.DaqError) and cls not in skip]


# TODO: replace with a single error code list in core shared by C++ and all bindings.
def header_defines():
    root = pathlib.Path(__file__).resolve().parents[3]
    headers = [*root.glob("core/**/include/**/errors.h"), *root.glob("core/**/include/**/*_errors.h"),
               root / "shared/libraries/config_protocol/include/config_protocol/errors.h"]
    codes, types = set(), {}
    for header in headers:
        text = header.read_text()
        codes |= set(re.findall(r"^#define OPENDAQ_ERR_(\w+)\s", text, re.M))
        types |= {name: int(value, 16) for name, value
                  in re.findall(r"^#define OPENDAQ_ERRTYPE_(\w+)\s+(0x[0-9A-Fa-f]+)", text, re.M)}
    return codes, types


class TestErrors(opendaq_test.TestCase):
    def test_imports(self):
        self.assertIs(opendaq.DaqError, opendaq.errors.DaqError)
        self.assertIs(NotFoundError, opendaq.errors.NotFoundError)
        self.assertIs(ErrorCode, opendaq.errors.ErrorCode)

    def test_every_code_has_class(self):
        classes = code_classes()
        by_code = {cls.code: cls for cls in classes}
        self.assertEqual(len(by_code), len(classes))
        self.assertEqual(set(by_code), set(ErrorCode))
        self.assertEqual(len(ErrorCode.__members__), len(ErrorCode))
        for member in ErrorCode:
            with self.subTest(member=member.name):
                cls = by_code[member]
                self.assertIs(type(opendaq.DaqError("", code=member)), cls)
                self.assertEqual(cls.__module__, "opendaq.errors")

    def test_header_codes_mapped(self):
        codes, types = header_defines()
        self.assertGreater(len(codes), 80)
        self.assertEqual(set(ErrorCode.__members__), codes)
        self.assertEqual(types.pop("GENERIC"), 0)
        self.assertEqual(set(types.values()), set(CATEGORIES.values()))

    def test_categories(self):
        for type_byte, cls in category_classes().items():
            with self.subTest(category=cls.__name__):
                self.assertEqual(cls.__bases__, (opendaq.DaqError,))
                self.assertEqual(cls.__module__, "opendaq.errors")
                code = 0x80000000 | (type_byte << 16) | 0xFFFF
                self.assertNotIn(code, set(ErrorCode))
                self.assertIs(type(opendaq.DaqError("", code=code)), cls)
        used = {error_type(m) for m in ErrorCode} - {0}
        self.assertLessEqual(used, set(CATEGORIES.values()))

    def test_no_builtin_shadowing(self):
        names = {cls.__name__ for cls in code_classes()}
        names |= set(CATEGORIES) | {"DaqError", "ErrorCode"}
        self.assertEqual(names & set(dir(builtins)), set())

    def test_hierarchy_bases(self):
        self.assertTrue(issubclass(opendaq.DaqError, RuntimeError))
        for cls in code_classes() + list(category_classes().values()):
            with self.subTest(cls=cls.__name__):
                self.assertTrue(issubclass(cls, opendaq.DaqError))
                self.assertTrue(issubclass(cls, RuntimeError))

    def test_parent_matches_error_type(self):
        categories = category_classes()
        for cls in code_classes():
            with self.subTest(cls=cls.__name__):
                expected = categories.get(error_type(cls.code), opendaq.DaqError)
                self.assertEqual(cls.__bases__, (expected,))
        self.assertEqual(opendaq.errors.InvalidDataError.__bases__, (opendaq.errors.ReaderError,))

    def test_generic_codes_derive_from_daq_error(self):
        for cls in (opendaq.errors.NotFoundError, opendaq.errors.FrozenError):
            with self.subTest(cls=cls.__name__):
                self.assertEqual(error_type(cls.code), 0)
                self.assertEqual(cls.__bases__, (opendaq.DaqError,))

    def test_construct_subclass(self):
        e = NotFoundError("x")
        self.assertIs(type(e), NotFoundError)
        self.assertEqual(e.code, ErrorCode.NOTFOUND)
        self.assertEqual(str(e), "x")
        self.assertEqual(e.args, ("x",))

    def test_construct_dispatches_on_code(self):
        e = opendaq.DaqError("x", code=ErrorCode.FROZEN)
        self.assertIs(type(e), opendaq.errors.FrozenError)
        self.assertEqual(e.code, ErrorCode.FROZEN)
        self.assertEqual(str(e), "x")

    def test_construct_unknown_code_in_known_type(self):
        known = {int(m) for m in ErrorCode}
        code = next(c for c in (0x800D0000 | n for n in range(0x10000)) if c not in known)
        e = opendaq.DaqError("x", code=code)
        self.assertIs(type(e), opendaq.errors.ReaderError)
        self.assertEqual(e.code, code)

    def test_construct_unknown_error_type(self):
        self.assertNotIn(0xFF, CATEGORIES.values())
        code = 0x80FF0001
        self.assertNotIn(code, {int(m) for m in ErrorCode})
        e = opendaq.DaqError("x", code=code)
        self.assertIs(type(e), opendaq.DaqError)
        self.assertEqual(e.code, code)

    def test_code_type(self):
        for cls in code_classes():
            with self.subTest(cls=cls.__name__):
                self.assertIs(type(cls.code), ErrorCode)
        self.assertIs(type(NotFoundError("x").code), ErrorCode)
        e = opendaq.DaqError("x", code=int(ErrorCode.FROZEN))
        self.assertIs(e.code, ErrorCode.FROZEN)

    def test_category_takes_matching_code(self):
        errors = opendaq.errors
        e = errors.ReaderError("x", code=ErrorCode.INVALID_DATA)
        self.assertIs(type(e), errors.InvalidDataError)
        self.assertIs(e.code, ErrorCode.INVALID_DATA)
        e = errors.ReaderError("x", code=0x800D0FFF)
        self.assertIs(type(e), errors.ReaderError)
        self.assertEqual(e.code, 0x800D0FFF)

    def test_category_rejects_mismatched_code(self):
        with self.assertRaisesRegex(ValueError, "FROZEN.*ReaderError"):
            opendaq.errors.ReaderError("x", code=ErrorCode.FROZEN)
        with self.assertRaises(ValueError):
            opendaq.errors.ReaderError("x", code=0x800E0001)

    def test_category_requires_code(self):
        for cls in (opendaq.DaqError, opendaq.errors.ReaderError):
            with self.subTest(cls=cls.__name__), self.assertRaisesRegex(TypeError, cls.__name__):
                cls("x")
        self.assertIsNone(opendaq.DaqError.code)
        self.assertIsNone(opendaq.errors.ReaderError.code)

    def test_code_class_rejects_code(self):
        with self.assertRaisesRegex(TypeError, "NotFoundError"):
            NotFoundError("x", code=ErrorCode.NOTFOUND)

    def test_invalid_code(self):
        with self.assertRaisesRegex(TypeError, "str"):
            opendaq.DaqError("x", code="bad")
        with self.assertRaisesRegex(TypeError, "float"):
            opendaq.DaqError("x", code=1.0)
        for code in (0, 0x17, -1, 0x100000000):
            with self.subTest(code=code), self.assertRaisesRegex(ValueError, "not an openDAQ error code"):
                opendaq.DaqError("x", code=code)

    def test_catch_by_category(self):
        with self.assertRaises(opendaq.errors.ReaderError) as cm:
            raise opendaq.errors.InvalidDataError("x")
        self.assertIs(type(cm.exception), opendaq.errors.InvalidDataError)

        with self.assertRaises(opendaq.errors.ReaderError):
            raise opendaq.DaqError("x", code=opendaq.errors.InvalidDataError.code)


if __name__ == '__main__':
    unittest.main()
