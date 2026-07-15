#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoopType CoopType;

typedef struct CoopObject {
    const CoopType *type;
} CoopObject;

typedef enum CoopStatus {
    COOP_STATUS_OK = 0,
    COOP_STATUS_INVALID_ARGUMENT,
    COOP_STATUS_NOT_FOUND,
    COOP_STATUS_ACCESS_DENIED,
    COOP_STATUS_ABSTRACT_TYPE,
    COOP_STATUS_ALLOCATION_FAILED,
    COOP_STATUS_BAD_LAYOUT,
    COOP_STATUS_TYPE_MISMATCH,
    COOP_STATUS_INVALID_CALL
} CoopStatus;

typedef enum CoopVisibility {
    COOP_VIS_PUBLIC = 0,
    COOP_VIS_PROTECTED,
    COOP_VIS_PRIVATE
} CoopVisibility;

enum {
    COOP_TYPE_ABSTRACT = 1u << 0,
    COOP_TYPE_FINAL = 1u << 1
};

typedef int (*CoopMethodFn)(void *self, void *result, void **args, size_t arg_count);
typedef void (*CoopCtorFn)(void *self);
typedef void (*CoopDtorFn)(void *self);

typedef struct CoopParentLink {
    const CoopType *type;
    size_t offset;
} CoopParentLink;

typedef struct CoopFieldDesc {
    const char *name;
    size_t offset;
    size_t size;
    CoopVisibility visibility;
    bool is_static : 1;
} CoopFieldDesc;

typedef struct CoopMethodDesc {
    const char *name;
    CoopMethodFn fn;
    CoopVisibility visibility;
    bool is_static : 1;
    bool is_virtual : 1;
} CoopMethodDesc;

typedef struct CoopTypeDef {
    const char *name;
    size_t instance_size;
    size_t static_size;
    uint32_t flags;
    const CoopParentLink *parents;
    size_t parent_count;
    const CoopFieldDesc *fields;
    size_t field_count;
    const CoopMethodDesc *methods;
    size_t method_count;
    CoopCtorFn ctor;
    CoopDtorFn dtor;
} CoopTypeDef;

typedef struct CoopApi {
    CoopStatus (*type_register)(const CoopTypeDef *def, CoopType **out_type);
    void (*type_destroy)(CoopType *type);
    const char *(*type_name)(const CoopType *type);
    size_t (*type_instance_size)(const CoopType *type);
    bool (*type_is_a)(const CoopType *type, const CoopType *target);
    CoopStatus (*type_parent_offset)(const CoopType *type, const CoopType *target, size_t *out_offset);
    CoopStatus (*object_new)(const CoopType *type, void **out_obj);
    void (*object_delete)(void *obj);
    const CoopType *(*object_type)(const void *obj);
    bool (*object_is_a)(const void *obj, const CoopType *target);
    CoopStatus (*object_as)(void *obj, const CoopType *target, void **out_view);
    CoopStatus (*invoke_public)(void *obj, const char *method_name, void *result, void **args, size_t arg_count);
    CoopStatus (*invoke_protected)(void *obj,
                                   const CoopType *caller,
                                   const char *method_name,
                                   void *result,
                                   void **args,
                                   size_t arg_count);
    CoopStatus (*invoke_private)(void *obj,
                                 const CoopType *caller,
                                 const char *method_name,
                                 void *result,
                                 void **args,
                                 size_t arg_count);
    CoopStatus (*invoke_parent)(void *obj,
                                const CoopType *parent,
                                const char *method_name,
                                void *result,
                                void **args,
                                size_t arg_count);
    CoopStatus (*invoke_static_public)(const CoopType *owner,
                                       const char *method_name,
                                       void *result,
                                       void **args,
                                       size_t arg_count);
    CoopStatus (*invoke_static_protected)(const CoopType *owner,
                                          const CoopType *caller,
                                          const char *method_name,
                                          void *result,
                                          void **args,
                                          size_t arg_count);
    CoopStatus (*invoke_static_private)(const CoopType *owner,
                                        const CoopType *caller,
                                        const char *method_name,
                                        void *result,
                                        void **args,
                                        size_t arg_count);
    CoopStatus (*field_public_ptr)(void *obj, const char *field_name, void **out_ptr);
    CoopStatus (*field_protected_ptr)(void *obj, const CoopType *caller, const char *field_name, void **out_ptr);
    CoopStatus (*field_private_ptr)(void *obj, const CoopType *caller, const char *field_name, void **out_ptr);
    CoopStatus (*static_field_public_ptr)(const CoopType *owner, const char *field_name, void **out_ptr);
    CoopStatus (*static_field_protected_ptr)(const CoopType *owner,
                                             const CoopType *caller,
                                             const char *field_name,
                                             void **out_ptr);
    CoopStatus (*static_field_private_ptr)(const CoopType *owner,
                                           const CoopType *caller,
                                           const char *field_name,
                                           void **out_ptr);
    const char *(*status_string)(CoopStatus status);
} CoopApi;

extern const CoopApi *coop;

#ifdef __cplusplus
}
#endif
