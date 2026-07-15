# COOP

Feature-complete OOP runtime for modern C (C23/C26-oriented), with:

- public/protected/private fields and methods
- static fields and static methods
- abstraction support (non-instantiable abstract types)
- RTTI (`is-a`, type names, parent offsets)
- polymorphic method dispatch
- single and multiple inheritance
- explicit parent-qualified calls for easy parent interaction
- flat contiguous object memory with explicit parent offsets

## Build and test

```bash
cd /home/runner/work/COOP/COOP
make test
```

## Core model

- Every object layout starts with `CoopObject` (type pointer header).
- Type metadata is registered with `coop_type_register` using `CoopTypeDef`.
- Parent relationships are explicit (`CoopParentLink { type, offset }`) and keep layout flat.
- Method/field lookup order is deterministic: local type first, then parents left-to-right depth-first.

## Encapsulation model in C

C cannot hard-block access like C++ private/protected at language level, so COOP enforces encapsulation through:

- API boundaries and visibility checks (`public`, `protected`, `private`)
- caller-context requirements for protected/private operations
- opaque runtime type handles (`CoopType`) and metadata-driven access

This gives consistent runtime safety while keeping C usage simple.

## Quick start

1. Define C structs with `CoopObject` at offset 0 for each subobject.
2. Describe fields, methods, parents, and lifecycle callbacks in `CoopTypeDef`.
3. Register types with `coop_type_register`.
4. Create instances with `coop_object_new` and invoke methods with `coop_invoke_*`.
5. Use `coop_invoke_parent` for explicit parent behavior.
6. Validate relationships with `coop_type_is_a`, `coop_object_is_a`, and `coop_object_as`.

See `/home/runner/work/COOP/COOP/tests/test_coop.c` for end-to-end examples.

## API guarantees

- Deterministic dispatch and lookup order.
- Abstract types cannot be instantiated.
- Multiple inheritance uses explicit parent offsets and contiguous memory.
- Visibility is enforced by runtime access checks.
- Static members are owned per registered type instance.
