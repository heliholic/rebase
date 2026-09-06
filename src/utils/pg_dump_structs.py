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
        # Comments too: pg_limits.h spells out PG_DECLARE_ARRAY(type, LENGTH,
        # name) in prose, and that is not a declaration of a group called
        # "name".
        text = strip_c_comments(fh.read())
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

    def close(self):
        self.fh.close()

    def find(self, type_name):
        for name in lookup_names(type_name):
            die = self.by_name.get(name)
            if die is not None:
                return die
        return None


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


def pg_layout_hash(type_die, pgn, is_array):
    """FNV-1 over the PGN and the resolved layout of one parameter group.

    The single definition of a PG hash: pg_hash.h and the .def comments both
    come through here, so a .def can be trusted as a record of what the
    firmware will accept back from EEPROM.
    """
    fingerprint = layout_fingerprint(type_die)
    if is_array:
        # Mark the PG as an array but leave the element count out of the
        # hash. The count is target-derived - SPIDEV_COUNT, I2CDEV_COUNT,
        # GYRO_COUNT and friends differ per MCU - and hashing it would make an
        # otherwise identical record unreadable across targets. pgLoad()
        # already reconciles a length change: it copies MIN(stored, pgSize)
        # and leaves the remaining elements at their defaults.
        #
        # The marker itself still matters. Without it a PG that changed
        # between PG_DECLARE and PG_DECLARE_ARRAY of the same element type
        # would keep its hash, and the old record would be reinterpreted
        # rather than rejected.
        fingerprint = "pg_array:%s" % fingerprint
    # The hash alone identifies the record in EEPROM - findEEPROM() matches on
    # it and nothing else - so the PGN has to be in it. Without it, PGs that
    # share a struct type (the three displayPortProfile_t groups,
    # pinPullup/pinPulldown) hash identically and would all load the first
    # record on flash.
    fingerprint = "pgn:%d:%s" % (pgn, fingerprint)
    return fnv1(fingerprint.encode("utf-8"))


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
    # Canonicalise the spelling the compiler happened to pick. uint32_t is
    # "long unsigned int" on arm-none-eabi and "unsigned int" on the host, and
    # plain char is unsigned on ARM but signed on x86. Both describe the same
    # bytes, so neither may move the hash - otherwise SITL and the unit tests
    # disagree with the firmware about groups they lay out identically.
    name = BASE_TYPE_ALIASES.get(name, name)
    if name == "char":
        encoding = "char"
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


PGN_ID_RE = re.compile(
    r"^[ \t]*(PG_[A-Z0-9_]+)[ \t]*=[ \t]*(0[xX][0-9a-fA-F]+|\d+)[ \t]*,?[ \t]*$",
    re.M,
)


def load_pgn_ids(pg_dir):
    """Map PGN macro (PG_GYRO_CONFIG, ...) -> its number, from pg_ids.h.

    Retired ids are left in the file commented out; strip_c_comments drops
    them so they cannot be resolved by accident.
    """
    path = os.path.join(pg_dir, "pg_ids.h")
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = strip_c_comments(fh.read())
    except OSError as exc:
        raise SystemExit("cannot read %s: %s" % (path, exc))
    ids = {name: int(value, 0) for name, value in PGN_ID_RE.findall(text)}
    if not ids:
        raise SystemExit("%s: no PGN enumerators found" % path)
    return ids


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


def resolve_pgn(pg_name, pgn_map, pgn_ids, pg_dir):
    """(PG_<NAME>_HASH define, PGN number) for a PG instance name.

    Both lookups must succeed. The number goes into the hash, and the hash
    is what identifies the record in EEPROM, so a guessed or missing PGN
    would hand the record the wrong identity rather than fail loudly.
    """
    pgn = pgn_map.get(pg_name)
    if pgn is None:
        raise SystemExit(
            "no PG_REGISTER* found for PG %s in %s/*.c; the PGN is part of the "
            "layout hash and cannot be inferred from the name" % (pg_name, pg_dir))
    if pgn not in pgn_ids:
        raise SystemExit(
            "PG %s registers as %s, which is not defined in %s/pg_ids.h"
            % (pg_name, pgn, pg_dir))
    return "%s_HASH" % pgn, pgn_ids[pgn]


def require_complete(missing, what):
    """Refuse to emit a partial result.

    Every group in pg/ is laid out on every target, whether or not the target
    compiles it in: a layout that depends on build flags is a bug, so a hash
    is well defined even where the group is not registered. A group the probe
    could not see means the probe is wrong, not that the group is optional.
    """
    if not missing:
        return
    raise SystemExit(
        "these PGs are declared in pg/ but absent from the probe build, so the "
        "%s would be incomplete:\n%s\n"
        "Add the USE_*/ENABLE_* macro that gates each one to PG_PROBE_GATES in "
        "mk/pg_hash.mk." % (what, "\n".join("  %-28s %-28s pg/%s" % m for m in missing)))


def generate_hash_header(obj_dir, header_dir, out_path, target, obj=None):
    """Write PG_<NAME>_HASH defines.

    Layouts come either from a single object built from a TU that includes
    every pg/*.h (--object, much faster), or from one object per header
    (--obj-dir). Both describe the same translation environment, so they
    produce identical hashes.
    """
    pgn_map = load_pgn_map(header_dir)
    pgn_ids = load_pgn_ids(header_dir)
    hashes = {}
    missing = []

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
                    missing.append((pg_name, type_name, header_name))
                    continue
                define, pgn = resolve_pgn(pg_name, pgn_map, pgn_ids, header_dir)
                value = pg_layout_hash(type_die, pgn, array_len is not None)
                hashes[define] = (value, pg_name, type_name)
        finally:
            if shared is None:
                dwarf.close()

    if shared is not None:
        shared.close()

    # Only the single-object probe sees every header at once; --obj-dir is
    # given whichever objects happen to exist and cannot judge completeness.
    if obj:
        require_complete(missing, "header")

    # The stored record carries the hash and nothing else, so two properties
    # the EEPROM format relies on have to hold before we emit the header.
    # Both are astronomically unlikely; both are silent data loss if missed.
    for define, (value, pg_name, _type) in sorted(hashes.items()):
        if value == 0:
            raise SystemExit(
                "%s hashes to 0, which config_eeprom.c uses as the "
                "end-of-records terminator: every PG stored after %s would be "
                "lost. Perturb the layout of %s to move the hash."
                % (define, pg_name, pg_name))
    collisions = {}
    for define, (value, pg_name, _type) in hashes.items():
        collisions.setdefault(value, []).append((define, pg_name))
    for value, owners in sorted(collisions.items()):
        if len(owners) > 1:
            raise SystemExit(
                "hash collision on 0x%08X between %s; findEEPROM() matches on "
                "the hash alone, so these PGs cannot be told apart in EEPROM"
                % (value, ", ".join("%s (%s)" % o for o in sorted(owners))))

    os.makedirs(os.path.dirname(os.path.abspath(out_path)) or ".", exist_ok=True)
    lines = [
        "/*",
        " * AUTO-GENERATED FILE. DO NOT EDIT.",
        " *",
        " * Parameter group layout hashes for TARGET %s." % (target or "unknown"),
        " * Algorithm: 32-bit FNV-1 (src/main/common/crc.c).",
        " *",
        " * Covers the PGN and the resolved memory layout of the group, so the",
        " * hash both identifies the record in EEPROM and rejects it when the",
        " * layout no longer matches.",
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


def pg_comment(pg_name, type_name, length, type_die, pgn_map, pgn_ids):
    """The `/* PG ... */` banner that introduces a group in a .def file.

    Carries the same hash pg_hash.h defines, so a .def read on its own says
    which EEPROM record the group answers to. A group whose PG_REGISTER is
    not in pg/*.c has no PGN and therefore no hash - `pgn ?` rather than a
    guess, since pg-hash fails loudly on the same group anyway.
    """
    parts = [f"PG {pg_name} : {type_name}"]
    if length:
        parts.append(f"array {length}")
    pgn = pgn_ids.get(pgn_map.get(pg_name))
    if pgn is None:
        parts.append("PGN ?, hash ?")
    else:
        parts.append(f"PGN {pgn}, hash 0x{pg_layout_hash(type_die, pgn, bool(length)):08X}")
    return "/* %s */" % ", ".join(parts)


def dump_def(object_path, header_path, output_path, target, extra_types):
    declares = parse_pg_declares(header_path) if header_path else []

    # Both are text scans of pg/*.c and pg/pg_ids.h, so they resolve every
    # PGN regardless of which single header this object was built from.
    pg_dir = os.path.dirname(os.path.abspath(header_path)) if header_path else None
    pgn_map = load_pgn_map(pg_dir) if pg_dir else {}
    pgn_ids = load_pgn_ids(pg_dir) if pg_dir else {}

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
                blocks.append(pg_comment(pg_name, type_name, length, die,
                                         pgn_map, pgn_ids))
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


# --------------------------------------------------------------------------
# Markdown protocol document
#
# One file describing every parameter group, so the stored-config format can
# be read without a build. It has to be target-invariant: a PG holds the same
# bytes on every board (see mk/pg_hash.mk pg-hash-check), so the document
# names the length macro of a PG array rather than resolving it, and never
# prints the target it was generated from.
# --------------------------------------------------------------------------

MD_PREAMBLE = """\
# Parameter group (PG) format

Rotorflight stores its configuration as a sequence of *parameter groups*. A
group is a plain C struct; the bytes the flight controller writes to EEPROM are
that struct's in-memory image, so the layout below **is** the storage format.

This document is generated from the compiled firmware's debug info by
`src/utils/pg_dump_structs.py`. It covers every group, every struct nested
inside one, and every enum whose values are stored in one.

## How a group is stored

The saved config is a header, a run of records, a terminator, and a checksum:

| Part | Fields |
| --- | --- |
| Header | `uint32_t magic`, `uint32_t version` |
| Record (repeated) | `uint32_t hash`, `uint16_t size`, `uint8_t pg[]` |
| Footer | `uint32_t terminator` (zero) |
| Checksum | CRC-16 over everything above |

Every part is `__attribute__((packed))`: there is no padding *between* the
record header and the group payload, and none between records. Padding *within*
a group is real, is stored, and is listed in the tables below.

## How a group is identified

`hash` is the only identity a record carries. On load, the firmware walks the
records and matches each one against the registered groups by hash alone
(`findEEPROM()` in `src/main/config/config_eeprom.c`); a record whose hash
matches nothing is skipped, and a group with no matching record keeps its
defaults.

The hash is a 32-bit FNV-1 over a canonical fingerprint of the group: its PGN
plus its fully resolved layout - every member's offset, size, name and type,
recursively, including enumerator names and values. So **any** change to a
group's layout changes its hash, and an old record is then ignored rather than
misread. A hash of zero is rejected at build time, because zero is the
terminator.

Two properties are deliberately *not* in the hash:

- **The element count of a `PG_DECLARE_ARRAY` group.** Those counts are
  target-derived (`SPIDEV_COUNT`, `I2CDEV_COUNT`, ...) and differ per MCU.
  `pgLoad()` reconciles a count change by copying `MIN(stored, current)`
  elements and leaving the rest at their defaults. The tables below therefore
  name the length macro instead of a number.
- **The group's `size`.** It is stored in the record and used to bound the
  copy, but the hash already pins the layout.

## Reading the tables

`Offset` and `Size` are bytes from the start of the group (for an array group,
from the start of one element). Padding rows are the bytes the compiler
inserts; they are stored as-is and their contents are undefined. Members of an
anonymous `struct`/`union` are indented under it and share its offsets.
"""


def md_slug(text):
    """GitHub heading anchor for a heading that is a bare type name."""
    out = []
    for ch in text.lower():
        if ch.isalnum() or ch in "-_":
            out.append(ch)
        elif ch == " ":
            out.append("-")
    return "".join(out)


def md_escape(text):
    return text.replace("|", "\\|")


def canonical_type_name(dwarf, core, fallback=None):
    """The one name a type is documented under.

    A group can be declared by its typedef or by its struct tag -
    PG_DECLARE(struct vtxTableConfig_s, vtxTableConfig) does the latter - and
    both spellings have to land on the same section. Prefer the typedef, so
    the heading matches what the rest of the source calls the type.
    """
    tag = die_name(core)
    if tag and tag.endswith("_s"):
        typedef = dwarf.find(tag[:-2] + "_t")
        resolved = unwrap(typedef) if typedef is not None else None
        if resolved is not None and resolved.offset == core.offset:
            return die_name(typedef)
    return tag or fallback


def link_target(die):
    """The record/enum a member's type refers to, looking through arrays.

    Pointers are deliberately not followed: a pointer in a PG stores an
    address, not the thing it points at.
    """
    core = unwrap(die) if die is not None else None
    while core is not None and core.tag == "DW_TAG_array_type":
        core = unwrap(follow_type(core))
    return core


def md_type_ref(type_die, registry):
    """A member's type, linked to its own section when it has one."""
    core = link_target(type_die)
    if core is not None and core.offset in registry:
        name = registry[core.offset][0]
        return "[`%s`](#%s)" % (md_escape(name), md_slug(name))
    return "`%s`" % md_escape(display_type(type_die))


def gather_named_types(dwarf, die, registry):
    """Register every named struct/union/enum reachable from die, by offset."""
    for core, name in collect_nested(die, set()):
        best = canonical_type_name(dwarf, core, name)
        if best:
            registry.setdefault(core.offset, (best, core))


def md_member_rows(core, registry, base=0, depth=0):
    """(offset, size, type markup, name markup, depth) for one record."""
    rows = []
    kind = "union" if core.tag == "DW_TAG_union_type" else "struct"
    prev_end = 0
    for member in members_of(core):
        off = member["offset"]
        size = member["size"] or 0
        if kind != "union" and off > prev_end:
            rows.append((base + prev_end, off - prev_end, "&mdash;", "*(padding)*", depth))

        type_die = member["type_die"]
        member_core = member["core"]
        suffix = ""
        if member_core is not None and member_core.tag == "DW_TAG_array_type":
            suffix = array_suffix(member_core)
        elif type_die is not None and type_die.tag == "DW_TAG_array_type":
            suffix = array_suffix(type_die)

        anonymous = (is_anonymous_record(member_core)
                     and not (type_die is not None
                              and type_die.tag == "DW_TAG_typedef"
                              and die_name(type_die)))
        if anonymous:
            name = member["name"]
            rows.append((base + off, size, "anonymous `%s`" % kind_word(member_core),
                         "`%s`" % name if name else "*(unnamed)*", depth))
            rows.extend(md_member_rows(member_core, registry, base + off, depth + 1))
        else:
            rows.append((base + off, size, md_type_ref(type_die, registry),
                         "`%s%s`" % (member["name"] or "<anonymous>", suffix), depth))

        prev_end = max(prev_end, off + size if kind != "union" else size)

    total = byte_size(core) or 0
    if kind != "union" and total > prev_end:
        rows.append((base + prev_end, total - prev_end, "&mdash;", "*(tail padding)*", depth))
    return rows


def md_sizeof(core):
    size = byte_size(core) or 0
    return "`sizeof` = %d byte%s." % (size, "" if size == 1 else "s")


def md_record_table(core, registry):
    rows = md_member_rows(core, registry)
    if not rows:
        return ["*(no members)*", ""]
    lines = ["| Offset | Size | Type | Member |", "| ---: | ---: | --- | --- |"]
    for off, size, type_text, name_text, depth in rows:
        indent = "&nbsp;&nbsp;&nbsp;&nbsp;" * depth + ("&#8627; " if depth else "")
        lines.append("| %d | %d | %s | %s%s |" % (off, size, type_text, indent, name_text))
    lines.append("")
    return lines


def md_enum_table(core):
    lines = ["| Enumerator | Value |", "| --- | ---: |"]
    for child in core.iter_children():
        if child.tag != "DW_TAG_enumerator":
            continue
        lines.append("| `%s` | %s |" % (die_name(child), die_attr(child, "DW_AT_const_value")))
    lines.append("")
    return lines


def generate_markdown(obj_path, header_dir, out_path):
    """Write the PG protocol document.

    Fails rather than emitting a partial document: a group that is gated out
    of the probe build would silently vanish from a file whose whole purpose
    is to be complete.
    """
    pgn_map = load_pgn_map(header_dir)
    pgn_ids = load_pgn_ids(header_dir)
    dwarf = DwarfTypes(obj_path)
    try:
        groups = []
        missing = []
        registry = {}
        skip = {"pg.h", "pg_ids.h", "pg_hash.h"}
        for header_path in sorted(glob.glob(os.path.join(header_dir, "*.h"))):
            header_name = os.path.basename(header_path)
            if header_name in skip:
                continue
            for type_name, pg_name, array_len in parse_pg_declares(header_path):
                type_die = dwarf.find(type_name)
                if type_die is None:
                    missing.append((pg_name, type_name, header_name))
                    continue
                define, pgn = resolve_pgn(pg_name, pgn_map, pgn_ids, header_dir)
                gather_named_types(dwarf, type_die, registry)
                core = unwrap(type_die)
                groups.append({
                    "pg": pg_name,
                    "type": type_name,
                    "display": canonical_type_name(dwarf, core, display_type(type_die)),
                    "offset": core.offset,
                    "header": header_name,
                    "array": array_len,
                    "pgn": pgn,
                    "define": define,
                    "hash": pg_layout_hash(type_die, pgn, array_len is not None),
                    "die": type_die,
                    "core": core,
                })

        require_complete(missing, "document")

        # Sections are keyed by DIE offset, so a type reached under two
        # spellings is documented once and every reference links to it.
        root_offsets = {g["offset"] for g in groups}
        nested = sorted((name, core) for off, (name, core) in registry.items()
                        if off not in root_offsets
                        and core.tag != "DW_TAG_enumeration_type")
        enums = sorted((name, core) for _off, (name, core) in registry.items()
                       if core.tag == "DW_TAG_enumeration_type")

        # Several PGs can share one struct (the displayPortProfile_t trio), so
        # the layout is documented once per type and the index points at it.
        by_type = {}
        for group in groups:
            by_type.setdefault(group["display"], []).append(group)

        lines = [MD_PREAMBLE, "## Parameter groups", ""]
        lines.append("%d groups, %d nested structures, %d enumerations.\n"
                     % (len(groups), len(nested), len(enums)))
        lines.append("| Group | PGN | Hash | Type | Size | Header |")
        lines.append("| --- | ---: | --- | --- | ---: | --- |")
        for group in sorted(groups, key=lambda g: g["pg"]):
            size = byte_size(group["die"]) or 0
            size_text = ("%d &times; `%s`" % (size, group["array"])
                         if group["array"] else str(size))
            lines.append("| `%s` | %d | `0x%08X` | [`%s`](#%s) | %s | `pg/%s` |" % (
                group["pg"], group["pgn"], group["hash"], md_escape(group["display"]),
                md_slug(group["display"]), size_text, group["header"]))
        lines.append("")

        lines.append("## Group layouts")
        lines.append("")
        for display in sorted(by_type):
            core = unwrap(by_type[display][0]["die"])
            lines.append("### %s" % display)
            lines.append("")
            owners = by_type[display]
            for group in sorted(owners, key=lambda g: g["pg"]):
                array_note = (" &mdash; array of `%s` elements" % group["array"]
                              if group["array"] else "")
                lines.append("- `%s` &mdash; PGN %d (`%s`), hash `0x%08X`%s"
                             % (group["pg"], group["pgn"], group["define"][:-len("_HASH")],
                                group["hash"], array_note))
            lines.append("")
            lines.append(md_sizeof(core))
            lines.append("")
            lines.extend(md_record_table(core, registry))

        if nested:
            lines.append("## Nested structures")
            lines.append("")
            for name, core in nested:
                lines.append("### %s" % name)
                lines.append("")
                lines.append(md_sizeof(core))
                lines.append("")
                lines.extend(md_record_table(core, registry))

        if enums:
            lines.append("## Enumerations")
            lines.append("")
            lines.append("An enum stored in a PG is `PG_ENUM` (packed), so its width is the "
                         "same in every build. Its enumerator names and values are part of "
                         "the layout hash: renumbering one changes what a stored byte means, "
                         "and the hash has to catch that.")
            lines.append("")
            for name, core in enums:
                lines.append("### %s" % name)
                lines.append("")
                lines.append(md_sizeof(core))
                lines.append("")
                lines.extend(md_enum_table(core))

        os.makedirs(os.path.dirname(os.path.abspath(out_path)) or ".", exist_ok=True)
        with open(out_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(lines).rstrip() + "\n")
    finally:
        dwarf.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", help="ELF/object file with DWARF")
    parser.add_argument("--output", help="Destination .def path")
    parser.add_argument("--header", help="pg/*.h to scan for PG_DECLARE types")
    parser.add_argument("--target", default="", help="Firmware TARGET name for the banner")
    parser.add_argument("--hash-header", help="Write PG_<NAME>_HASH defines to this header")
    parser.add_argument("--markdown", help="Write the PG protocol document to this path")
    parser.add_argument("--obj-dir", help="Directory of per-header pg_def *.o files")
    parser.add_argument("--header-dir", help="Directory of pg/*.h and pg/*.c")
    parser.add_argument("types", nargs="*", help="Extra type names to dump")
    args = parser.parse_args()
    if args.markdown:
        if not args.header_dir or not args.object:
            parser.error("--markdown requires --header-dir and --object")
        generate_markdown(args.object, args.header_dir, args.markdown)
        return
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
