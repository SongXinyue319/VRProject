import argparse
import struct
from pathlib import Path


def read_header(f):
    lines = []
    while True:
        line = f.readline()
        if not line:
            raise ValueError("PLY header ended unexpectedly")
        text = line.decode("ascii").strip()
        lines.append(text)
        if text == "end_header":
            return lines


def parse_header(lines):
    fmt = None
    vertex_count = 0
    face_count = 0
    vertex_props = []
    face_props = []
    current = None

    for line in lines:
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "format":
            fmt = parts[1]
        elif parts[0] == "element":
            current = parts[1]
            if current == "vertex":
                vertex_count = int(parts[2])
            elif current == "face":
                face_count = int(parts[2])
        elif parts[0] == "property":
            if current == "vertex":
                vertex_props.append(parts)
            elif current == "face":
                face_props.append(parts)

    return fmt, vertex_count, face_count, vertex_props, face_props


def scalar_format(fmt, typ):
    endian = ">" if fmt == "binary_big_endian" else "<"
    mapping = {
        "char": "b",
        "uchar": "B",
        "short": "h",
        "ushort": "H",
        "int": "i",
        "uint": "I",
        "float": "f",
        "double": "d",
    }
    return endian + mapping[typ]


def read_binary_scalar(f, fmt, typ):
    spec = scalar_format(fmt, typ)
    return struct.unpack(spec, f.read(struct.calcsize(spec)))[0]


def fit_vertices(vertices, fit_height, y_min):
    if fit_height is None:
        return vertices

    mins = [min(v[i] for v in vertices) for i in range(3)]
    maxs = [max(v[i] for v in vertices) for i in range(3)]
    height = maxs[1] - mins[1]
    if height == 0:
        raise ValueError("Cannot fit a model with zero height")

    scale = fit_height / height
    center_x = (mins[0] + maxs[0]) * 0.5
    center_z = (mins[2] + maxs[2]) * 0.5
    return [
        ((x - center_x) * scale, (y - mins[1]) * scale + y_min, (z - center_z) * scale)
        for x, y, z in vertices
    ]


def write_obj(out, vertices, faces):
    with out.open("w", encoding="ascii", newline="\n") as obj:
        obj.write(f"o {out.stem}\n")
        for x, y, z in vertices:
            obj.write(f"v {x:.9g} {y:.9g} {z:.9g}\n")
        for indexes in faces:
            for i in range(1, len(indexes) - 1):
                obj.write(f"f {indexes[0]} {indexes[i]} {indexes[i + 1]}\n")


def convert_ascii(f, out, vertex_count, face_count, vertex_props, fit_height, y_min):
    xyz_indexes = [
        next(i for i, prop in enumerate(vertex_props) if prop[-1] == axis)
        for axis in ("x", "y", "z")
    ]

    vertices = []
    for _ in range(vertex_count):
        parts = f.readline().decode("ascii").split()
        vertices.append(tuple(float(parts[i]) for i in xyz_indexes))

    faces = []
    for _ in range(face_count):
        parts = f.readline().decode("ascii").split()
        n = int(parts[0])
        faces.append([int(x) + 1 for x in parts[1 : 1 + n]])

    write_obj(out, fit_vertices(vertices, fit_height, y_min), faces)


def convert_binary(f, fmt, out, vertex_count, face_count, vertex_props, face_props, fit_height, y_min):
    vertices = []
    for _ in range(vertex_count):
        values = {}
        for prop in vertex_props:
            values[prop[-1]] = read_binary_scalar(f, fmt, prop[1])
        vertices.append((values["x"], values["y"], values["z"]))

    faces = []
    for _ in range(face_count):
        indexes = None
        for prop in face_props:
            if prop[1] == "list":
                count_type, value_type, name = prop[2], prop[3], prop[4]
                count = read_binary_scalar(f, fmt, count_type)
                values = [read_binary_scalar(f, fmt, value_type) for _ in range(count)]
                if name == "vertex_indices":
                    indexes = [value + 1 for value in values]
            else:
                read_binary_scalar(f, fmt, prop[1])
        if indexes:
            faces.append(indexes)

    write_obj(out, fit_vertices(vertices, fit_height, y_min), faces)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("dest", type=Path)
    parser.add_argument("--fit-height", type=float)
    parser.add_argument("--y-min", type=float, default=0.03)
    args = parser.parse_args()

    args.dest.parent.mkdir(parents=True, exist_ok=True)
    with args.source.open("rb") as f:
        header = read_header(f)
        fmt, vertex_count, face_count, vertex_props, face_props = parse_header(header)
        if fmt == "ascii":
            convert_ascii(f, args.dest, vertex_count, face_count, vertex_props, args.fit_height, args.y_min)
        elif fmt in {"binary_little_endian", "binary_big_endian"}:
            convert_binary(
                f,
                fmt,
                args.dest,
                vertex_count,
                face_count,
                vertex_props,
                face_props,
                args.fit_height,
                args.y_min,
            )
        else:
            raise ValueError(f"Unsupported PLY format: {fmt}")

    print(f"{args.source} -> {args.dest} ({vertex_count} vertices, {face_count} faces)")


if __name__ == "__main__":
    main()
