# COOP

Feature-complete OOP runtime for modern C23/C26-style development.

## Highlights

- public/protected/private fields and methods
- static fields and static methods
- abstraction (non-instantiable abstract types)
- RTTI (`is-a`, type names, parent offsets)
- polymorphic method dispatch
- single and multiple inheritance
- explicit parent-qualified calls for easy parent interaction
- flat contiguous object memory with explicit parent offsets

## API style

COOP supports direct function calls and a function-table facade:

- classic C API: `coop_type_register(...)`
- object-like API: `coop->type_register(...)`

Use `coop->...` when you want namespaced, encapsulated calls through struct function pointers.

## Build and test (CMake + CTest + CMocka)

Install CMake and CMocka, then run:

```bash
cd /home/runner/work/COOP/COOP
cmake -S . -B /tmp/coop-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/coop-build
ctest --test-dir /tmp/coop-build --output-on-failure
```

## Quick start

1. Define C structs with `CoopObject` at offset 0 for each subobject.
2. Describe fields, methods, parents, and lifecycle callbacks in `CoopTypeDef`.
3. Register types with `coop->type_register`.
4. Create instances with `coop->object_new` and invoke methods with `coop->invoke_*`.
5. Use `coop->invoke_parent` for explicit parent behavior.
6. Validate relationships with `coop->type_is_a`, `coop->object_is_a`, and `coop->object_as`.

See `/home/runner/work/COOP/COOP/tests/test_coop.c` for end-to-end examples.
