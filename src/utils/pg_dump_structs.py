#!/usr/bin/env python3
"""Extract PG struct layouts from DWARF debug info into .def files."""

from __future__ import annotations

import argparse
import glob
import os
import re
import sys

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    sys.stderr.write("pg_dump_structs.py requires pyelftools (python3-pyelftools)\n")
    sys.exit(1)


PG_DECLARE_RE = re.compile(
    r"PG_DECLARE(_ARRAY)?\s*\(\s*((?:struct|union)\s+\w+|\w+)\s*,\s*(?:(\w+)\s*,\s*)?(\w+)\s*\)"
)

BASE_TYPE_ALIASES = {
    "unsigned char": "uint8_t",
    "signed char": "int8_t",
    "short unsigned int": "uint16_t",
    "short int": "int16_t",
    "unsigned int": "uint32_t",
    "int": "int32_t",
    "long unsigned int": "uint32_t",
    "long int": "int32_t",
    "long long unsigned int": "uint64_t",
    "long long int": "int64_t",
    "_Bool": "bool",
}

UNWRAP_TAGS = (
    "DW_TAG_typedef",
    "DW_TAG_const_type",
    "DW_TAG_volatile_type",
    "DW_TAG_restrict_type",
    "DW_TAG_atomic_type",
)


def die_name(die):
    attr = die.attributes.get("DW_AT_name")
    if not attr:
        return None
    value = attr.value
    if isinstance(value, bytes):
        return value.decode("utf-8", "replace")
    return str(value)


def die_attr(die, name, default=None):
    attr = die.attributes.get(name)
    return attr.value if attr else default


def follow_type(die):
    if "DW_AT_type" not in die.attributes:
        return None
    return die.get_DIE_from_attribute("DW_AT_type")


def unwrap(die):
    seen = set()
    while die is not None and die.offset not in seen:
        seen.add(die.offset)
        if die.tag in UNWRAP_TAGS:
            nxt = follow_type(die)
            if nxt is None:
                return die
            die = nxt
            continue
        return die
    return die


def byte_size(die):
    if die is None:
        return None
    size = die_attr(die, "DW_AT_byte_size")
    if size is not None:
        return size
    if die.tag == "DW_TAG_array_type":
        elem = follow_type(die)
        elem_size = byte_size(elem)
        count = 1
        for bound in array_bounds(die):
            if bound is None:
                return None
            count *= bound
        if elem_size is None:
            return None
        return elem_size * count
    if die.tag in UNWRAP_TAGS:
        inner = follow_type(die)
        if inner is not None and inner.offset != die.offset:
            return byte_size(inner)
    return None


def member_offset(die):
    loc = die.attributes.get("DW_AT_data_member_location")
    if not loc:
        return 0
    if isinstance(loc.value, int):
        return loc.value
    value = loc.value
    if isinstance(value, list) and value and value[0] == 0x23:
        n = 0
        shift = 0
        for b in value[1:]:
            n |= (b & 0x7F) << shift
            if (b & 0x80) == 0:
                break
            shift += 7
        return n
    return 0


def array_bounds(die):
    bounds = []
    for child in die.iter_children():
        if child.tag != "DW_TAG_subrange_type":
            continue
        count = die_attr(child, "DW_AT_count")
        if count is not None:
            bounds.append(count)
            continue
        upper = die_attr(child, "DW_AT_upper_bound")
        lower = die_attr(child, "DW_AT_lower_bound", 0)
        if upper is not None:
            bounds.append(upper - lower + 1)
        else:
            bounds.append(None)
    return bounds


def array_suffix(die):
    parts = []
    cur = die
    while cur is not None and cur.tag == "DW_TAG_array_type":
        bounds = array_bounds(cur)
        if bounds:
            for bound in bounds:
                parts.append("[]" if bound is None else f"[{bound}]")
        else:
            parts.append("[]")
        cur = unwrap(follow_type(cur)) if follow_type(cur) else None
        if cur is not None and cur.tag != "DW_TAG_array_type":
            break
    return "".join(parts)


def display_type(die):
    """Type name as it should appear in a member declaration, without array bounds."""
    if die is None:
        return "?"
    if die.tag == "DW_TAG_typedef":
        name = die_name(die)
        if name:
            return name
        return display_type(follow_type(die))
    if die.tag == "DW_TAG_pointer_type":
        return display_type(follow_type(die)) + " *"
    if die.tag == "DW_TAG_array_type":
        return display_type(follow_type(die))
    if die.tag in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
        prefix = "const " if die.tag == "DW_TAG_const_type" else "volatile "
        return prefix + display_type(follow_type(die))
    if die.tag == "DW_TAG_base_type":
        name = die_name(die) or "base"
        return BASE_TYPE_ALIASES.get(name, name)
    if die.tag == "DW_TAG_enumeration_type":
        return die_name(die) or "enum"
    if die.tag == "DW_TAG_structure_type":
        name = die_name(die)
        return f"struct {name}" if name else "struct"
    if die.tag == "DW_TAG_union_type":
        name = die_name(die)
        return f"union {name}" if name else "union"
    name = die_name(die)
    return name or (die.tag or "?")


def parse_pg_declares(header_path):
    with open(header_path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    declares = []
    for match in PG_DECLARE_RE.finditer(text):
        is_array = match.group(1) == "_ARRAY"
        type_name = match.group(2).strip()
        if type_name.startswith("_"):
            continue
        length = match.group(3).strip() if is_array and match.group(3) else None
        pg_name = match.group(4)
        declares.append((type_name, pg_name, length))
    return declares


def lookup_names(type_name):
    names = [type_name]
    if type_name.startswith("struct "):
        names.append(type_name[len("struct "):])
    elif type_name.startswith("union "):
        names.append(type_name[len("union "):])
    return names


class DwarfTypes:
    def __init__(self, path):
        self.fh = open(path, "rb")
        self.elf = ELFFile(self.fh)
        if not self.elf.has_dwarf_info():
            raise SystemExit(f"{path}: no DWARF debug info")
        self.dwarf = self.elf.get_dwarf_info()
        self.by_name = {}
        self.variables = {}
        for cu in self.dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                name = die_name(die)
                if not name:
                    continue
                if die.tag in (
                    "DW_TAG_typedef",
                    "DW_TAG_structure_type",
                    "DW_TAG_union_type",
                    "DW_TAG_enumeration_type",
                ):
                    self.by_name.setdefault(name, die)
                elif die.tag == "DW_TAG_variable":
                    self.variables.setdefault(name, die)

    def close(self):
        self.fh.close()

    def find(self, type_name):
        for name in lookup_names(type_name):
            die = self.by_name.get(name)
            if die is not None:
                return die
        return None

    def array_length(self, pg_name):
        """Resolved element count of a PG_DECLARE_ARRAY, from the
        <name>_SystemArray variable the macro emits.

        The declaration spells the length as a macro (PID_PROFILE_COUNT),
        so the source text says nothing about its value. The compiler has
        already resolved it here, which is what actually determines the
        size of the stored record.
        """
        die = self.variables.get(pg_name + "_SystemArray")
        if die is None:
            return None
        array = unwrap(follow_type(die))
        if array is None or array.tag != "DW_TAG_array_type":
            return None
        bounds = array_bounds(array)
        if len(bounds) != 1 or bounds[0] is None:
            return None
        return bounds[0]


def collect_nested(die, seen):
    """Yield (core_die, display_name) for nested struct/union/enum types."""
    if die is None:
        return
    typedef_name = die_name(die) if die.tag == "DW_TAG_typedef" else None
    core = unwrap(die)
    if core is None:
        return
    if core.tag in ("DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_enumeration_type"):
        if core.offset in seen:
            return
        seen.add(core.offset)
        yield core, typedef_name or die_name(core)
        if core.tag == "DW_TAG_enumeration_type":
            return
        for child in core.iter_children():
            if child.tag != "DW_TAG_member":
                continue
            yield from collect_nested(follow_type(child), seen)
        return
    if core.tag == "DW_TAG_array_type":
        yield from collect_nested(follow_type(core), seen)


def format_bitfield(die):
    bit_size = die_attr(die, "DW_AT_bit_size")
    if bit_size is None:
        return ""
    return f" : {bit_size}"


def members_of(die):
    members = []
    for child in die.iter_children():
        if child.tag != "DW_TAG_member":
            continue
        type_die = follow_type(child)
        core = unwrap(type_die) if type_die else None
        size = byte_size(core) if core is not None else None
        if size is None and type_die is not None:
            size = byte_size(type_die)
        members.append({
            "name": die_name(child) or "",
            "offset": member_offset(child),
            "size": size,
            "type_die": type_die,
            "core": core,
            "bitfield": format_bitfield(child),
            "bit_size": die_attr(child, "DW_AT_bit_size"),
            "bit_offset": die_attr(child, "DW_AT_bit_offset"),
            "data_bit_offset": die_attr(child, "DW_AT_data_bit_offset"),
        })
    members.sort(key=lambda m: m["offset"])
    return members


def is_anonymous_record(die):
    if die is None:
        return False
    return die.tag in ("DW_TAG_structure_type", "DW_TAG_union_type") and not die_name(die)


def format_record(die, typedef_name=None, indent=0):
    pad = "    " * indent
    core = unwrap(die)
    if core is None:
        return f"{pad}/* unresolved type */\n"

    if core.tag == "DW_TAG_enumeration_type":
        return format_enum(core, typedef_name, indent)

    kind = "union" if core.tag == "DW_TAG_union_type" else "struct"
    tag = die_name(core)
    size = byte_size(core) or 0
    members = members_of(core)

    if typedef_name:
        if tag and tag != typedef_name:
            header = f"{pad}typedef {kind} {tag} {{"
        else:
            header = f"{pad}typedef {kind} {{"
        trailer = f"{pad}}} {typedef_name};"
    elif tag:
        header = f"{pad}{kind} {tag} {{"
        trailer = f"{pad}}};"
    else:
        header = f"{pad}{kind} {{"
        trailer = f"{pad}}};"

    # Build (declaration, offset, size) triples first, then emit: the layout
    # comments are aligned to a column derived from the widest declaration in
    # this record, so they line up regardless of member name length.
    entries = []
    prev_end = 0
    padding = 0
    type_width = 0
    rendered = []

    for member in members:
        type_die = member["type_die"]
        core_m = member["core"]
        suffix = ""
        if core_m is not None and core_m.tag == "DW_TAG_array_type":
            suffix = array_suffix(core_m)
        elif type_die is not None and type_die.tag == "DW_TAG_array_type":
            suffix = array_suffix(type_die)

        if (is_anonymous_record(core_m)
                and not (type_die is not None
                         and type_die.tag == "DW_TAG_typedef"
                         and die_name(type_die))):
            type_text = kind_word(core_m)
            body = format_record(core_m, None, indent + 1)
            rendered.append((member, type_text, suffix, body))
            continue

        type_text = display_type(type_die)
        type_width = max(type_width, len(type_text))
        rendered.append((member, type_text, suffix, None))

    type_width = min(max(type_width, 8), 32)

    for member, type_text, suffix, body in rendered:
        off = member["offset"]
        size_m = member["size"] or 0
        name = member["name"]
        if kind != "union" and off > prev_end:
            gap = off - prev_end
            padding += gap
            entries.append((f"{pad}    /* padding {gap} byte{'s' if gap != 1 else ''} */",
                            prev_end, gap))

        if body is not None:
            label = name if name else ""
            # A nested record spans several lines; only its closing line
            # carries a layout comment, so the inner lines pass through with
            # their own alignment already applied.
            entries.append((f"{pad}    {type_text} {{", None, None))
            for raw in body.splitlines()[1:-1]:
                entries.append((raw, None, None))
            extra = f" {label}{suffix}{member['bitfield']}" if (label or suffix) else member["bitfield"]
            entries.append((f"{pad}    }}{extra};", off, size_m))
        else:
            decl_name = name if name else "<anonymous>"
            decl = f"{type_text:{type_width}} {decl_name}{suffix}{member['bitfield']};"
            entries.append((f"{pad}    {decl}", off, size_m))

        if kind != "union":
            prev_end = max(prev_end, off + size_m)
        else:
            prev_end = max(prev_end, size_m)

    if size > prev_end:
        gap = size - prev_end
        padding += gap
        entries.append((f"{pad}    /* tail padding {gap} byte{'s' if gap != 1 else ''} */",
                        prev_end, gap))

    # Align the layout comments one space past the widest commented
    # declaration. Lines without a comment (nested record headers, and the
    # inner lines of a nested record) do not push the column out.
    comment_col = max((len(text) for text, off, _ in entries if off is not None),
                      default=0) + 2
    off_width = max((len(str(off)) for _, off, _ in entries if off is not None),
                    default=1)
    size_width = max((len(str(sz)) for _, _, sz in entries if sz is not None),
                     default=1)

    lines = [f"{header}  /* sizeof={size} padding={padding} */"]
    for text, off, size_m in entries:
        if off is None:
            lines.append(text)
        else:
            size_text = f"+{size_m}"
            lines.append(f"{text:{comment_col}}"
                         f"/* {off:{off_width}d} {size_text:>{size_width + 1}} */")
    lines.append(trailer)
    return "\n".join(lines) + "\n"


FNV_OFFSET_BASIS = 2166136261
FNV_PRIME = 16777619

REGISTER_RE = re.compile(
    r"PG_REGISTER(_ARRAY)?(?:_WITH_RESET_(?:FN|TEMPLATE))?\s*\(\s*(.*?)\s*\)\s*;",
    re.S,
)


def fnv1(data):
    """32-bit FNV-1, matching src/main/common/crc.c."""
    hash_value = FNV_OFFSET_BASIS
    for byte in data:
        hash_value = (hash_value * FNV_PRIME) & 0xFFFFFFFF
        hash_value ^= byte
    return hash_value


def type_encoding(die):
    core = unwrap(die) if die is not None else None
    if core is None:
        return None
    return die_attr(core, "DW_AT_encoding")


def enumerators_of(die):
    items = []
    for child in die.iter_children():
        if child.tag != "DW_TAG_enumerator":
            continue
        items.append("%s=%s" % (die_name(child) or "", die_attr(child, "DW_AT_const_value")))
    items.sort()
    return items


def layout_fingerprint(die, seen=None):
    """Canonical ABI fingerprint of a DWARF type."""
    if die is None:
        return "void"
    if seen is None:
        seen = set()

    core = unwrap(die)
    if core is None:
        return "void"

    size = byte_size(die)
    if size is None:
        size = byte_size(core)
    size_text = str(size) if size is not None else "?"

    if core.tag == "DW_TAG_pointer_type":
        return "ptr:%s" % size_text

    if core.tag == "DW_TAG_subroutine_type":
        return "fnptr:%s" % size_text

    if core.tag == "DW_TAG_enumeration_type":
        encoding = type_encoding(core)
        if encoding is None:
            encoding = type_encoding(follow_type(core))
        return "enum:%s:%s:%s" % (size_text, encoding if encoding is not None else "?", ",".join(enumerators_of(core)))

    if core.tag == "DW_TAG_array_type":
        bounds = array_bounds(core)
        bound_text = "x".join("?" if bound is None else str(bound) for bound in bounds) if bounds else "?"
        return "arr[%s]:%s" % (bound_text, layout_fingerprint(follow_type(core), seen))

    if core.tag in ("DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_class_type"):
        if core.offset in seen:
            return "rec:%s:%s" % (die_name(core) or hex(core.offset), size_text)
        seen.add(core.offset)
        kind = "union" if core.tag == "DW_TAG_union_type" else "struct"
        parts = []
        for member in members_of(core):
            extra = ""
            if member["bit_size"] is not None:
                extra = ":b%s/%s/%s" % (
                    member["data_bit_offset"] if member["data_bit_offset"] is not None else "?",
                    member["bit_offset"] if member["bit_offset"] is not None else "?",
                    member["bit_size"],
                )
            parts.append("%s:%s:%s:%s%s" % (
                member["offset"],
                member["size"] if member["size"] is not None else "?",
                member["name"],
                layout_fingerprint(member["type_die"], seen),
                extra,
            ))
        return "%s:%s:%s" % (kind, size_text, ",".join(parts))

    encoding = type_encoding(core)
    name = die_name(core) or ""
    return "base:%s:%s:%s" % (size_text, encoding if encoding is not None else "?", name)


def strip_c_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*?$", "", text, flags=re.M)
    return text


def split_macro_args(text):
    args = []
    depth = 0
    current = []
    for char in text:
        if char == "(":
            depth += 1
            current.append(char)
        elif char == ")":
            depth -= 1
            current.append(char)
        elif char == "," and depth == 0:
            args.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    if current:
        args.append("".join(current).strip())
    return args


def camel_to_pgn(name):
    stepped = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    stepped = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", stepped)
    return "PG_%s" % stepped.upper()


def load_pgn_map(pg_dir):
    """Map PG instance name -> PGN macro (PG_GYRO_CONFIG, ...)."""
    mapping = {}
    for path in sorted(glob.glob(os.path.join(pg_dir, "*.c"))):
        text = strip_c_comments(open(path, encoding="utf-8", errors="replace").read())
        for array_flag, arg_text in REGISTER_RE.findall(text):
            args = split_macro_args(arg_text)
            if array_flag:
                if len(args) < 4:
                    continue
                inst_name, pgn = args[2], args[3]
            else:
                if len(args) < 3:
                    continue
                inst_name, pgn = args[1], args[2]
            if inst_name and pgn:
                mapping[inst_name] = pgn
    return mapping


def hash_define_name(pg_name, pgn_map):
    pgn = pgn_map.get(pg_name)
    if pgn:
        return "%s_HASH" % pgn
    return "%s_HASH" % camel_to_pgn(pg_name)


def generate_hash_header(obj_dir, header_dir, out_path, target, obj=None):
    """Write PG_<NAME>_HASH defines.

    Layouts come either from a single object built from a TU that includes
    every pg/*.h (--object, much faster), or from one object per header
    (--obj-dir). Both describe the same translation environment, so they
    produce identical hashes.
    """
    pgn_map = load_pgn_map(header_dir)
    hashes = {}

    shared = DwarfTypes(obj) if obj else None

    skip = {"pg.h", "pg_ids.h", "pg_hash.h"}
    for header_path in sorted(glob.glob(os.path.join(header_dir, "*.h"))):
        header_name = os.path.basename(header_path)
        if header_name in skip:
            continue
        declares = parse_pg_declares(header_path)
        if not declares:
            continue

        if shared is not None:
            obj_path = obj
            dwarf = shared
        else:
            obj_path = os.path.join(obj_dir, os.path.splitext(header_name)[0] + ".o")
            if not os.path.isfile(obj_path):
                continue
            dwarf = DwarfTypes(obj_path)
        try:
            for type_name, pg_name, array_len in declares:
                type_die = dwarf.find(type_name)
                if type_die is None:
                    continue
                fingerprint = layout_fingerprint(type_die)
                if array_len is not None:
                    # Hash the resolved element count, not the macro spelling:
                    # changing PID_PROFILE_COUNT resizes the stored record, and
                    # renaming the macro does not.
                    count = dwarf.array_length(pg_name)
                    if count is None:
                        raise SystemExit(
                            "%s: cannot resolve array length for PG %s (%s); "
                            "expected a %s_SystemArray symbol in %s"
                            % (header_path, pg_name, array_len, pg_name, obj_path)
                        )
                    fingerprint = "pg_array[%d]:%s" % (count, fingerprint)
                define = hash_define_name(pg_name, pgn_map)
                hashes[define] = (fnv1(fingerprint.encode("utf-8")), pg_name, type_name)
        finally:
            if shared is None:
                dwarf.close()

    if shared is not None:
        shared.close()

    os.makedirs(os.path.dirname(os.path.abspath(out_path)) or ".", exist_ok=True)
    lines = [
        "/*",
        " * AUTO-GENERATED FILE. DO NOT EDIT.",
        " *",
        " * Parameter group layout hashes for TARGET %s." % (target or "unknown"),
        " * Algorithm: 32-bit FNV-1 (src/main/common/crc.c).",
        " */",
        "",
        "#pragma once",
        "",
    ]
    for define in sorted(hashes):
        value, pg_name, type_name = hashes[define]
        lines.append("#define %-40s 0x%08Xu /* %s : %s */" % (define, value, pg_name, type_name))
    lines.append("")
    with open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))


def kind_word(die):
    if die.tag == "DW_TAG_union_type":
        return "union"
    return "struct"


def format_enum(die, typedef_name=None, indent=0):
    pad = "    " * indent
    name = typedef_name or die_name(die) or ""
    size = byte_size(die) or 0
    header = f"{pad}enum {name} {{" if name else f"{pad}enum {{"
    lines = [f"{header}  /* sizeof={size} */"]
    for child in die.iter_children():
        if child.tag != "DW_TAG_enumerator":
            continue
        value = die_attr(child, "DW_AT_const_value")
        lines.append(f"{pad}    {die_name(child)} = {value},")
    close = f"{pad}}};" if not typedef_name else f"{pad}}} {typedef_name};"
    # enums are tagged types, not typically `typedef enum { } name` in DWARF when named
    if typedef_name and typedef_name == die_name(die):
        close = f"{pad}}};"
    elif typedef_name and die_name(die) and typedef_name != die_name(die):
        lines[0] = f"{pad}typedef enum {die_name(die)} {{  /* sizeof={size} */"
        close = f"{pad}}} {typedef_name};"
    elif typedef_name and not die_name(die):
        lines[0] = f"{pad}typedef enum {{  /* sizeof={size} */"
        close = f"{pad}}} {typedef_name};"
    lines.append(close)
    return "\n".join(lines) + "\n"


def typedef_name_for(die):
    if die.tag == "DW_TAG_typedef":
        return die_name(die)
    return None


def dump_def(object_path, header_path, output_path, target, extra_types):
    declares = parse_pg_declares(header_path) if header_path else []

    dwarf = DwarfTypes(object_path)
    try:
        found = []
        missing = []
        dumped_offsets = set()
        dumped_type_names = set()
        blocks = []

        grouped = []
        seen_types = {}
        for type_name, pg_name, length in declares:
            if type_name not in seen_types:
                seen_types[type_name] = len(grouped)
                grouped.append((type_name, []))
            grouped[seen_types[type_name]][1].append((pg_name, length))

        for type_name, pgs in grouped:
            die = dwarf.find(type_name)
            if die is None:
                for pg_name, _length in pgs:
                    missing.append((type_name, pg_name))
                continue
            found.append(type_name)
            for pg_name, length in pgs:
                array_note = f", array {length}" if length else ""
                blocks.append(f"/* PG {pg_name} : {type_name}{array_note} */")
            if type_name in dumped_type_names:
                continue
            dumped_type_names.add(type_name)
            typedef_name = typedef_name_for(die)
            if not typedef_name and type_name.startswith(("struct ", "union ")):
                tag = type_name.split(None, 1)[1]
                if tag.endswith("_s"):
                    guessed = dwarf.find(tag[:-2] + "_t")
                    if guessed is not None:
                        typedef_name = die_name(guessed)
            if not typedef_name and not type_name.startswith(("struct ", "union ")):
                typedef_name = type_name
            blocks.append(format_record(die, typedef_name))
            dumped_offsets.add(unwrap(die).offset)

            for nested_die, nested_name in collect_nested(die, set()):
                if nested_die.offset == unwrap(die).offset:
                    continue
                if nested_die.offset in dumped_offsets:
                    continue
                if not nested_name:
                    continue
                dumped_offsets.add(nested_die.offset)
                blocks.append(format_record(nested_die, nested_name))

        for type_name in extra_types:
            if type_name in found:
                continue
            die = dwarf.find(type_name)
            if die is None:
                missing.append((type_name, None))
                continue
            found.append(type_name)
            blocks.append(format_record(die, typedef_name_for(die) or type_name))

        header_rel = os.path.relpath(header_path) if header_path else ""
        lines = [
            "/*",
            " * Auto-generated PG layout. Do not edit.",
            f" * Target  : {target or '?'}",
            f" * Header  : {header_rel}",
            " * Generator: src/utils/pg_dump_structs.py",
            " */",
            "",
        ]
        if missing:
            lines.append("/* Types not present in this build: */")
            for type_name, pg_name in missing:
                extra = f" ({pg_name})" if pg_name else ""
                lines.append(f"/*   {type_name}{extra} */")
            lines.append("")
        if blocks:
            lines.append("\n".join(blocks).rstrip() + "\n")
        elif not missing:
            lines.append("/* No PG_DECLARE types in header. */\n")

        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines).replace("\n\n\n", "\n\n"))
            if not lines[-1].endswith("\n"):
                fh.write("\n")

        if declares and not found:
            sys.stderr.write(
                f"{os.path.basename(output_path)}: no PG types compiled in for TARGET={target or '?'}\n"
            )
            return 0
        return 0
    finally:
        dwarf.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", help="ELF/object file with DWARF")
    parser.add_argument("--output", help="Destination .def path")
    parser.add_argument("--header", help="pg/*.h to scan for PG_DECLARE types")
    parser.add_argument("--target", default="", help="Firmware TARGET name for the banner")
    parser.add_argument("--hash-header", help="Write PG_<NAME>_HASH defines to this header")
    parser.add_argument("--obj-dir", help="Directory of per-header pg_def *.o files")
    parser.add_argument("--header-dir", help="Directory of pg/*.h and pg/*.c")
    parser.add_argument("types", nargs="*", help="Extra type names to dump")
    args = parser.parse_args()
    if args.hash_header:
        if not args.header_dir or not (args.obj_dir or args.object):
            parser.error("--hash-header requires --header-dir and one of --object/--obj-dir")
        generate_hash_header(args.obj_dir, args.header_dir, args.hash_header,
                             args.target, obj=args.object)
        return
    if not args.object or not args.output:
        parser.error("--object and --output are required unless --hash-header is set")
    sys.exit(dump_def(args.object, args.header, args.output, args.target, args.types))


if __name__ == "__main__":
    main()
