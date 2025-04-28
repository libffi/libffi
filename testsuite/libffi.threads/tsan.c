/* { dg-do run } */

#include "ffitest.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 20

pthread_barrier_t barrier;

typedef float (*callback_fn)(float, float);

void callback(ffi_cif *cif __UNUSED__, void *ret, void **args, void *userdata __UNUSED__) {
    float a = *(float *)args[0];
    float b = *(float *)args[1];
    *(float *)ret = a / 2 + b / 2;
}

void *thread_func(void *arg) {
    pthread_barrier_wait(&barrier);

    ffi_cif cif;
    ffi_type *args[2] = { &ffi_type_float, &ffi_type_float };

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_float, args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif failed\n");
        return NULL;
    }

    ffi_closure *closure = ffi_closure_alloc(sizeof(ffi_closure), (void **)&arg);

    if (ffi_prep_closure_loc(closure, &cif, callback, NULL, arg) != FFI_OK) {
        fprintf(stderr, "ffi_prep_closure_loc failed\n");
        return NULL;
    }

    callback_fn fn = (callback_fn)arg;
    (void) fn(4.0f, 6.0f);

    ffi_closure_free(closure);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_create(&threads[i], NULL, thread_func, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);

    printf("Completed\n");
    return 0;
}
