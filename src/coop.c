#include "coop/coop.h"

#include <stdlib.h>
#include <string.h>

typedef enum CoopAccessMode {
    ACCESS_PUBLIC = 0,
    ACCESS_PROTECTED,
    ACCESS_PRIVATE
} CoopAccessMode;

typedef struct MethodLookup {
    const CoopMethodDesc *method;
    const CoopType *owner;
    size_t self_offset;
} MethodLookup;

typedef struct FieldLookup {
    const CoopFieldDesc *field;
    const CoopType *owner;
    size_t self_offset;
} FieldLookup;

struct CoopType {
    CoopTypeDef def;
    unsigned char *static_storage;
};

static bool has_ancestor_offset(const CoopType *type,
                                const CoopType *target,
                                size_t base,
                                size_t *out_offset) {
    if (type == NULL || target == NULL) {
        return false;
    }

    if (type == target) {
        if (out_offset != NULL) {
            *out_offset = base;
        }
        return true;
    }

    for (size_t i = 0; i < type->def.parent_count; ++i) {
        const CoopParentLink *link = &type->def.parents[i];
        if (has_ancestor_offset(link->type, target, base + link->offset, out_offset)) {
            return true;
        }
    }

    return false;
}

static bool visibility_allows(CoopVisibility visibility,
                              CoopAccessMode access_mode,
                              const CoopType *owner,
                              const CoopType *caller) {
    if (visibility == COOP_VIS_PUBLIC) {
        return true;
    }

    if (access_mode == ACCESS_PUBLIC) {
        return false;
    }

    if (visibility == COOP_VIS_PROTECTED) {
        return caller != NULL && owner != NULL && coop_type_is_a(caller, owner);
    }

    return access_mode == ACCESS_PRIVATE && caller == owner;
}

static bool lookup_method_in_type(const CoopType *type,
                                  const char *name,
                                  bool want_static,
                                  size_t base_offset,
                                  MethodLookup *out) {
    if (type == NULL || name == NULL) {
        return false;
    }

    for (size_t i = 0; i < type->def.method_count; ++i) {
        const CoopMethodDesc *method = &type->def.methods[i];
        if (method->is_static != want_static) {
            continue;
        }
        if (strcmp(method->name, name) == 0) {
            if (out != NULL) {
                out->method = method;
                out->owner = type;
                out->self_offset = base_offset;
            }
            return true;
        }
    }

    for (size_t i = 0; i < type->def.parent_count; ++i) {
        const CoopParentLink *link = &type->def.parents[i];
        if (lookup_method_in_type(link->type, name, want_static, base_offset + link->offset, out)) {
            return true;
        }
    }

    return false;
}

static bool lookup_field_in_type(const CoopType *type,
                                 const char *name,
                                 bool want_static,
                                 size_t base_offset,
                                 FieldLookup *out) {
    if (type == NULL || name == NULL) {
        return false;
    }

    for (size_t i = 0; i < type->def.field_count; ++i) {
        const CoopFieldDesc *field = &type->def.fields[i];
        if (field->is_static != want_static) {
            continue;
        }
        if (strcmp(field->name, name) == 0) {
            if (out != NULL) {
                out->field = field;
                out->owner = type;
                out->self_offset = base_offset;
            }
            return true;
        }
    }

    for (size_t i = 0; i < type->def.parent_count; ++i) {
        const CoopParentLink *link = &type->def.parents[i];
        if (lookup_field_in_type(link->type, name, want_static, base_offset + link->offset, out)) {
            return true;
        }
    }

    return false;
}

static void init_headers_for_type(const CoopType *type,
                                  const CoopType *dynamic_type,
                                  unsigned char *root,
                                  size_t base_offset,
                                  size_t root_size) {
    if (base_offset + sizeof(CoopObject) <= root_size) {
        CoopObject *header = (CoopObject *)(void *)(root + base_offset);
        header->type = dynamic_type;
    }

    for (size_t i = 0; i < type->def.parent_count; ++i) {
        const CoopParentLink *link = &type->def.parents[i];
        init_headers_for_type(link->type,
                              dynamic_type,
                              root,
                              base_offset + link->offset,
                              root_size);
    }
}

static void run_ctors(const CoopType *type, unsigned char *root, size_t base_offset) {
    for (size_t i = 0; i < type->def.parent_count; ++i) {
        const CoopParentLink *link = &type->def.parents[i];
        run_ctors(link->type, root, base_offset + link->offset);
    }

    if (type->def.ctor != NULL) {
        type->def.ctor((void *)(root + base_offset));
    }
}

static void run_dtors(const CoopType *type, unsigned char *root, size_t base_offset) {
    if (type->def.dtor != NULL) {
        type->def.dtor((void *)(root + base_offset));
    }

    for (size_t i = 0; i < type->def.parent_count; ++i) {
        const CoopParentLink *link = &type->def.parents[i];
        run_dtors(link->type, root, base_offset + link->offset);
    }
}

static CoopStatus invoke_instance(void *obj,
                                  const CoopType *lookup_root,
                                  size_t lookup_base,
                                  CoopAccessMode access_mode,
                                  const CoopType *caller,
                                  const char *method_name,
                                  void *result,
                                  void **args,
                                  size_t arg_count) {
    if (obj == NULL || lookup_root == NULL || method_name == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    MethodLookup lookup = {0};
    if (!lookup_method_in_type(lookup_root, method_name, false, lookup_base, &lookup)) {
        return COOP_STATUS_NOT_FOUND;
    }

    if (!visibility_allows(lookup.method->visibility, access_mode, lookup.owner, caller)) {
        return COOP_STATUS_ACCESS_DENIED;
    }

    if (lookup.method->fn == NULL) {
        return COOP_STATUS_INVALID_CALL;
    }

    unsigned char *root = (unsigned char *)(void *)obj;
    (void)lookup.method->fn((void *)(root + lookup.self_offset), result, args, arg_count);
    return COOP_STATUS_OK;
}

static CoopStatus invoke_static(const CoopType *owner,
                                CoopAccessMode access_mode,
                                const CoopType *caller,
                                const char *method_name,
                                void *result,
                                void **args,
                                size_t arg_count) {
    if (owner == NULL || method_name == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    MethodLookup lookup = {0};
    if (!lookup_method_in_type(owner, method_name, true, 0u, &lookup)) {
        return COOP_STATUS_NOT_FOUND;
    }

    if (!visibility_allows(lookup.method->visibility, access_mode, lookup.owner, caller)) {
        return COOP_STATUS_ACCESS_DENIED;
    }

    if (lookup.method->fn == NULL) {
        return COOP_STATUS_INVALID_CALL;
    }

    void *storage = (void *)lookup.owner->static_storage;
    (void)lookup.method->fn(storage, result, args, arg_count);
    return COOP_STATUS_OK;
}

static CoopStatus field_ptr_instance(void *obj,
                                     const CoopType *lookup_root,
                                     size_t lookup_base,
                                     CoopAccessMode access_mode,
                                     const CoopType *caller,
                                     const char *field_name,
                                     void **out_ptr) {
    if (obj == NULL || lookup_root == NULL || field_name == NULL || out_ptr == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    FieldLookup lookup = {0};
    if (!lookup_field_in_type(lookup_root, field_name, false, lookup_base, &lookup)) {
        return COOP_STATUS_NOT_FOUND;
    }

    if (!visibility_allows(lookup.field->visibility, access_mode, lookup.owner, caller)) {
        return COOP_STATUS_ACCESS_DENIED;
    }

    unsigned char *root = (unsigned char *)(void *)obj;
    *out_ptr = (void *)(root + lookup.self_offset + lookup.field->offset);
    return COOP_STATUS_OK;
}

static CoopStatus field_ptr_static(const CoopType *owner,
                                   CoopAccessMode access_mode,
                                   const CoopType *caller,
                                   const char *field_name,
                                   void **out_ptr) {
    if (owner == NULL || field_name == NULL || out_ptr == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    FieldLookup lookup = {0};
    if (!lookup_field_in_type(owner, field_name, true, 0u, &lookup)) {
        return COOP_STATUS_NOT_FOUND;
    }

    if (!visibility_allows(lookup.field->visibility, access_mode, lookup.owner, caller)) {
        return COOP_STATUS_ACCESS_DENIED;
    }

    *out_ptr = (void *)(lookup.owner->static_storage + lookup.field->offset);
    return COOP_STATUS_OK;
}

static bool validate_type_def(const CoopTypeDef *def) {
    if (def == NULL || def->name == NULL) {
        return false;
    }
    if (def->instance_size < sizeof(CoopObject)) {
        return false;
    }
    if ((def->flags & COOP_TYPE_FINAL) != 0u && def->parent_count > 0u) {
        return false;
    }

    for (size_t i = 0; i < def->parent_count; ++i) {
        const CoopParentLink *link = &def->parents[i];
        if (link->type == NULL) {
            return false;
        }
        if ((link->type->def.flags & COOP_TYPE_FINAL) != 0u) {
            return false;
        }
        if (link->offset + link->type->def.instance_size > def->instance_size) {
            return false;
        }
    }

    for (size_t i = 0; i < def->field_count; ++i) {
        const CoopFieldDesc *field = &def->fields[i];
        if (field->name == NULL || field->size == 0u) {
            return false;
        }

        const size_t limit = field->is_static ? def->static_size : def->instance_size;
        if (field->offset + field->size > limit) {
            return false;
        }
    }

    for (size_t i = 0; i < def->method_count; ++i) {
        const CoopMethodDesc *method = &def->methods[i];
        if (method->name == NULL || method->fn == NULL) {
            return false;
        }
    }

    return true;
}

CoopStatus coop_type_register(const CoopTypeDef *def, CoopType **out_type) {
    if (def == NULL || out_type == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }
    if (!validate_type_def(def)) {
        return COOP_STATUS_BAD_LAYOUT;
    }

    CoopType *type = (CoopType *)calloc(1u, sizeof(*type));
    if (type == NULL) {
        return COOP_STATUS_ALLOCATION_FAILED;
    }

    type->def = *def;
    if (def->static_size > 0u) {
        type->static_storage = (unsigned char *)calloc(def->static_size, 1u);
        if (type->static_storage == NULL) {
            free(type);
            return COOP_STATUS_ALLOCATION_FAILED;
        }
    }

    *out_type = type;
    return COOP_STATUS_OK;
}

void coop_type_destroy(CoopType *type) {
    if (type == NULL) {
        return;
    }

    free(type->static_storage);
    free(type);
}

const char *coop_type_name(const CoopType *type) {
    if (type == NULL) {
        return NULL;
    }
    return type->def.name;
}

size_t coop_type_instance_size(const CoopType *type) {
    if (type == NULL) {
        return 0u;
    }
    return type->def.instance_size;
}

bool coop_type_is_a(const CoopType *type, const CoopType *target) {
    return has_ancestor_offset(type, target, 0u, NULL);
}

CoopStatus coop_type_parent_offset(const CoopType *type, const CoopType *target, size_t *out_offset) {
    if (type == NULL || target == NULL || out_offset == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    if (!has_ancestor_offset(type, target, 0u, out_offset)) {
        return COOP_STATUS_NOT_FOUND;
    }

    return COOP_STATUS_OK;
}

CoopStatus coop_object_new(const CoopType *type, void **out_obj) {
    if (type == NULL || out_obj == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }
    if ((type->def.flags & COOP_TYPE_ABSTRACT) != 0u) {
        return COOP_STATUS_ABSTRACT_TYPE;
    }

    unsigned char *instance = (unsigned char *)calloc(type->def.instance_size, 1u);
    if (instance == NULL) {
        return COOP_STATUS_ALLOCATION_FAILED;
    }

    init_headers_for_type(type, type, instance, 0u, type->def.instance_size);
    run_ctors(type, instance, 0u);

    *out_obj = (void *)instance;
    return COOP_STATUS_OK;
}

void coop_object_delete(void *obj) {
    if (obj == NULL) {
        return;
    }

    CoopObject *header = (CoopObject *)(void *)obj;
    if (header->type != NULL) {
        run_dtors(header->type, (unsigned char *)(void *)obj, 0u);
    }
    free(obj);
}

const CoopType *coop_object_type(const void *obj) {
    if (obj == NULL) {
        return NULL;
    }

    const CoopObject *header = (const CoopObject *)(const void *)obj;
    return header->type;
}

bool coop_object_is_a(const void *obj, const CoopType *target) {
    return coop_type_is_a(coop_object_type(obj), target);
}

CoopStatus coop_object_as(void *obj, const CoopType *target, void **out_view) {
    if (obj == NULL || target == NULL || out_view == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    const CoopType *dynamic = coop_object_type(obj);
    size_t offset = 0u;
    if (!has_ancestor_offset(dynamic, target, 0u, &offset)) {
        return COOP_STATUS_TYPE_MISMATCH;
    }

    *out_view = (void *)((unsigned char *)(void *)obj + offset);
    return COOP_STATUS_OK;
}

CoopStatus coop_invoke_public(void *obj, const char *method_name, void *result, void **args, size_t arg_count) {
    const CoopType *dynamic = coop_object_type(obj);
    return invoke_instance(obj, dynamic, 0u, ACCESS_PUBLIC, NULL, method_name, result, args, arg_count);
}

CoopStatus coop_invoke_protected(void *obj,
                                 const CoopType *caller,
                                 const char *method_name,
                                 void *result,
                                 void **args,
                                 size_t arg_count) {
    const CoopType *dynamic = coop_object_type(obj);
    return invoke_instance(obj, dynamic, 0u, ACCESS_PROTECTED, caller, method_name, result, args, arg_count);
}

CoopStatus coop_invoke_private(void *obj,
                               const CoopType *caller,
                               const char *method_name,
                               void *result,
                               void **args,
                               size_t arg_count) {
    const CoopType *dynamic = coop_object_type(obj);
    return invoke_instance(obj, dynamic, 0u, ACCESS_PRIVATE, caller, method_name, result, args, arg_count);
}

CoopStatus coop_invoke_parent(void *obj,
                              const CoopType *parent,
                              const char *method_name,
                              void *result,
                              void **args,
                              size_t arg_count) {
    if (obj == NULL || parent == NULL) {
        return COOP_STATUS_INVALID_ARGUMENT;
    }

    const CoopType *dynamic = coop_object_type(obj);
    size_t base_offset = 0u;
    if (!has_ancestor_offset(dynamic, parent, 0u, &base_offset)) {
        return COOP_STATUS_TYPE_MISMATCH;
    }

    return invoke_instance(obj,
                           parent,
                           base_offset,
                           ACCESS_PROTECTED,
                           dynamic,
                           method_name,
                           result,
                           args,
                           arg_count);
}

CoopStatus coop_invoke_static_public(const CoopType *owner,
                                     const char *method_name,
                                     void *result,
                                     void **args,
                                     size_t arg_count) {
    return invoke_static(owner, ACCESS_PUBLIC, NULL, method_name, result, args, arg_count);
}

CoopStatus coop_invoke_static_protected(const CoopType *owner,
                                        const CoopType *caller,
                                        const char *method_name,
                                        void *result,
                                        void **args,
                                        size_t arg_count) {
    return invoke_static(owner, ACCESS_PROTECTED, caller, method_name, result, args, arg_count);
}

CoopStatus coop_invoke_static_private(const CoopType *owner,
                                      const CoopType *caller,
                                      const char *method_name,
                                      void *result,
                                      void **args,
                                      size_t arg_count) {
    return invoke_static(owner, ACCESS_PRIVATE, caller, method_name, result, args, arg_count);
}

CoopStatus coop_field_public_ptr(void *obj, const char *field_name, void **out_ptr) {
    const CoopType *dynamic = coop_object_type(obj);
    return field_ptr_instance(obj, dynamic, 0u, ACCESS_PUBLIC, NULL, field_name, out_ptr);
}

CoopStatus coop_field_protected_ptr(void *obj, const CoopType *caller, const char *field_name, void **out_ptr) {
    const CoopType *dynamic = coop_object_type(obj);
    return field_ptr_instance(obj, dynamic, 0u, ACCESS_PROTECTED, caller, field_name, out_ptr);
}

CoopStatus coop_field_private_ptr(void *obj, const CoopType *caller, const char *field_name, void **out_ptr) {
    const CoopType *dynamic = coop_object_type(obj);
    return field_ptr_instance(obj, dynamic, 0u, ACCESS_PRIVATE, caller, field_name, out_ptr);
}

CoopStatus coop_static_field_public_ptr(const CoopType *owner, const char *field_name, void **out_ptr) {
    return field_ptr_static(owner, ACCESS_PUBLIC, NULL, field_name, out_ptr);
}

CoopStatus coop_static_field_protected_ptr(const CoopType *owner,
                                           const CoopType *caller,
                                           const char *field_name,
                                           void **out_ptr) {
    return field_ptr_static(owner, ACCESS_PROTECTED, caller, field_name, out_ptr);
}

CoopStatus coop_static_field_private_ptr(const CoopType *owner,
                                         const CoopType *caller,
                                         const char *field_name,
                                         void **out_ptr) {
    return field_ptr_static(owner, ACCESS_PRIVATE, caller, field_name, out_ptr);
}

const char *coop_status_string(CoopStatus status) {
    switch (status) {
        case COOP_STATUS_OK:
            return "ok";
        case COOP_STATUS_INVALID_ARGUMENT:
            return "invalid_argument";
        case COOP_STATUS_NOT_FOUND:
            return "not_found";
        case COOP_STATUS_ACCESS_DENIED:
            return "access_denied";
        case COOP_STATUS_ABSTRACT_TYPE:
            return "abstract_type";
        case COOP_STATUS_ALLOCATION_FAILED:
            return "allocation_failed";
        case COOP_STATUS_BAD_LAYOUT:
            return "bad_layout";
        case COOP_STATUS_TYPE_MISMATCH:
            return "type_mismatch";
        case COOP_STATUS_INVALID_CALL:
            return "invalid_call";
        default:
            return "unknown";
    }
}

static const CoopApi COOP_API = {
    .type_register = coop_type_register,
    .type_destroy = coop_type_destroy,
    .type_name = coop_type_name,
    .type_instance_size = coop_type_instance_size,
    .type_is_a = coop_type_is_a,
    .type_parent_offset = coop_type_parent_offset,
    .object_new = coop_object_new,
    .object_delete = coop_object_delete,
    .object_type = coop_object_type,
    .object_is_a = coop_object_is_a,
    .object_as = coop_object_as,
    .invoke_public = coop_invoke_public,
    .invoke_protected = coop_invoke_protected,
    .invoke_private = coop_invoke_private,
    .invoke_parent = coop_invoke_parent,
    .invoke_static_public = coop_invoke_static_public,
    .invoke_static_protected = coop_invoke_static_protected,
    .invoke_static_private = coop_invoke_static_private,
    .field_public_ptr = coop_field_public_ptr,
    .field_protected_ptr = coop_field_protected_ptr,
    .field_private_ptr = coop_field_private_ptr,
    .static_field_public_ptr = coop_static_field_public_ptr,
    .static_field_protected_ptr = coop_static_field_protected_ptr,
    .static_field_private_ptr = coop_static_field_private_ptr,
    .status_string = coop_status_string,
};

const CoopApi *coop = &COOP_API;
