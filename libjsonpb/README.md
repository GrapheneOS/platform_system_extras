# `libjsonpbparse`

This library provides functions to parse a JSON file to a structured Protobuf
message.

At this time of writing, `libprotobuf-cpp-full` is at version 3.0.0-beta, and
unknown fields in a JSON file cannot be ignored. Do **NOT** use this library in
vendor / recovery until `libprotobuf-cpp-full` is updated.

## Using `libjsoncpp` in parser code

Since `libjsonpbparse` cannot be used in vendor / recovery processes yet,
`libjsoncpp` is used instead. However, there are notable differences in the
logic of `libjsoncpp` and `libprotobuf` when parsing JSON files.

- There are no implicit string to integer conversion in `libjsoncpp`. Hence:
  - If the Protobuf schema uses 64-bit integers (`(s|fixed|u|)int64`):
    - The JSON file must use strings (to pass tests in `libjsonpbverify`)
    - Parser code (that uses `libjsoncpp`) must explicitly convert strings to
      integers. Example:
      ```c++
      strtoull(value.asString(), 0, 10)
      ```
  - If the Protobuf schema uses special floating point values:
    - The JSON file must use strings (e.g. `"NaN"`, `"Infinity"`, `"-Infinity"`)
    - Parser code must explicitly handle these cases. Example:
      ```c++
      double d;
      if (value.isNumeric()) {
        d = value.asDouble();
      } else {
        auto&& s = value.asString();
        if (s == "NaN") d = std::numeric_limits<double>::quiet_NaN();
        else if (s == "Infinity") d = std::numeric_limits<double>::infinity();
        else if (s == "-Infinity") d = -std::numeric_limits<double>::infinity();
      }
      ```
- `libprotobuf` accepts either `lowerCamelCase` (or `json_name` option if it is
  defined) or the original field name as keys in the input JSON file.
  The test in `libjsonpbverify` explicitly check this case to avoid ambiguity;
  only the original field name (or `json_name` option if it is defined) can be
  used.

Once `libprotobuf` in the source tree is updated to a higher version and
`libjsonpbparse` is updated to ignore unknown fields in JSON files, all parsing
code must be converted to use `libjsonpbparse` for consistency.
