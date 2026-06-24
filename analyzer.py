from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np
import struct

from dataclasses import dataclass
from enum import StrEnum
import ctypes
import sys
import csv
import os
import glob
from pygccxml import declarations
from pygccxml import utils
from pygccxml import parser
from pygccxml.declarations import cpptypes, declarated_t, typedef_t, class_t
import argparse
from pprint import *
from math import prod

def clamp(value, lo=0.0, hi=1.0):
    return max(lo, min(hi, value))

C_TYPE_MAP = {
    "double": ctypes.c_double,
    "float": ctypes.c_float,
    "int": ctypes.c_int,
    "unsigned int": ctypes.c_uint,
    "long": ctypes.c_long,
    "long unsigned int": ctypes.c_ulong,
    "long long unsigned int": ctypes.c_ulong,
    "char": ctypes.c_char,
    "unsigned char": ctypes.c_ubyte,
    "short": ctypes.c_short,
    "short unsigned int": ctypes.c_ushort,
    "size_t": ctypes.c_size_t,
    "usize": ctypes.c_size_t,
    "bool" : ctypes.c_bool,
}

STRUCT_CACHE = {}

def resolve_typedefs(decl):
    """Recursively unwrap typedefs."""
    while isinstance(decl, typedef_t):
        decl = decl.decl_type
    return decl

def get_fund_ctype_from_decl(decl_type):
    # 1. Fundamental types
    c_type_name = str(decl_type).replace("const ", "").strip()
    if c_type_name in C_TYPE_MAP:
        return C_TYPE_MAP[c_type_name]

    # 2. Void
    if isinstance(decl_type, cpptypes.void_t):
        return ctypes.c_void_p

    # 3. Pointer
    if isinstance(decl_type, cpptypes.pointer_t):
        base_type = resolve_typedefs(decl_type.base)
        if isinstance(base_type, cpptypes.void_t):
            return ctypes.c_void_p
        return ctypes.POINTER(get_ctype_from_decl(base_type))

    # 4. Array
    if isinstance(decl_type, cpptypes.array_t):
        return get_ctype_from_decl(decl_type.base) * decl_type.size

    return None

def get_ctype_from_decl(decl_type):
    decl_type = resolve_typedefs(decl_type)
    typ = get_fund_ctype_from_decl(decl_type)
    if typ is not None:
        return typ

    # 5. User-defined struct/class
    if isinstance(decl_type, declarated_t):
        decl = resolve_typedefs(decl_type.declaration)
        if decl in STRUCT_CACHE:
            return STRUCT_CACHE[decl]
        if isinstance(decl, class_t):
            struct_ctype = make_ctypes_struct(decl)
            STRUCT_CACHE[decl] = struct_ctype
            return struct_ctype

        typ = get_fund_ctype_from_decl(decl)
        if typ is not None:
            return typ

        raise ValueError(f"Unsupported declarated type: {decl}")

    raise ValueError(f"Unknown type: {decl_type}")

def make_ctypes_struct(struct):
    """Convert pygccxml struct/class to ctypes.Structure."""
    struct = resolve_typedefs(struct)
    fields = []

    for field in struct.variables():
        try:
            field_ctype = get_ctype_from_decl(field.decl_type)
        except ValueError as e:
            print(f"[WARN] Skipping field {field.name}: {e}")
            continue
        fields.append((field.name, field_ctype))

    class Struct(ctypes.Structure):
        _fields_ = fields

    return Struct

def get_annotations(decl):
    """
    Returns a list of clang annotate attributes, if any.
    """
    notes = []
    if decl.attributes:
        notes = decl.attributes.split(" ")
        notes = [a[len("annotate("):-1] for a in notes]
    return notes

def parse_annotations(annotation_list):
    """
    Parse annotation list into dict.
    """
    result = {}
    for item in annotation_list:
        pairs = item.split(',')
        for pair in pairs:
            if '=' in pair:
                key, val = pair.split('=', 1)
                result[key.strip()] = val.strip()
            else:
                result[pair.strip()] = True
    return result

def get_struct_field_offsets(StructCls):
    """
    Calculate field offsets and total struct size using ctypes alignment rules.
    Returns:
        offsets: dict of field_name -> offset
        total_size: size of the structure in bytes
    """
    offsets = {}
    offset = 0
    max_align = 1

    for field_name, field_ctype in StructCls._fields_:
        align = ctypes.alignment(field_ctype)
        size = ctypes.sizeof(field_ctype)

        # align current offset
        if offset % align != 0:
            offset += align - (offset % align)

        offsets[field_name] = offset
        offset += size

        if align > max_align:
            max_align = align

    # final struct size must be multiple of max_align
    if offset % max_align != 0:
        offset += max_align - (offset % max_align)

    return offsets, offset

def get_field_metadata(struct):
    StructCls = make_ctypes_struct(struct)
    metadata = {}
    fields = {}

    offsets, total_size = get_struct_field_offsets(StructCls)

    for field_name, field_ctype in StructCls._fields_:
        field_decl = next((f for f in struct.variables() if f.name == field_name), None)
        if field_decl is None:
            annotations = {}
        else:
            notes = get_annotations(field_decl)
            annotations = parse_annotations(notes)

        fields[field_name] = {
            "offset": offsets[field_name],
            "ctype": field_ctype,
            "annotations": annotations
        }

    metadata["fields"] = fields
    metadata["size"] = total_size
    metadata["struct_name"] = struct.name
    return metadata

class BufferReader:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def read_usize(self) -> int:
        # Assuming usize is 8 bytes on 64-bit, little-endian
        val = struct.unpack_from("<Q", self.data, self.offset)[0]
        self.offset += 8
        return val

    def read_int(self) -> int:
        # return self.read_usize()
        # Assuming 4-byte int, little-endian
        val = struct.unpack_from("<i", self.data, self.offset)[0]
        self.offset += 4
        return val

    def read_string(self) -> str:
        """Read a null-terminated string from the current offset."""
        start = self.offset
        # Find the next null byte
        end = self.data.find(b'\0', start)
        if end == -1:
            raise ValueError("Null-terminated string not found")

        val = self.data[start:end].decode("utf-8")
        self.offset = end + 1 # move past the null byte
        return val

    def read_bytes(self, size: int) -> bytes:
        val = self.data[self.offset:self.offset + size]
        self.offset += size
        return val

class SerializedTest:
    def __init__(self, module_name, target, runner, cpu):
        self.module_name = module_name
        self.target = target
        self.runner = runner
        self.cpu = cpu
        self.result_size = 0
        self.result_code = 0
        self.result = b''

    def __repr__(self):
        # Represent bytes as length and first few bytes in hex
        if self.result:
            preview = self.result[:8].hex() + ('...' if self.result_size > 8 else '')
        else:
            preview = 'empty'

        return (
            f"Test(module_name={self.module_name!r}, "
            f"target={hex(self.target)}, runner={hex(self.runner)}, cpu={hex(self.cpu)}, "
            f"result_size={hex(self.result_size)}, result_code={hex(self.result_code)}, "
            f"result={preview})"
        )

    def add_metadata(self, m):
        self.metadata = m


def load_run(data):
    reader = BufferReader(data)
    num_tests = reader.read_usize()

    tests = []
    for _ in range(num_tests):
        module_name = reader.read_string()
        cpu = reader.read_int()
        result_code = reader.read_int()
        runner = reader.read_int()
        target = reader.read_int()
        result_size = reader.read_usize()
        result = reader.read_bytes(result_size)

        t = SerializedTest(module_name, target, runner, cpu)
        t.result_size = result_size
        t.result_code = result_code
        t.result = result

        tests.append(t)

    return tests

def pretty_print_test(test, name=None):
    if isinstance(test, TestResults):
        s = test.outcome
        status = "OK" if test.ok() else "KO"
        print(f"  Module: {s.module_name}")
        print(f"  Status: {status}")
        print(f"  Size:   {s.result_size} bytes")
        print(f"  Fields:")
        for fname, fmeta in test._fields.items():
            ctype = fmeta["ctype"]
            offset = fmeta["offset"]
            fsize = ctypes.sizeof(ctype)
            # skip fields that extend beyond the available data
            if offset + fsize > len(test._data):
                print(f"    {fname}: <truncated, beyond result buffer>")
                continue
            if hasattr(ctype, "_length_") and hasattr(ctype, "_type_"):
                length = ctype._length_
                item_type = ctype._type_
                vals = test.raw(fname)
                mn = min(vals)
                mx = max(vals)
                show = vals[:6]
                trunc = "..." if length > 6 else ""
                print(f"    {fname}[{length}] ({offset}): "
                      f"min={mn:.4g}, max={mx:.4g}, "
                      f"vals=[{', '.join(f'{v:.4g}' for v in show)}{trunc}]")
            else:
                val = getattr(test, fname)
                print(f"    {fname} ({offset}): {val}")
    else:
        print(f"  Module: {name or '?'}")
        print(f"  Status: NOT FOUND")

def pretty_print_all(tests_dict):
    if not tests_dict:
        print("  No tests loaded.")
        return
    for name in sorted(tests_dict):
        pretty_print_test(tests_dict[name], name)


def plot_test(test):
    rows = 16
    cols = 16

    data = np.zeros((rows, cols))

    # Select the first annotated field
    annotated_fields = []
    for fname, fmeta in test._fields.items():
        if fmeta['annotations']:
            annotated_fields += [(fname, fmeta)]

    if len(annotated_fields) == 1:
        # Extract info
        field_name, field_meta = annotated_fields[0]
        offset = field_meta['offset']
        ctype_array = field_meta['ctype']
        annotations = field_meta['annotations']

        # Determine array size from ctypes
        # array_size = np.prod(ctype_array._length_) if hasattr(ctype_array, "_length_") else ctypes.sizeof(ctype_array) // ctypes.sizeof(ctype_array._type_)

        # Create a ctypes object from the binary buffer
        # print(len(test.result))
        # print(f"{offset}:{offset + ctypes.sizeof(ctype_array)}")
        buf = (ctype_array).from_buffer_copy(test._data[offset:offset + ctypes.sizeof(ctype_array)])

        # Convert to NumPy array
        data = np.array(buf[:], dtype=np.float64)

        # Reshape according to annotations (x, y)
        plot_type = annotations.get('to_plot')

        if plot_type == 'heatmap':
            x_label, x_range = (annotations.get('x').split(':')[0] ,int(annotations.get('x').split(':')[1]))
            y_label, y_range = (annotations.get('y').split(':')[0] ,int(annotations.get('y').split(':')[1]))
            data = data.reshape((x_range, y_range))
            plt.imshow(data, cmap='viridis', origin='lower', aspect='auto')
            plt.colorbar(label=annotations.get("values"))
            plt.xlabel(x_label)
            plt.ylabel(y_label if plot_type == 'heatmap' else "Value")
            plt.title(f"Heatmap for field: {field_name}")
            plt.show()

        elif plot_type == 'line':
            y_label, y_range = (annotations.get('y').split(':')[0] ,int(annotations.get('y').split(':')[1]))
            x_label, x_range = (annotations.get('x').split(':')[0] ,int(annotations.get('x').split(':')[1]))
            # data = data.reshape(y_range)
            plt.title(f"Line plot for field: {field_name}")
            plt.plot(data, label=y_label)
            plt.xlabel(x_label)
            plt.ylabel(y_label)
            plt.legend()
            plt.show()

        elif plot_type == 'scatter':
            if data.ndim == 2 and data.shape[1] >= 2:
                plt.scatter(data[:, 0], data[:, 1])
            else:
                plt.scatter(range(len(data)), data)
        else:
            print(f"[WARN] Unknown plot type '{plot_type}' for field '{field_name}'")
    else:
        grouped = {}
        # pprint(annotated_fields)
        # grouped = {}

        for key, entry in annotated_fields:
            ann = entry["annotations"]
            name = ann["name"]

            grouped.setdefault(name, []).append({
                "name": key,
                "x": ann["x"],
                "y": ann["y"],
                "to_plot": ann["to_plot"],
                "values": ann.get("values"),
                "offset": entry["offset"],
                "ctype": entry["ctype"],
            })
        # for key, entry in zip(annotated_fields[0::2], annotated_fields[1::2]):
        #     print("-----------")
        #     pprint(key)
        #     ann = entry["annotations"]
        #     name = ann["name"]
        #
        #     grouped.setdefault(name, []).append({
        #         "name": key,
        #         "x": ann["x"],
        #         "y": ann["y"],
        #         "to_plot":ann["to_plot"],
        #         "values": ann.get("values"),
        #         "offset": entry["offset"],
        #         "ctype": entry["ctype"]
        #     })

        def check_consistency(grouped):
            for group_name, items in grouped.items():
                xs = {item["x"] for item in items}
                ys = {item["y"] for item in items}
                print(items)
                ys = {item["to_plot"] for item in items}

                same_x = (len(xs) == 1)
                same_y = (len(ys) == 1)
                same_plot = (len(ys) == 1)

                if not same_x or not same_y or not same_plot:
                    return False

            return True

        consistent = check_consistency(grouped)
        if not consistent:
            print("ERROR: inconsistent plot annotations")
            sys.exit(1)

        assert len(grouped) == 1, "Multiple plots not supported yet"
        plot = next(iter(grouped.values()))

        y_label, y_range = (plot[0].get('y').split(':')[0] ,int(plot[0].get('y').split(':')[1]))
        x_label, x_range = (plot[0].get('x').split(':')[0] ,int(plot[0].get('x').split(':')[1]))

        plot_type = plot[0].get('to_plot')

        def get_numpy_arr(field_meta):
            offset = field_meta['offset']
            ctype_array = field_meta['ctype']

            buf = ctype_array.from_buffer_copy(
                test._data[offset : offset + ctypes.sizeof(ctype_array)]
            )
            return np.array(buf[:], dtype=np.float64)
            # offset = field_meta['offset']
            # ctype_array = field_meta['ctype']
            #
            # buf = (ctype_array).from_buffer_copy(test.result[offset:offset + ctypes.sizeof(ctype_array)])
            # return np.array(buf[:], dtype=np.float64)

        if plot_type == 'heatmap':
            assert False, "TODO"
            data = get_numpy_arr(plot[0]).reshape((x_range, y_range))
            plt.imshow(data, cmap='viridis', origin='lower', aspect='auto')
            plt.colorbar(label=plot[0].get("values"))
            plt.xlabel(x_label)
            plt.ylabel(y_label)
            plt.show()

        elif plot_type == 'line':
            for readings in plot:
                data = get_numpy_arr(readings)
                plt.plot(data, label=readings.get('values', readings['name']))

            plt.xlabel(x_label)
            plt.ylabel(y_label)
            plt.legend()
            plt.show()

            # for readings in plot:
            #     plt.plot(get_numpy_arr(readings), label=readings.get('values'))
            #
            # plt.xlabel(x_label)
            # plt.ylabel(y_label)
            # plt.legend()
            # plt.show()

        else:
            print(f"[WARN] Unknown plot type '{plot_type}' for field '{field_name}'")


class TestResults:
    RES_OK = 0
    RES_KO = 1

    def __init__(self, meta, serialized):
        self.outcome: SerializedTest = serialized
        self._data = serialized.result
        self.meta = meta
        self._fields = meta['fields']

    def __getattr__(self, name):
        # Called only if normal attribute lookup fails
        if name not in self._fields:
            raise AttributeError(f"{type(self).__name__!s} has no attribute {name!r}")

        field = self._fields[name]
        ctype = field["ctype"]
        offset = field["offset"]

        # Create a ctypes object from buffer at offset
        return ctype.from_buffer_copy(
            self._data[offset : offset + ctypes.sizeof(ctype)]
        ).value

    def raw(self, name):
        # Called only if normal attribute lookup fails
        if name not in self._fields:
            raise AttributeError(f"{type(self).__name__!s} has no attribute {name!r}")

        field = self._fields[name]
        ctype = field["ctype"]
        offset = field["offset"]

        # Get the raw bytes
        buf = self._data[offset : offset + ctypes.sizeof(ctype)]

        # Determine number of elements (assuming ctype is an array or single double)
        if hasattr(ctype, "_length_") and hasattr(ctype, "_type_"):
            # ctypes array
            length = ctype._length_
            item_type = ctype._type_
            if item_type is ctypes.c_double:
                # Unpack as doubles
                return list(struct.unpack(f"{length}d", buf))
            else:
                item_size = ctypes.sizeof(item_type)
                return [item_type.from_buffer_copy(
                    buf[i * item_size : (i + 1) * item_size]
                ).value for i in range(length)]
        else:
            # Single value
            if ctype is ctypes.c_double:
                return [struct.unpack("d", buf)[0]]
            else:
                return [ctype.from_buffer_copy(buf).value]

    def ok(self):
        return self.outcome.result_code == TestResults.RES_OK

_META_CACHE: dict[str, dict] = {}

def parse_test(test: SerializedTest):
    name = test.module_name
    if name in _META_CACHE:
        return TestResults(_META_CACHE[name], test)

    gpath, gname = utils.find_xml_generator()

    xml_generator_config = parser.xml_generator_configuration_t(
        xml_generator_path=gpath,
        xml_generator=gname,
        include_paths=[
            "include/",
            "modules/"
        ],
        define_symbols={
            "RUNNER_USER": "1",
            "TARGET_X86_64": "1",
        },
        cflags=(
            "-x c"
            " -D_Bool=bool"
            " -Wno-unused-command-line-argument"
            " -Wno-macro-redefined"
        )
    )

    decls = parser.parse([f"modules/{name}/{name}_test.h"], xml_generator_config)
    global_ns = declarations.get_global_namespace(decls)
    global_ns.init_optimizer()
    for struct in global_ns.classes():
        meta = get_field_metadata(struct)
        if meta["struct_name"] == f"{name}_result_t":
            _META_CACHE[name] = meta
            return TestResults(meta, test)

    # annotated_structs = [
    #     s for s in structs
    #     if any(isinstance(f, dict) and f.get('annotations')
    #         for k, f in s['fields'].items()
    #         if not k.startswith('_'))
    # ]


def load_tests(run_file):
    tests = []
    with open(run_file, "rb") as f:
        run = f.read()
        tests = load_run(run)

    parsed_tests = {}
    for test in tests:
        if test.module_name == "root": continue
        try:
            parsed_tests[test.module_name] = parse_test(test)
        except Exception as e:
            print(f"[WARN] Skipping test {test.module_name}: {e}")

    return parsed_tests


tests: dict[str, TestResults] = {}
def get_test(t: str) -> TestResults:
    return tests.get(t)

class Attack:
    def __init__(self, name, requirements, vmax):
        self.name = name
        self.requirements = requirements
        self.vmax = vmax

        self.v = sum(1 for r in requirements if r())
        self.severity = 0

        self.features = {}
        self.multipliers = {}

    def add_feature(self, name, value, present=True):
        self.features[name] = {
            "value": value,
            "present": present
        }

    def add_requirement(self, name, present):
        # Requirement is stored as feature with value = presence
        self.features[name] = {
            "value": float(present),
            "present": present
        }

    def add_multiplier(self, name, value):
        self.multipliers[name] = value

    def compute(self, base_value):
        mult = 1
        for m in self.multipliers.values():
            mult *= m

        self.severity = clamp(base_value * mult)
        return self

    def breakdown_str(self):
        parts = []

        for k, d in self.features.items():
            if d["present"]:
                parts.append(f"{k}:{clamp(d['value'])*100:.1f}%")
            else:
                parts.append(f"{k}:OFF")

        for k, v in self.multipliers.items():
            parts.append(f"{k}(x{v:.1f})")

        return ", ".join(parts)

@dataclass(frozen=True)
class Severity:
    value: float
    label: str

def avg(*vals):
    return sum(vals) / len(vals) if vals else 0

class Test(StrEnum):
    CACHE = "cache"
    PHT = "pht"
    ROB = "rob"
    LAP = "lap"
    RSB = "rsb"
    SIMD = "simd"
    PIPELINE = "pipeline"
    LFB = "lfb"
    SMT = "smt"
    TLB = "tlb"
    SGX = "sgx"
    SPEC_MEM_ACCESS = "spec_mem_access"
    OOO_MEM_ACCESS = "ooo_mem_access"
    STL_FORWARD = "stl_forward"
    STALE_CODE_EXECUTION = "stale_code_execution"
    BTB = "btb"
    FPU = "fpu"
    LOCKS = "locks"
    O3 = "o3"
    BTB_DETECTION = "btb_detection"
    STL_DETECTION = "stl_detection"
    RSB_DETECTION = "rsb_detection"
    TLB_WINDOW = "tlb_window"
    LFB_WINDOW = "lfb_window"
    LAP_WINDOW = "lap_window"
    KERNEL_RSB = "kernel_rsb"
    KERNEL_BTI = "kernel_bti"
    KERNEL_LAP = "kernel_lap"
    KERNEL_LFB = "kernel_lfb"
    KERNEL_STL = "kernel_stl"
    KERNEL_STALE_CODE = "kernel_stale_code"
    KERNEL_O3 = "kernel_o3"
    KERNEL_TLB = "kernel_tlb"
    KERNEL_MISPREDICTION = "kernel_misprediction"
    USER_RSB = "user_rsb"
    USER_BTI = "user_bti"
    USER_LAP = "user_lap"
    USER_LFB = "user_lfb"
    USER_STL = "user_stl"
    USER_STALE_CODE = "user_stale_code"
    USER_O3 = "user_o3"
    USER_TLB = "user_tlb"
    USER_MISPREDICTION = "user_misprediction"
    PROCESS_RSB = "process_rsb"
    PROCESS_BTI = "process_bti"
    PROCESS_LAP = "process_lap"
    PROCESS_LFB = "process_lfb"
    PROCESS_STL = "process_stl"
    PROCESS_STALE_CODE = "process_stale_code"
    PROCESS_O3 = "process_o3"
    PROCESS_TLB = "process_tlb"
    PROCESS_MISPREDICTION = "process_misprediction"
    OVERALL = "Agregated Processor vulnerability"

ALL_MODULES = [m for m in Test if m != Test.OVERALL]

EXCLUDED: set[Test] = {
    Test.CACHE, Test.ROB, Test.SIMD, Test.PIPELINE,
    Test.SMT, Test.SGX, Test.FPU, Test.LOCKS,
}

VARIANT_PRIV: dict[Test, str] = {
    Test.KERNEL_RSB: "kernel space",
    Test.KERNEL_BTI: "kernel space",
    Test.KERNEL_LAP: "kernel space",
    Test.KERNEL_LFB: "kernel space",
    Test.KERNEL_STL: "kernel space",
    Test.KERNEL_STALE_CODE: "kernel space",
    Test.KERNEL_O3: "kernel space",
    Test.KERNEL_TLB: "kernel space",
    Test.KERNEL_MISPREDICTION: "kernel space",
    Test.USER_RSB: "user space",
    Test.USER_BTI: "user space",
    Test.USER_LAP: "user space",
    Test.USER_LFB: "user space",
    Test.USER_STL: "user space",
    Test.USER_STALE_CODE: "user space",
    Test.USER_O3: "user space",
    Test.USER_TLB: "user space",
    Test.USER_MISPREDICTION: "user space",
    Test.PROCESS_RSB: "same space",
    Test.PROCESS_BTI: "same space",
    Test.PROCESS_LAP: "same space",
    Test.PROCESS_LFB: "same space",
    Test.PROCESS_STL: "same space",
    Test.PROCESS_STALE_CODE: "same space",
    Test.PROCESS_O3: "same space",
    Test.PROCESS_TLB: "same space",
    Test.PROCESS_MISPREDICTION: "same space",
}

FEATURE_VARIANTS: dict[Test, list[Test]] = {
    Test.PHT: [Test.KERNEL_MISPREDICTION, Test.USER_MISPREDICTION, Test.PROCESS_MISPREDICTION],
    Test.SPEC_MEM_ACCESS: [Test.KERNEL_MISPREDICTION, Test.USER_MISPREDICTION, Test.PROCESS_MISPREDICTION],
    Test.LAP: [Test.KERNEL_LAP, Test.USER_LAP, Test.PROCESS_LAP],
    Test.LFB: [Test.KERNEL_LFB, Test.USER_LFB, Test.PROCESS_LFB],
    Test.OOO_MEM_ACCESS: [Test.KERNEL_O3, Test.USER_O3, Test.PROCESS_O3],
    Test.BTB: [Test.KERNEL_BTI, Test.USER_BTI, Test.PROCESS_BTI],
    Test.STL_FORWARD: [Test.KERNEL_STL, Test.USER_STL, Test.PROCESS_STL],
    Test.RSB: [Test.KERNEL_RSB, Test.USER_RSB, Test.PROCESS_RSB],
    Test.STALE_CODE_EXECUTION: [Test.KERNEL_STALE_CODE, Test.USER_STALE_CODE, Test.PROCESS_STALE_CODE],
    Test.TLB: [Test.KERNEL_TLB, Test.USER_TLB, Test.PROCESS_TLB],
}

PRIV_ORDER = ["excluded", "no", "same space", "user space", "kernel space"]
PRIV_RANK = {p: i for i, p in enumerate(PRIV_ORDER)}

def compute_priv(test_enum, severity=0):
    if test_enum in EXCLUDED:
        return "excluded"
    if severity <= 0:
        return "no"
    candidates = []
    for v in FEATURE_VARIANTS.get(test_enum, []):
        tr = get_test(v)
        if tr and tr.ok():
            candidates.append(VARIANT_PRIV.get(v, "excluded"))
    return max_priv(candidates) if candidates else "no"

def max_priv(privs):
    best = "excluded"
    for p in privs:
        if PRIV_RANK.get(p, 0) > PRIV_RANK.get(best, 0):
            best = p
    return best

BASE_FEATURE_MAP: dict[str, Test] = {
    "kernel_rsb": Test.RSB,
    "kernel_bti": Test.BTB,
    "kernel_lap": Test.LAP,
    "kernel_lfb": Test.LFB,
    "kernel_stl": Test.STL_FORWARD,
    "kernel_stale_code": Test.STALE_CODE_EXECUTION,
    "kernel_o3": Test.OOO_MEM_ACCESS,
    "kernel_misprediction": Test.PHT,
    "user_rsb": Test.RSB,
    "user_bti": Test.BTB,
    "user_lap": Test.LAP,
    "user_lfb": Test.LFB,
    "user_stl": Test.STL_FORWARD,
    "user_stale_code": Test.STALE_CODE_EXECUTION,
    "user_o3": Test.OOO_MEM_ACCESS,
    "user_misprediction": Test.PHT,
    "process_rsb": Test.RSB,
    "process_bti": Test.BTB,
    "process_lap": Test.LAP,
    "process_lfb": Test.LFB,
    "process_stl": Test.STL_FORWARD,
    "process_stale_code": Test.STALE_CODE_EXECUTION,
    "process_o3": Test.OOO_MEM_ACCESS,
    "process_misprediction": Test.PHT,
    "user_tlb": Test.TLB,
}

TESTS: dict[str, list[Test]] = {
    m.value: [m] for m in ALL_MODULES
} | {
    k: [v] for k, v in BASE_FEATURE_MAP.items()
} | {
    "ridl": [Test.LFB, Test.OOO_MEM_ACCESS, Test.SMT],
    "cacheout": [Test.SGX, Test.SMT, Test.LFB],
    "fallout": [Test.STL_FORWARD, Test.OOO_MEM_ACCESS, Test.TLB],
    "foreshadow": [Test.STL_FORWARD, Test.OOO_MEM_ACCESS, Test.TLB, Test.BTB],
    "zombieload": [Test.SMT, Test.OOO_MEM_ACCESS, Test.SGX],
    "meltdown": [Test.OOO_MEM_ACCESS],
    "lvi": [Test.OOO_MEM_ACCESS, Test.SPEC_MEM_ACCESS, Test.STL_FORWARD, Test.LFB],
    "spectre_rsb": [Test.SPEC_MEM_ACCESS, Test.RSB, Test.BTB],
    "spectre_stl": [Test.SPEC_MEM_ACCESS, Test.STL_FORWARD],
    "spectre_v1": [Test.SPEC_MEM_ACCESS, Test.PHT],
    "spectre_btb": [Test.SPEC_MEM_ACCESS, Test.BTB],
    "slap": [Test.LAP, Test.SPEC_MEM_ACCESS],
    "scsb": [Test.STALE_CODE_EXECUTION, Test.SPEC_MEM_ACCESS],
}


def feature_is_mitigated(attack_name, feature):
    key = attack_name.strip().lower().replace(" ", "_")

    if key in BASE_FEATURE_MAP:
        return "x" if BASE_FEATURE_MAP[key] == feature else ""

    if key not in TESTS:
        return ""

    for t in TESTS[key]:
        if t == feature:
            return "x"

    return ""



def process_run(run_file, plot=None, pp=None, export=False):
    global tests

    tests = load_tests(run_file)

    if args.plot is not None:
        plot_test(tests[plot])
        exit(0)

    if pp:
        if isinstance(pp, str):
            if pp in tests:
                pretty_print_test(tests[pp])
            else:
                print(f"Module '{pp}' not found in run file.")
        else:
            pretty_print_all(tests)
        return

    score = 0
    max_score = 0

    def t(name):
        r = get_test(name)
        if r is None:
            print(f"[WARN] Test '{name}' not found, using dummy")
            class Dummy:
                @staticmethod
                def ok(): return False
                def raw(self, n): return [0, 0]
                lfb_size = 0
                rob_size = 0
                cached_access_time = 0
                uncached_access_time = 1
                branch_taken_time_tot = 0
                taken_counted = 1
                branch_not_taken_time_tot = 0
                not_taken_counted = 1
                random_time_unroll = 0
                random_time = 1
                fixed_time = 0
                fixed_time_unroll = 1
                window_size = 0
                window_full = 1
                ba = 0
                ab = 0
                aa = 0
            return Dummy()
        return r

    cache = t(Test.CACHE)
    pht = t(Test.PHT)
    spec_mem_access = t(Test.SPEC_MEM_ACCESS)
    ooo_mem_access = t(Test.OOO_MEM_ACCESS)
    rob = t(Test.ROB)
    lap = t(Test.LAP)
    rsb = t(Test.RSB)
    simd = t(Test.SIMD)
    btb = t(Test.BTB)
    pipeline = t(Test.PIPELINE)
    lfb = t(Test.LFB)
    smt = t(Test.SMT)
    stl = t(Test.STL_FORWARD)
    tlb = t(Test.TLB)
    sgx = t(Test.SGX)
    o3 = t(Test.O3)
    stale_code_execution = t(Test.STALE_CODE_EXECUTION)
    user_break_tests = [Test.LFB, Test.USER_LFB, Test.USER_O3,
                        Test.USER_STALE_CODE, Test.USER_STL,
                        Test.USER_MISPREDICTION, Test.USER_RSB,
                        Test.USER_BTI, Test.USER_LAP]
    kernel_break_tests = [Test.KERNEL_LFB, Test.KERNEL_O3,
                          Test.KERNEL_STALE_CODE, Test.KERNEL_STL,
                          Test.KERNEL_MISPREDICTION, Test.KERNEL_RSB,
                          Test.KERNEL_BTI, Test.KERNEL_LAP]
    process_break_tests = [Test.PROCESS_LFB, Test.PROCESS_O3,
                           Test.PROCESS_STALE_CODE, Test.PROCESS_STL,
                           Test.PROCESS_MISPREDICTION, Test.PROCESS_RSB,
                           Test.PROCESS_BTI, Test.PROCESS_LAP]
    user_boundary_ok = any(get_test(t).ok() for t in user_break_tests if t in tests)
    kernel_boundary_ok = any(get_test(t).ok() for t in kernel_break_tests if t in tests)
    process_boundary_ok = any(get_test(t).ok() for t in process_break_tests if t in tests)
    mprotected_access_ok = process_boundary_ok and user_boundary_ok
    kernel_access_ok = kernel_boundary_ok and user_boundary_ok and process_boundary_ok

    vulnerability = 0
    severity = 0

    #################
    # FEATURE STUDY #
    #################

    def diff_power(a, b):
        denom = abs(a) + abs(b)
        return abs(a - b) / denom if denom > 0 else 0.0

    cache_severity = diff_power(cache.cached_access_time, cache.uncached_access_time)
    # print (cache.cached_access_time, cache.uncached_access_time)
    pht_severity = diff_power(pht.branch_taken_time_tot / pht.taken_counted,
                              pht.branch_not_taken_time_tot / pht.not_taken_counted)
    rob_severity = rob.ok()
    lap_severity = diff_power(max(lap.random_time_unroll, lap.random_time),
                              max(lap.fixed_time, lap.fixed_time_unroll))

    simd_severity = simd.ok()
    pipeline_severity = pipeline.ok()

    rob_size = rob.rob_size
    # print(f"rob {rob_size}")

    # The bigger is the lfb, the easier is to exploit it, this number comes from
    # reverse engineering of other processors.
    # TODO: Find a better way to measure this
    max_lfb_size = 25
    lfb_severity = clamp(lfb.lfb_size / max_lfb_size) if max_lfb_size > 0 else 0
    smt_severity = smt.ok()

    # TODO: Use latencies
    tlb_severity = diff_power(max(tlb.raw("readings")[1:]), min(tlb.raw("readings")[1:]))
    # print(tlb.raw("readings")[1:])
    # tlb_severity = tlb.ok()
    sgx_severity = sgx.ok()

    spec_window = 0
    ooo_window = 0
    bti_window = 0
    stl_window = 0
    rsb_window = 0
    tlb_window = 0
    lfb_window = 0
    lap_window = 0
    btb_window_time = max(btb.ba, btb.ab) - btb.aa
    # print(btb.ba, btb.ab, btb.aa)
    # print(f"btb {btb_window_time}")
    if spec_mem_access.ok():
        spec_window = spec_mem_access.window_size
        # print(spec_window)
        spec_window_time = spec_mem_access.window_full
        ## TODO: Improve
        rsb_window_time = max(rsb.raw("poison_readings")) - min(rsb.raw("poison_readings"))
        rsb_window = spec_window / spec_window_time * rsb_window_time if spec_window_time > 0 else 0

        # btb.ba = 38
        # btb.aa = 27
        btb_window_time = max(btb.ba, btb.ab) - btb.aa
        # print(f"btb {btb_window_time}")
        # print(btb.ba, btb.ab, btb.aa)

        if btb.ok() and spec_window_time > 0:
            bti_window = spec_window / spec_window_time * btb_window_time

        lap_window_time = max(lap.random_time_unroll, lap.random_time) - max(lap.fixed_time, lap.fixed_time_unroll)
        if lap.ok() and spec_window_time > 0 and lap_window_time > 0:
            lap_window = spec_window / spec_window_time * lap_window_time

    if ooo_mem_access.ok():
        ooo_window_time = ooo_mem_access.window_full
        ooo_window = ooo_mem_access.window_size

        stl_window_time = max(stl.raw("timings"))
        if stl.ok() and ooo_window_time > 0:
            stl_window = ooo_window / ooo_window_time * stl_window_time

        tlb_window_time = max(tlb.raw("readings")[1:]) - min(tlb.raw("readings")[1:])
        if tlb.ok() and ooo_window_time > 0:
            tlb_window = ooo_window / ooo_window_time * tlb_window_time

        lfb_window_time = max(lfb.raw("readings")) - min(lfb.raw("readings"))
        if lfb.ok() and ooo_window_time > 0:
            lfb_window = ooo_window / ooo_window_time * lfb_window_time

    spec_window_severity = 0
    if spec_mem_access.ok() and rob.ok() and rob_size > 0:
        spec_window_severity = clamp(spec_mem_access.window_size / rob_size)

    ooo_window_severity = 0
    if ooo_mem_access.ok() and rob.ok() and rob_size > 0:
        ooo_window_severity = clamp(ooo_window / rob_size)

    bti_window_severity = 0
    if btb.ok() and rob.ok() and rob_size > 0:
        bti_window_severity = min(bti_window / (rob_size), 1.0)

    stl_window_severity = 0
    if stl.ok() and rob.ok() and rob_size > 0:
        stl_window_severity = min(stl_window / (rob_size), 1.0)

    rsb_window_severity = 0
    if rsb.ok() and rob.ok() and rob_size > 0:
        rsb_window_severity = min(rsb_window / (rob_size), 1.0)

    tlb_window_severity = 0
    if tlb.ok() and rob.ok() and rob_size > 0:
        tlb_window_severity = min(tlb_window / (rob_size), 1.0)

    lfb_window_severity = 0
    if lfb.ok() and rob.ok() and rob_size > 0:
        lfb_window_severity = min(lfb_window / (rob_size), 1.0)

    lap_window_severity = 0
    if lap.ok() and rob.ok() and rob_size > 0:
        lap_window_severity = min(lap_window / (rob_size), 1.0)

    stale_code_window_severity = 0
    if stale_code_execution.ok() and rob.ok():
        stale_code_window_severity = spec_window_severity

    o3_detection_severity = 1.0 if o3.ok() else 0.0

    btb_detection_severity = 0
    if btb.ok():
        btb_detection_severity = diff_power(max(btb.ba, btb.ab), btb.aa)

    stl_detection_severity = 0
    if stl.ok():
        timings = stl.raw("timings")
        stl_detection_severity = diff_power(max(timings), min(timings))

    rsb_detection_severity = 0
    if rsb.ok():
        poison = rsb.raw("poison_readings")
        normal = rsb.raw("normal_readings")
        rsb_detection_severity = diff_power(max(poison), min(normal))

    severities: dict[Test, Severity] = {
        Test.CACHE: Severity(cache_severity, "cache"),
        Test.PHT: Severity(pht_severity, "pht"),
        Test.ROB: Severity(rob_severity, "rob"),
        Test.LAP: Severity(lap_severity, "lap"),
        Test.SIMD: Severity(simd_severity, "simd"),
        Test.PIPELINE: Severity(pipeline_severity, "pipeline"),
        Test.LFB: Severity(lfb_severity, "lfb"),
        Test.SMT: Severity(smt_severity, "smt"),
        Test.TLB: Severity(tlb_severity, "tlb"),
        Test.SGX: Severity(sgx_severity, "sgx"),
        Test.SPEC_MEM_ACCESS: Severity(spec_window_severity, "spec_window"),
        Test.OOO_MEM_ACCESS: Severity(ooo_window_severity, "ooo_window"),
        Test.BTB: Severity(bti_window_severity, "bti_window"),
        Test.STL_FORWARD: Severity(stl_window_severity, "stl_window"),
        Test.RSB: Severity(rsb_window_severity, "rsb_window"),
        Test.STALE_CODE_EXECUTION: Severity(stale_code_window_severity, "stale_code_window"),
        Test.TLB_WINDOW: Severity(tlb_window_severity, "tlb_window"),
        Test.LFB_WINDOW: Severity(lfb_window_severity, "lfb_window"),
        Test.LAP_WINDOW: Severity(lap_window_severity, "lap_window"),
        Test.O3: Severity(o3_detection_severity, "o3"),
        Test.BTB_DETECTION: Severity(btb_detection_severity, "btb"),
        Test.STL_DETECTION: Severity(stl_detection_severity, "stl"),
        Test.RSB_DETECTION: Severity(rsb_detection_severity, "rsb"),
    }
    def compute_overall(severities: dict[Test, Severity]) -> float:
        relative_value = 1 / len(severities)
        values = [s.value * relative_value for s in severities.values()]

        if not values:
            return 0.0

        return sum(v for v in values)

    severities[Test.OVERALL] = Severity(compute_overall(severities), "Agregated Processor vulnerability")

    for name, value in severities.items():
        print(f"{name:20s}: {clamp(value.value)*100:.2f}%")

    ################
    # ATTACK STUDY #
    ################
    attacks = {}

    # ---------------- RIDL ----------------
    a = Attack("ridl", [lfb.ok, ooo_mem_access.ok, smt.ok], 3)

    a.add_requirement("lfb_req", lfb.ok())
    a.add_requirement("ooo_req", ooo_mem_access.ok())
    a.add_requirement("smt_req", smt.ok())

    a.add_feature("ooo_window", ooo_window_severity, ooo_mem_access.ok())
    a.add_feature("lfb_size", lfb_severity, lfb.ok())
    a.add_feature("cache", cache_severity)

    if lfb.ok() and ooo_mem_access.ok():
        base = avg(ooo_window_severity, lfb_severity)

        if smt.ok():
            a.add_multiplier("smt", 2)

        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- CacheOut ----------------
    a = Attack("cacheout", [sgx.ok, smt.ok, lfb.ok], 3)

    a.add_requirement("sgx_req", sgx.ok())
    a.add_requirement("smt_req", smt.ok())
    a.add_requirement("lfb_req", lfb.ok())

    a.add_feature("ooo_window", ooo_window_severity, ooo_mem_access.ok())
    a.add_feature("lfb_size", lfb_severity, lfb.ok())
    a.add_feature("cache", cache_severity)

    if sgx.ok() and lfb.ok():
        base = avg(ooo_window_severity, lfb_severity)

        if smt.ok():
            a.add_multiplier("smt", 2)

        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Fallout ----------------
    a = Attack("fallout", [stl.ok, ooo_mem_access.ok, tlb.ok], 3)

    a.add_requirement("stl_req", stl.ok())
    a.add_requirement("ooo_req", ooo_mem_access.ok())
    a.add_requirement("tlb_req", tlb.ok())

    a.add_feature("ooo_window", ooo_window_severity, ooo_mem_access.ok())
    a.add_feature("lfb_size", lfb_severity, lfb.ok())
    a.add_feature("cache", cache_severity)

    if stl.ok() and ooo_mem_access.ok() and tlb.ok():
        base = avg(ooo_window_severity, lfb_severity)
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Foreshadow ----------------
    a = Attack("foreshadow", [stl.ok, ooo_mem_access.ok, tlb.ok, btb.ok], 4)

    # print(f"STL {stl.ok()}")
    # print(f"OOO {ooo_mem_access.ok()}")

    a.add_requirement("stl_req", stl.ok())
    a.add_requirement("ooo_req", ooo_mem_access.ok())
    a.add_requirement("tlb_req", tlb.ok())
    a.add_requirement("btb_req", btb.ok())

    a.add_feature("ooo_window", ooo_window_severity, ooo_mem_access.ok())
    a.add_feature("bti_window", bti_window_severity, btb.ok())
    a.add_feature("cache", cache_severity)

    if stl.ok() and ooo_mem_access.ok() and tlb.ok():
        base = ooo_window_severity

        if btb.ok():
            base += bti_window_severity

        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Zombieload ----------------
    a = Attack("zombieload", [smt.ok, ooo_mem_access.ok, sgx.ok], 3)

    a.add_requirement("smt_req", smt.ok())
    a.add_requirement("ooo_req", ooo_mem_access.ok())
    a.add_requirement("sgx_req", sgx.ok())

    a.add_feature("ooo_window", ooo_window_severity, ooo_mem_access.ok())
    a.add_feature("lfb_size", lfb_severity, lfb.ok())
    a.add_feature("cache", cache_severity)

    if smt.ok() and ooo_mem_access.ok():
        base = avg(ooo_window_severity, lfb_severity)

        if sgx.ok():
            a.add_multiplier("sgx", 2)

        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Meltdown ----------------
    a = Attack("meltdown", [ooo_mem_access.ok], 1)

    a.add_requirement("ooo_req", ooo_mem_access.ok())

    a.add_feature("ooo_window", ooo_window_severity, ooo_mem_access.ok())
    a.add_feature("cache", cache_severity)

    if ooo_mem_access.ok():
        base = ooo_window_severity
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- LVI ----------------
    a = Attack("lvi", [ooo_mem_access.ok, spec_mem_access.ok], 2)

    a.add_requirement("ooo_req", ooo_mem_access.ok())
    a.add_requirement("spec_req", spec_mem_access.ok())

    a.add_feature("stl_window", stl_window_severity, stl.ok())
    a.add_feature("lfb_size", lfb_severity, lfb.ok())
    a.add_feature("cache", cache_severity)

    if ooo_mem_access.ok() and spec_mem_access.ok():
        base = avg(stl_window_severity, lfb_severity)
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Spectre RSB ----------------
    a = Attack("rsb", [spec_mem_access.ok, btb.ok], 2)

    a.add_requirement("spec_req", spec_mem_access.ok())
    a.add_requirement("btb_req", btb.ok())
    # print(f"BTB {btb.ok()}")

    a.add_feature("rsb_window", rsb_window_severity, rsb.ok())
    a.add_feature("bti_window", bti_window_severity, btb.ok())
    a.add_feature("cache", cache_severity)

    if spec_mem_access.ok():
        base = rsb_window_severity

        if btb.ok():
            base += bti_window_severity

        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Spectre STL ----------------
    a = Attack("stl", [stl.ok, spec_mem_access.ok], 2)

    a.add_requirement("stl_req", stl.ok())
    a.add_requirement("spec_req", spec_mem_access.ok())

    a.add_feature("stl_window", stl_window_severity, stl.ok())
    a.add_feature("cache", cache_severity)

    if stl.ok() and spec_mem_access.ok():
        base = stl_window_severity
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Spectre V1 ----------------
    a = Attack("spectre v1", [spec_mem_access.ok, pht.ok], 2)

    a.add_requirement("spec_req", spec_mem_access.ok())
    a.add_requirement("pht_req", pht.ok())

    # print(f"PHT {pht.ok()}")

    a.add_feature("spec_window", spec_window_severity, spec_mem_access.ok())
    a.add_feature("cache", cache_severity)

    if spec_mem_access.ok() and pht.ok():
        base = spec_window_severity
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- Spectre BTB ----------------
    a = Attack("btb", [spec_mem_access.ok, btb.ok], 2)

    a.add_requirement("spec_req", spec_mem_access.ok())
    a.add_requirement("btb_req", btb.ok())

    a.add_feature("bti_window", bti_window_severity, btb.ok())
    a.add_feature("cache", cache_severity)

    if spec_mem_access.ok() and btb.ok():
        base = bti_window_severity
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- SLAP ----------------
    a = Attack("slap", [lap.ok, spec_mem_access.ok], 2)

    a.add_requirement("lap_req", lap.ok())
    a.add_requirement("spec_req", spec_mem_access.ok())

    a.add_feature("spec_window", spec_window_severity, spec_mem_access.ok())
    a.add_feature("lap", lap_severity, lap.ok())
    a.add_feature("cache", cache_severity)

    if lap.ok() and spec_mem_access.ok():
        base = spec_window_severity * lap_severity
        a.compute(base * cache_severity)

    attacks[a.name] = a


    # ---------------- SCSB ----------------
    a = Attack("scsb", [stale_code_execution.ok, spec_mem_access.ok], 2)

    a.add_requirement("stale_exec_req", stale_code_execution.ok())
    a.add_requirement("spec_req", spec_mem_access.ok())

    a.add_feature("spec_window", spec_window_severity, spec_mem_access.ok())
    a.add_feature("cache", cache_severity)

    if stale_code_execution.ok() and spec_mem_access.ok():
        base = spec_window_severity
        a.compute(base * cache_severity)

    attacks[a.name] = a

    m_multiplier = 1
    if mprotected_access_ok:
        m_multiplier *= 2
    if kernel_access_ok:
        m_multiplier *= 2

    max_vuln = sum(a.vmax for a in attacks.values())

    def normalize(a):
        mult = m_multiplier if a.name in {
            "meltdown", "foreshadow", "fallout",
            "cacheout", "ridl", "lvi", "zombieload"
        } else 1

        return (
            clamp(a.v / max_vuln),
            clamp((a.severity * mult) / max_vuln)
        )


    print("=== Individual Vulnerabilities & Severities ===")

    for a in attacks.values():
        nv, ns = normalize(a)

        print(
            f"{a.name.capitalize() + ' like':<16} -> "
            f"Vulnerability: {clamp(a.v/a.vmax)*100:6.2f}%, "
            f"Severity: {clamp(a.severity/a.vmax)*100:6.2f}% | "
            f"Features: [{a.breakdown_str()}]"
        )


    total_vuln = sum(normalize(a)[0] for a in attacks.values())
    total_sev  = sum(normalize(a)[1] for a in attacks.values())

    print("\n=== Overall ===")
    print(f"Overall Vulnerability: {clamp(total_vuln)*100:.2f}%")
    print(f"Overall Severity:     {clamp(total_sev)*100:.2f}%")

    def export_attacks_csv(csv_file):
        with open(csv_file, "w", newline="") as f:
            writer = csv.writer(f)

            # Human-readable headers
            writer.writerow([
                "Attack",
                "Vulnerability",
                "Severity",
                "Features",
                "Multipliers"
            ])

            # Find max lengths for names and numbers
            max_feat_name_len = max((len(fname) for a in attacks.values() for fname in a.features), default=0)
            max_mult_name_len = max((len(mname) for a in attacks.values() for mname in a.multipliers), default=0)

            for a in attacks.values():
                # Features: pad names and values separately
                features_str = ", ".join(
                    f"{fname.ljust(max_feat_name_len)}: {clamp(feat['value'])*100:6.2f}%"
                    for fname, feat in a.features.items()
                )

                # Multipliers: pad names and values separately
                multipliers_str = ", ".join(
                    f"{mname.ljust(max_mult_name_len)}: {val*100:6.2f}%"
                    if isinstance(val, (int, float)) else f"{mname.ljust(max_mult_name_len)}: {val}"
                    for mname, val in a.multipliers.items()
                )

                row = [
                    a.name,
                    f"{clamp(a.v / a.vmax) * 100:6.2f}%",
                    f"{clamp(a.severity / a.vmax) * 100:6.2f}%",
                    features_str,
                    multipliers_str
                ]

                writer.writerow(row)

    def export_features_csv(csv_file):
        with open(csv_file, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["feature", "severity_percent"])
            for name, value in sorted(severities.items()):
                writer.writerow([name, value.value * 100])

    def export_combined_csv(csv_file, test_name):
        file_exists = os.path.exists(csv_file)
        test_name_base = test_name.replace(".bin", "")

        PRIV_ORDER = ["excluded", "no", "same space", "user space", "kernel space"]
        PRIV_RANK = {p: i for i, p in enumerate(PRIV_ORDER)}

        with open(csv_file, "a", newline="") as f:
            writer = csv.writer(f)

            if not file_exists:
                writer.writerow([
                    "Test Name",
                    "Mitigated",
                    "test microarchitetturali",
                    "Vunlnerability factor per feature",
                    "Can bypass priviledge level",
                    "Attack family",
                    "Vulnerability",
                    "Breakdown attacchi",
                ])
            else:
                writer.writerow([])

            attack_rows = []
            for a in attacks.values():
                feat_parts = []
                for fname, feat in a.features.items():
                    val_str = f"{clamp(feat['value'])*100:.2f}%"
                    if feat["present"]:
                        feat_parts.append(f"{fname} : {val_str}")
                    else:
                        feat_parts.append(f"{fname} : KO")
                breakdown = ", ".join(feat_parts)

                attack_rows.append([
                    a.name,
                    f"{clamp(a.v / a.vmax) * 100:.2f}%",
                    breakdown,
                ])

            feature_items = list(severities.items())
            max_rows = max(len(feature_items), len(attack_rows))

            all_privs = [compute_priv(e, sev.value) for e, sev in feature_items if e != Test.OVERALL]
            agg_priv = max_priv(all_privs)

            for i in range(max_rows):
                row = ["", "", "", "", "", "", "", ""]

                if i == 0:
                    row[0] = test_name

                if i < len(feature_items):
                    test_enum, sev = feature_items[i]
                    row[1] = feature_is_mitigated(test_name_base, test_enum)
                    row[2] = sev.label
                    row[3] = f"{clamp(sev.value) * 100:.2f}%"
                    row[4] = compute_priv(test_enum, sev.value) if test_enum != Test.OVERALL else agg_priv

                if i < len(attack_rows):
                    name, vuln, breakdown = attack_rows[i]
                    row[5] = name
                    row[6] = vuln
                    row[7] = breakdown

                writer.writerow(row)

    if export:
        export_combined_csv("all_runs.csv", os.path.basename(run_file))
        print("\nAppended results to all_runs.csv")

        # base = run_file
        # export_attacks_csv(f"{base}_attacks.csv")
        # export_features_csv(f"{base}_features.csv")
        # print(f"\nExported CSVs:\n- {base}_attacks.csv\n- {base}_features.csv")




if __name__ == "__main__":
    argparser = argparse.ArgumentParser(description="Plotter for tests")
    argparser.add_argument("run", type=str, help="Run file or directory")
    argparser.add_argument("--plot", help="Name of the module to plot")
    argparser.add_argument("--pp", "--pretty-print", nargs="?", const=True,
                           default=False, help="Pretty-print a module (or all if no name given)")
    argparser.add_argument("--export", nargs="?", const=True,
                           default=False, help="Export to CSV")

    args = argparser.parse_args()

    # Convert --export flag to filename or bool
    if args.export is True and not os.path.isdir(args.run):
        args.export = f"{args.run}.csv"

    # ---------- Directory Mode ----------
    if os.path.isdir(args.run):
        def run_sort_key(name):
            base = name.replace(".bin", "")
            if base == "all": return (0, "")
            if base in TESTS: return (1, base)
            return (2, base)

        run_order = sorted(
            (f for f in os.listdir(args.run) if f.endswith(".bin")),
            key=run_sort_key
        )

        for run_name in run_order:
            run_file = os.path.join(args.run, run_name)
            if os.path.isfile(run_file):
                print(f"\n===== Processing {run_file} =====")
                process_run(
                    run_file,
                    plot=args.plot,
                    pp=args.pp,
                    export=args.export
                )
            else:
                print(f"[WARN] File {run_file} not found, skipping.")

    # ---------- Single File Mode ----------
    else:
        process_run(
            args.run,
            plot=args.plot,
            pp=args.pp,
            export=args.export
        )
