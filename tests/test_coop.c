#include "coop/coop.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Animal {
    CoopObject base;
    int age;
    int protected_counter;
    int secret;
} Animal;

typedef struct Pet {
    CoopObject base;
    int affinity;
} Pet;

typedef struct Dog {
    Animal animal;
    Pet pet;
    int barks;
} Dog;

typedef struct Chimera {
    Animal animal;
    Pet pet;
} Chimera;

typedef struct AnimalStatics {
    int population;
    int revision;
    int private_seed;
} AnimalStatics;

static int g_ctor_count = 0;
static int g_dtor_count = 0;

static CoopType *g_animal_type = NULL;
static CoopType *g_pet_type = NULL;
static CoopType *g_dog_type = NULL;
static CoopType *g_chimera_type = NULL;
static CoopType *g_entity_type = NULL;

#define EXPECT_TRUE(expr)                                                                             \
    do {                                                                                              \
        if (!(expr)) {                                                                                \
            fprintf(stderr, "EXPECT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);       \
            return false;                                                                             \
        }                                                                                             \
    } while (0)

#define EXPECT_STATUS(actual, expected)                                                               \
    do {                                                                                              \
        CoopStatus _a = (actual);                                                                     \
        CoopStatus _e = (expected);                                                                   \
        if (_a != _e) {                                                                               \
            fprintf(stderr,                                                                            \
                    "EXPECT_STATUS failed at %s:%d: got %s expected %s\n",                         \
                    __FILE__,                                                                          \
                    __LINE__,                                                                          \
                    coop_status_string(_a),                                                           \
                    coop_status_string(_e));                                                          \
            return false;                                                                             \
        }                                                                                             \
    } while (0)

#define EXPECT_INT_EQ(actual, expected)                                                               \
    do {                                                                                              \
        int _a = (actual);                                                                            \
        int _e = (expected);                                                                          \
        if (_a != _e) {                                                                               \
            fprintf(stderr, "EXPECT_INT_EQ failed at %s:%d: got %d expected %d\n",                 \
                    __FILE__,                                                                          \
                    __LINE__,                                                                          \
                    _a,                                                                                \
                    _e);                                                                               \
            return false;                                                                             \
        }                                                                                             \
    } while (0)

static void animal_ctor(void *self) {
    Animal *animal = (Animal *)self;
    animal->age = 1;
    animal->protected_counter = 0;
    animal->secret = 7;
    g_ctor_count++;
}

static void pet_ctor(void *self) {
    Pet *pet = (Pet *)self;
    pet->affinity = 10;
    g_ctor_count++;
}

static void dog_ctor(void *self) {
    Dog *dog = (Dog *)self;
    dog->barks = 3;
    g_ctor_count++;
}

static void animal_dtor(void *self) {
    (void)self;
    g_dtor_count++;
}

static void pet_dtor(void *self) {
    (void)self;
    g_dtor_count++;
}

static void dog_dtor(void *self) {
    (void)self;
    g_dtor_count++;
}

static int animal_speak(void *self, void *result, void **args, size_t arg_count) {
    (void)self;
    (void)args;
    (void)arg_count;
    if (result != NULL) {
        *(int *)result = 1;
    }
    return 0;
}

static int pet_speak(void *self, void *result, void **args, size_t arg_count) {
    (void)self;
    (void)args;
    (void)arg_count;
    if (result != NULL) {
        *(int *)result = 3;
    }
    return 0;
}

static int dog_speak(void *self, void *result, void **args, size_t arg_count) {
    (void)args;
    (void)arg_count;
    Dog *dog = (Dog *)self;
    dog->barks += 1;
    if (result != NULL) {
        *(int *)result = 2;
    }
    return 0;
}

static int animal_grow(void *self, void *result, void **args, size_t arg_count) {
    Animal *animal = (Animal *)self;
    if (arg_count != 1 || args == NULL || args[0] == NULL) {
        return -1;
    }

    int delta = *(int *)args[0];
    animal->age += delta;
    if (result != NULL) {
        *(int *)result = animal->age;
    }
    return 0;
}

static int animal_guarded_touch(void *self, void *result, void **args, size_t arg_count) {
    (void)args;
    (void)arg_count;
    Animal *animal = (Animal *)self;
    animal->protected_counter += 1;
    if (result != NULL) {
        *(int *)result = animal->protected_counter;
    }
    return 0;
}

static int animal_set_secret(void *self, void *result, void **args, size_t arg_count) {
    Animal *animal = (Animal *)self;
    if (arg_count != 1 || args == NULL || args[0] == NULL) {
        return -1;
    }

    animal->secret = *(int *)args[0];
    if (result != NULL) {
        *(int *)result = animal->secret;
    }
    return 0;
}

static int animal_static_bump(void *self, void *result, void **args, size_t arg_count) {
    AnimalStatics *st = (AnimalStatics *)self;
    if (arg_count != 1 || args == NULL || args[0] == NULL) {
        return -1;
    }

    st->population += *(int *)args[0];
    if (result != NULL) {
        *(int *)result = st->population;
    }
    return 0;
}

static int animal_static_reseed(void *self, void *result, void **args, size_t arg_count) {
    AnimalStatics *st = (AnimalStatics *)self;
    if (arg_count != 1 || args == NULL || args[0] == NULL) {
        return -1;
    }

    st->private_seed = *(int *)args[0];
    if (result != NULL) {
        *(int *)result = st->private_seed;
    }
    return 0;
}

static const CoopFieldDesc ANIMAL_FIELDS[] = {
    {.name = "age", .offset = offsetof(Animal, age), .size = sizeof(int), .visibility = COOP_VIS_PUBLIC, .is_static = false},
    {.name = "guard", .offset = offsetof(Animal, protected_counter), .size = sizeof(int), .visibility = COOP_VIS_PROTECTED, .is_static = false},
    {.name = "secret", .offset = offsetof(Animal, secret), .size = sizeof(int), .visibility = COOP_VIS_PRIVATE, .is_static = false},
    {.name = "population", .offset = offsetof(AnimalStatics, population), .size = sizeof(int), .visibility = COOP_VIS_PUBLIC, .is_static = true},
    {.name = "revision", .offset = offsetof(AnimalStatics, revision), .size = sizeof(int), .visibility = COOP_VIS_PROTECTED, .is_static = true},
    {.name = "seed", .offset = offsetof(AnimalStatics, private_seed), .size = sizeof(int), .visibility = COOP_VIS_PRIVATE, .is_static = true},
};

static const CoopMethodDesc ANIMAL_METHODS[] = {
    {.name = "speak", .fn = animal_speak, .visibility = COOP_VIS_PUBLIC, .is_static = false, .is_virtual = true},
    {.name = "grow", .fn = animal_grow, .visibility = COOP_VIS_PUBLIC, .is_static = false, .is_virtual = false},
    {.name = "guarded_touch", .fn = animal_guarded_touch, .visibility = COOP_VIS_PROTECTED, .is_static = false, .is_virtual = false},
    {.name = "set_secret", .fn = animal_set_secret, .visibility = COOP_VIS_PRIVATE, .is_static = false, .is_virtual = false},
    {.name = "bump_population", .fn = animal_static_bump, .visibility = COOP_VIS_PUBLIC, .is_static = true, .is_virtual = false},
    {.name = "reseed", .fn = animal_static_reseed, .visibility = COOP_VIS_PRIVATE, .is_static = true, .is_virtual = false},
};

static const CoopFieldDesc PET_FIELDS[] = {
    {.name = "affinity", .offset = offsetof(Pet, affinity), .size = sizeof(int), .visibility = COOP_VIS_PUBLIC, .is_static = false},
};

static const CoopMethodDesc PET_METHODS[] = {
    {.name = "speak", .fn = pet_speak, .visibility = COOP_VIS_PUBLIC, .is_static = false, .is_virtual = true},
};

static const CoopFieldDesc DOG_FIELDS[] = {
    {.name = "barks", .offset = offsetof(Dog, barks), .size = sizeof(int), .visibility = COOP_VIS_PUBLIC, .is_static = false},
};

static const CoopMethodDesc DOG_METHODS[] = {
    {.name = "speak", .fn = dog_speak, .visibility = COOP_VIS_PUBLIC, .is_static = false, .is_virtual = true},
};

static bool register_types(void) {
    CoopStatus st;

    static const CoopTypeDef ANIMAL_DEF = {
        .name = "Animal",
        .instance_size = sizeof(Animal),
        .static_size = sizeof(AnimalStatics),
        .flags = 0u,
        .parents = NULL,
        .parent_count = 0u,
        .fields = ANIMAL_FIELDS,
        .field_count = sizeof(ANIMAL_FIELDS) / sizeof(ANIMAL_FIELDS[0]),
        .methods = ANIMAL_METHODS,
        .method_count = sizeof(ANIMAL_METHODS) / sizeof(ANIMAL_METHODS[0]),
        .ctor = animal_ctor,
        .dtor = animal_dtor,
    };

    static const CoopTypeDef PET_DEF = {
        .name = "Pet",
        .instance_size = sizeof(Pet),
        .static_size = 0u,
        .flags = 0u,
        .parents = NULL,
        .parent_count = 0u,
        .fields = PET_FIELDS,
        .field_count = sizeof(PET_FIELDS) / sizeof(PET_FIELDS[0]),
        .methods = PET_METHODS,
        .method_count = sizeof(PET_METHODS) / sizeof(PET_METHODS[0]),
        .ctor = pet_ctor,
        .dtor = pet_dtor,
    };

    static const CoopTypeDef ENTITY_DEF = {
        .name = "Entity",
        .instance_size = sizeof(CoopObject),
        .static_size = 0u,
        .flags = COOP_TYPE_ABSTRACT,
        .parents = NULL,
        .parent_count = 0u,
        .fields = NULL,
        .field_count = 0u,
        .methods = NULL,
        .method_count = 0u,
        .ctor = NULL,
        .dtor = NULL,
    };

    st = coop_type_register(&ANIMAL_DEF, &g_animal_type);
    if (st != COOP_STATUS_OK) {
        return false;
    }

    st = coop_type_register(&PET_DEF, &g_pet_type);
    if (st != COOP_STATUS_OK) {
        return false;
    }

    st = coop_type_register(&ENTITY_DEF, &g_entity_type);
    if (st != COOP_STATUS_OK) {
        return false;
    }

    static CoopParentLink dog_parents[2];
    dog_parents[0].type = g_animal_type;
    dog_parents[0].offset = offsetof(Dog, animal);
    dog_parents[1].type = g_pet_type;
    dog_parents[1].offset = offsetof(Dog, pet);

    static const CoopTypeDef DOG_DEF = {
        .name = "Dog",
        .instance_size = sizeof(Dog),
        .static_size = 0u,
        .flags = 0u,
        .parents = dog_parents,
        .parent_count = 2u,
        .fields = DOG_FIELDS,
        .field_count = sizeof(DOG_FIELDS) / sizeof(DOG_FIELDS[0]),
        .methods = DOG_METHODS,
        .method_count = sizeof(DOG_METHODS) / sizeof(DOG_METHODS[0]),
        .ctor = dog_ctor,
        .dtor = dog_dtor,
    };

    st = coop_type_register(&DOG_DEF, &g_dog_type);
    if (st != COOP_STATUS_OK) {
        return false;
    }

    static CoopParentLink chimera_parents[2];
    chimera_parents[0].type = g_animal_type;
    chimera_parents[0].offset = offsetof(Chimera, animal);
    chimera_parents[1].type = g_pet_type;
    chimera_parents[1].offset = offsetof(Chimera, pet);

    static const CoopTypeDef CHIMERA_DEF = {
        .name = "Chimera",
        .instance_size = sizeof(Chimera),
        .static_size = 0u,
        .flags = 0u,
        .parents = chimera_parents,
        .parent_count = 2u,
        .fields = NULL,
        .field_count = 0u,
        .methods = NULL,
        .method_count = 0u,
        .ctor = NULL,
        .dtor = NULL,
    };

    st = coop_type_register(&CHIMERA_DEF, &g_chimera_type);
    return st == COOP_STATUS_OK;
}

static void destroy_types(void) {
    coop_type_destroy(g_chimera_type);
    coop_type_destroy(g_dog_type);
    coop_type_destroy(g_entity_type);
    coop_type_destroy(g_pet_type);
    coop_type_destroy(g_animal_type);
    g_chimera_type = NULL;
    g_dog_type = NULL;
    g_entity_type = NULL;
    g_pet_type = NULL;
    g_animal_type = NULL;
}

static bool test_registration_and_rtti(void) {
    EXPECT_TRUE(strcmp(coop_type_name(g_dog_type), "Dog") == 0);
    EXPECT_TRUE(coop_type_is_a(g_dog_type, g_dog_type));
    EXPECT_TRUE(coop_type_is_a(g_dog_type, g_animal_type));
    EXPECT_TRUE(coop_type_is_a(g_dog_type, g_pet_type));
    EXPECT_TRUE(!coop_type_is_a(g_animal_type, g_pet_type));

    size_t off = 0u;
    EXPECT_STATUS(coop_type_parent_offset(g_dog_type, g_pet_type, &off), COOP_STATUS_OK);
    EXPECT_TRUE(off == offsetof(Dog, pet));
    return true;
}

static bool test_abstract_enforcement(void) {
    void *obj = NULL;
    EXPECT_STATUS(coop_object_new(g_entity_type, &obj), COOP_STATUS_ABSTRACT_TYPE);
    EXPECT_TRUE(obj == NULL);
    return true;
}

static bool test_lifecycle_and_dispatch(void) {
    g_ctor_count = 0;
    g_dtor_count = 0;

    void *dog = NULL;
    EXPECT_STATUS(coop_object_new(g_dog_type, &dog), COOP_STATUS_OK);
    EXPECT_INT_EQ(g_ctor_count, 3);

    int out = 0;
    EXPECT_STATUS(coop_invoke_public(dog, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 2);

    EXPECT_STATUS(coop_invoke_parent(dog, g_animal_type, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 1);

    EXPECT_STATUS(coop_invoke_parent(dog, g_pet_type, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 3);

    coop_object_delete(dog);
    EXPECT_INT_EQ(g_dtor_count, 3);
    return true;
}

static bool test_multiple_inheritance_resolution_order(void) {
    void *chimera = NULL;
    EXPECT_STATUS(coop_object_new(g_chimera_type, &chimera), COOP_STATUS_OK);

    int out = 0;
    EXPECT_STATUS(coop_invoke_public(chimera, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 1);

    coop_object_delete(chimera);
    return true;
}

static bool test_parent_view_and_polymorphism(void) {
    void *dog = NULL;
    EXPECT_STATUS(coop_object_new(g_dog_type, &dog), COOP_STATUS_OK);

    EXPECT_TRUE(coop_object_is_a(dog, g_animal_type));
    EXPECT_TRUE(coop_object_is_a(dog, g_pet_type));

    void *pet_view = NULL;
    EXPECT_STATUS(coop_object_as(dog, g_pet_type, &pet_view), COOP_STATUS_OK);
    EXPECT_TRUE((char *)pet_view == (char *)dog + offsetof(Dog, pet));
    EXPECT_TRUE(coop_object_type(pet_view) == g_dog_type);

    coop_object_delete(dog);
    return true;
}

static bool test_visibility_and_encapsulation(void) {
    void *dog = NULL;
    EXPECT_STATUS(coop_object_new(g_dog_type, &dog), COOP_STATUS_OK);

    void *ptr = NULL;
    EXPECT_STATUS(coop_field_public_ptr(dog, "age", &ptr), COOP_STATUS_OK);
    EXPECT_TRUE(ptr != NULL);

    EXPECT_STATUS(coop_field_public_ptr(dog, "guard", &ptr), COOP_STATUS_ACCESS_DENIED);
    EXPECT_STATUS(coop_field_protected_ptr(dog, g_dog_type, "guard", &ptr), COOP_STATUS_OK);
    EXPECT_STATUS(coop_field_private_ptr(dog, g_dog_type, "secret", &ptr), COOP_STATUS_ACCESS_DENIED);
    EXPECT_STATUS(coop_field_private_ptr(dog, g_animal_type, "secret", &ptr), COOP_STATUS_OK);

    int out = 0;
    EXPECT_STATUS(coop_invoke_public(dog, "guarded_touch", &out, NULL, 0u), COOP_STATUS_ACCESS_DENIED);
    EXPECT_STATUS(coop_invoke_protected(dog, g_dog_type, "guarded_touch", &out, NULL, 0u), COOP_STATUS_OK);
    EXPECT_STATUS(coop_invoke_private(dog, g_dog_type, "set_secret", &out, NULL, 0u), COOP_STATUS_ACCESS_DENIED);

    int secret = 77;
    void *args[] = {&secret};
    EXPECT_STATUS(coop_invoke_private(dog, g_animal_type, "set_secret", &out, args, 1u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 77);

    coop_object_delete(dog);
    return true;
}

static bool test_static_fields_and_methods(void) {
    void *ptr = NULL;
    EXPECT_STATUS(coop_static_field_public_ptr(g_animal_type, "population", &ptr), COOP_STATUS_OK);
    int *population = (int *)ptr;
    *population = 10;

    int delta = 5;
    void *args[] = {&delta};
    int out = 0;
    EXPECT_STATUS(coop_invoke_static_public(g_animal_type, "bump_population", &out, args, 1u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 15);

    EXPECT_STATUS(coop_static_field_public_ptr(g_animal_type, "seed", &ptr), COOP_STATUS_ACCESS_DENIED);
    EXPECT_STATUS(coop_invoke_static_private(g_animal_type, g_dog_type, "reseed", &out, args, 1u), COOP_STATUS_ACCESS_DENIED);
    EXPECT_STATUS(coop_invoke_static_private(g_animal_type, g_animal_type, "reseed", &out, args, 1u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 5);

    EXPECT_STATUS(coop_static_field_protected_ptr(g_animal_type, g_dog_type, "revision", &ptr), COOP_STATUS_OK);
    *(int *)ptr = 9;
    EXPECT_STATUS(coop_static_field_public_ptr(g_animal_type, "revision", &ptr), COOP_STATUS_ACCESS_DENIED);
    return true;
}

static bool test_method_argument_and_field_mutation(void) {
    void *dog = NULL;
    EXPECT_STATUS(coop_object_new(g_dog_type, &dog), COOP_STATUS_OK);

    int amount = 4;
    void *args[] = {&amount};
    int out = 0;
    EXPECT_STATUS(coop_invoke_public(dog, "grow", &out, args, 1u), COOP_STATUS_OK);
    EXPECT_INT_EQ(out, 5);

    void *age_ptr = NULL;
    EXPECT_STATUS(coop_field_public_ptr(dog, "age", &age_ptr), COOP_STATUS_OK);
    EXPECT_INT_EQ(*(int *)age_ptr, 5);

    coop_object_delete(dog);
    return true;
}

typedef bool (*TestFn)(void);

typedef struct TestCase {
    const char *name;
    TestFn fn;
} TestCase;

int main(void) {
    if (!register_types()) {
        fprintf(stderr, "failed to register types\n");
        destroy_types();
        return EXIT_FAILURE;
    }

    const TestCase tests[] = {
        {"registration_and_rtti", test_registration_and_rtti},
        {"abstract_enforcement", test_abstract_enforcement},
        {"lifecycle_and_dispatch", test_lifecycle_and_dispatch},
        {"mi_resolution_order", test_multiple_inheritance_resolution_order},
        {"parent_view_and_polymorphism", test_parent_view_and_polymorphism},
        {"visibility_and_encapsulation", test_visibility_and_encapsulation},
        {"static_fields_and_methods", test_static_fields_and_methods},
        {"method_argument_and_field_mutation", test_method_argument_and_field_mutation},
    };

    size_t failed = 0u;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        bool ok = tests[i].fn();
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", tests[i].name);
        if (!ok) {
            failed++;
        }
    }

    destroy_types();

    if (failed != 0u) {
        fprintf(stderr, "%zu test(s) failed\n", failed);
        return EXIT_FAILURE;
    }

    printf("all tests passed\n");
    return EXIT_SUCCESS;
}
