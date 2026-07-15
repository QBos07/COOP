#include "coop/coop.h"

#include <cmocka.h>
#include <stddef.h>
#include <setjmp.h>
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

static void assert_status(CoopStatus actual, CoopStatus expected) {
    assert_int_equal((int)actual, (int)expected);
}

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

static int register_types(void **state) {
    (void)state;

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

    st = coop->type_register(&ANIMAL_DEF, &g_animal_type);
    if (st != COOP_STATUS_OK) {
        return -1;
    }

    st = coop->type_register(&PET_DEF, &g_pet_type);
    if (st != COOP_STATUS_OK) {
        return -1;
    }

    st = coop->type_register(&ENTITY_DEF, &g_entity_type);
    if (st != COOP_STATUS_OK) {
        return -1;
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

    st = coop->type_register(&DOG_DEF, &g_dog_type);
    if (st != COOP_STATUS_OK) {
        return -1;
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

    st = coop->type_register(&CHIMERA_DEF, &g_chimera_type);
    return st == COOP_STATUS_OK ? 0 : -1;
}

static int destroy_types(void **state) {
    (void)state;

    coop->type_destroy(g_chimera_type);
    coop->type_destroy(g_dog_type);
    coop->type_destroy(g_entity_type);
    coop->type_destroy(g_pet_type);
    coop->type_destroy(g_animal_type);
    g_chimera_type = NULL;
    g_dog_type = NULL;
    g_entity_type = NULL;
    g_pet_type = NULL;
    g_animal_type = NULL;
    return 0;
}

static void test_registration_and_rtti(void **state) {
    (void)state;

    assert_string_equal(coop->type_name(g_dog_type), "Dog");
    assert_true(coop->type_is_a(g_dog_type, g_dog_type));
    assert_true(coop->type_is_a(g_dog_type, g_animal_type));
    assert_true(coop->type_is_a(g_dog_type, g_pet_type));
    assert_false(coop->type_is_a(g_animal_type, g_pet_type));

    size_t off = 0u;
    assert_status(coop->type_parent_offset(g_dog_type, g_pet_type, &off), COOP_STATUS_OK);
    assert_int_equal((int)off, (int)offsetof(Dog, pet));
}

static void test_abstract_enforcement(void **state) {
    (void)state;

    void *obj = NULL;
    assert_status(coop->object_new(g_entity_type, &obj), COOP_STATUS_ABSTRACT_TYPE);
    assert_null(obj);
}

static void test_lifecycle_and_dispatch(void **state) {
    (void)state;

    g_ctor_count = 0;
    g_dtor_count = 0;

    void *dog = NULL;
    assert_status(coop->object_new(g_dog_type, &dog), COOP_STATUS_OK);
    assert_int_equal(g_ctor_count, 3);

    int out = 0;
    assert_status(coop->invoke_public(dog, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    assert_int_equal(out, 2);

    assert_status(coop->invoke_parent(dog, g_animal_type, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    assert_int_equal(out, 1);

    assert_status(coop->invoke_parent(dog, g_pet_type, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    assert_int_equal(out, 3);

    coop->object_delete(dog);
    assert_int_equal(g_dtor_count, 3);
}

static void test_multiple_inheritance_resolution_order(void **state) {
    (void)state;

    void *chimera = NULL;
    assert_status(coop->object_new(g_chimera_type, &chimera), COOP_STATUS_OK);

    int out = 0;
    assert_status(coop->invoke_public(chimera, "speak", &out, NULL, 0u), COOP_STATUS_OK);
    assert_int_equal(out, 1);

    coop->object_delete(chimera);
}

static void test_parent_view_and_polymorphism(void **state) {
    (void)state;

    void *dog = NULL;
    assert_status(coop->object_new(g_dog_type, &dog), COOP_STATUS_OK);

    assert_true(coop->object_is_a(dog, g_animal_type));
    assert_true(coop->object_is_a(dog, g_pet_type));

    void *pet_view = NULL;
    assert_status(coop->object_as(dog, g_pet_type, &pet_view), COOP_STATUS_OK);
    assert_ptr_equal((char *)pet_view, (char *)dog + offsetof(Dog, pet));
    assert_ptr_equal(coop->object_type(pet_view), g_dog_type);

    coop->object_delete(dog);
}

static void test_visibility_and_encapsulation(void **state) {
    (void)state;

    void *dog = NULL;
    assert_status(coop->object_new(g_dog_type, &dog), COOP_STATUS_OK);

    void *ptr = NULL;
    assert_status(coop->field_public_ptr(dog, "age", &ptr), COOP_STATUS_OK);
    assert_non_null(ptr);

    assert_status(coop->field_public_ptr(dog, "guard", &ptr), COOP_STATUS_ACCESS_DENIED);
    assert_status(coop->field_protected_ptr(dog, g_dog_type, "guard", &ptr), COOP_STATUS_OK);
    assert_status(coop->field_private_ptr(dog, g_dog_type, "secret", &ptr), COOP_STATUS_ACCESS_DENIED);
    assert_status(coop->field_private_ptr(dog, g_animal_type, "secret", &ptr), COOP_STATUS_OK);

    int out = 0;
    assert_status(coop->invoke_public(dog, "guarded_touch", &out, NULL, 0u), COOP_STATUS_ACCESS_DENIED);
    assert_status(coop->invoke_protected(dog, g_dog_type, "guarded_touch", &out, NULL, 0u), COOP_STATUS_OK);
    assert_status(coop->invoke_private(dog, g_dog_type, "set_secret", &out, NULL, 0u), COOP_STATUS_ACCESS_DENIED);

    int secret = 77;
    void *args[] = {&secret};
    assert_status(coop->invoke_private(dog, g_animal_type, "set_secret", &out, args, 1u), COOP_STATUS_OK);
    assert_int_equal(out, 77);

    coop->object_delete(dog);
}

static void test_static_fields_and_methods(void **state) {
    (void)state;

    void *ptr = NULL;
    assert_status(coop->static_field_public_ptr(g_animal_type, "population", &ptr), COOP_STATUS_OK);
    int *population = (int *)ptr;
    *population = 10;

    int delta = 5;
    void *args[] = {&delta};
    int out = 0;
    assert_status(coop->invoke_static_public(g_animal_type, "bump_population", &out, args, 1u), COOP_STATUS_OK);
    assert_int_equal(out, 15);

    assert_status(coop->static_field_public_ptr(g_animal_type, "seed", &ptr), COOP_STATUS_ACCESS_DENIED);
    assert_status(coop->invoke_static_private(g_animal_type, g_dog_type, "reseed", &out, args, 1u), COOP_STATUS_ACCESS_DENIED);
    assert_status(coop->invoke_static_private(g_animal_type, g_animal_type, "reseed", &out, args, 1u), COOP_STATUS_OK);
    assert_int_equal(out, 5);

    assert_status(coop->static_field_protected_ptr(g_animal_type, g_dog_type, "revision", &ptr), COOP_STATUS_OK);
    *(int *)ptr = 9;
    assert_status(coop->static_field_public_ptr(g_animal_type, "revision", &ptr), COOP_STATUS_ACCESS_DENIED);
}

static void test_method_argument_and_field_mutation(void **state) {
    (void)state;

    void *dog = NULL;
    assert_status(coop->object_new(g_dog_type, &dog), COOP_STATUS_OK);

    int amount = 4;
    void *args[] = {&amount};
    int out = 0;
    assert_status(coop->invoke_public(dog, "grow", &out, args, 1u), COOP_STATUS_OK);
    assert_int_equal(out, 5);

    void *age_ptr = NULL;
    assert_status(coop->field_public_ptr(dog, "age", &age_ptr), COOP_STATUS_OK);
    assert_int_equal(*(int *)age_ptr, 5);

    coop->object_delete(dog);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_registration_and_rtti),
        cmocka_unit_test(test_abstract_enforcement),
        cmocka_unit_test(test_lifecycle_and_dispatch),
        cmocka_unit_test(test_multiple_inheritance_resolution_order),
        cmocka_unit_test(test_parent_view_and_polymorphism),
        cmocka_unit_test(test_visibility_and_encapsulation),
        cmocka_unit_test(test_static_fields_and_methods),
        cmocka_unit_test(test_method_argument_and_field_mutation),
    };

    return cmocka_run_group_tests(tests, register_types, destroy_types);
}
