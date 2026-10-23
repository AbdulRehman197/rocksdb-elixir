#include <windows.h>
#include <erl_nif.h>
#include <stdlib.h>
#include <string.h>

typedef struct rocksdb_t rocksdb_t;
typedef struct rocksdb_options_t rocksdb_options_t;
typedef struct rocksdb_writeoptions_t rocksdb_writeoptions_t;
typedef struct rocksdb_readoptions_t rocksdb_readoptions_t;
typedef struct rocksdb_iterator_t rocksdb_iterator_t;

typedef rocksdb_options_t* (*fn_rocksdb_options_create_t)(void);
typedef void (*fn_rocksdb_options_destroy_t)(rocksdb_options_t*);
typedef void (*fn_rocksdb_options_set_create_if_missing_t)(rocksdb_options_t*, unsigned char);
typedef rocksdb_writeoptions_t* (*fn_rocksdb_writeoptions_create_t)(void);
typedef void (*fn_rocksdb_writeoptions_destroy_t)(rocksdb_writeoptions_t*);
typedef rocksdb_readoptions_t* (*fn_rocksdb_readoptions_create_t)(void);
typedef void (*fn_rocksdb_readoptions_destroy_t)(rocksdb_readoptions_t*);
typedef rocksdb_t* (*fn_rocksdb_open_t)(const rocksdb_options_t*, const char*, char**);
typedef void (*fn_rocksdb_close_t)(rocksdb_t*);
typedef void (*fn_rocksdb_put_t)(rocksdb_t*, const rocksdb_writeoptions_t*, const char*, size_t, const char*, size_t, char**);
typedef char* (*fn_rocksdb_get_t)(rocksdb_t*, const rocksdb_readoptions_t*, const char*, size_t, size_t*, char**);
typedef void (*fn_rocksdb_delete_t)(rocksdb_t*, const rocksdb_writeoptions_t*, const char*, size_t, char**);
typedef void (*fn_rocksdb_free_t)(void*);

typedef rocksdb_iterator_t* (*fn_rocksdb_create_iterator_t)(rocksdb_t*, const rocksdb_readoptions_t*);
typedef void (*fn_rocksdb_iter_destroy_t)(rocksdb_iterator_t*);
typedef unsigned char (*fn_rocksdb_iter_valid_t)(const rocksdb_iterator_t*);
typedef void (*fn_rocksdb_iter_seek_to_first_t)(rocksdb_iterator_t*);
typedef void (*fn_rocksdb_iter_seek_to_last_t)(rocksdb_iterator_t*);
typedef void (*fn_rocksdb_iter_seek_t)(rocksdb_iterator_t*, const char*, size_t);
typedef void (*fn_rocksdb_iter_next_t)(rocksdb_iterator_t*);
typedef void (*fn_rocksdb_iter_prev_t)(rocksdb_iterator_t*);
typedef const char* (*fn_rocksdb_iter_key_t)(const rocksdb_iterator_t*, size_t*);
typedef const char* (*fn_rocksdb_iter_value_t)(const rocksdb_iterator_t*, size_t*);
typedef void (*fn_rocksdb_iter_get_error_t)(const rocksdb_iterator_t*, char**);

static fn_rocksdb_options_create_t p_rocksdb_options_create = NULL;
static fn_rocksdb_options_destroy_t p_rocksdb_options_destroy = NULL;
static fn_rocksdb_options_set_create_if_missing_t p_rocksdb_options_set_create_if_missing = NULL;
static fn_rocksdb_writeoptions_create_t p_rocksdb_writeoptions_create = NULL;
static fn_rocksdb_writeoptions_destroy_t p_rocksdb_writeoptions_destroy = NULL;
static fn_rocksdb_readoptions_create_t p_rocksdb_readoptions_create = NULL;
static fn_rocksdb_readoptions_destroy_t p_rocksdb_readoptions_destroy = NULL;
static fn_rocksdb_open_t p_rocksdb_open = NULL;
static fn_rocksdb_close_t p_rocksdb_close = NULL;
static fn_rocksdb_put_t p_rocksdb_put = NULL;
static fn_rocksdb_get_t p_rocksdb_get = NULL;
static fn_rocksdb_delete_t p_rocksdb_delete = NULL;
static fn_rocksdb_free_t p_rocksdb_free = NULL;

static fn_rocksdb_create_iterator_t p_rocksdb_create_iterator = NULL;
static fn_rocksdb_iter_destroy_t p_rocksdb_iter_destroy = NULL;
static fn_rocksdb_iter_valid_t p_rocksdb_iter_valid = NULL;
static fn_rocksdb_iter_seek_to_first_t p_rocksdb_iter_seek_to_first = NULL;
static fn_rocksdb_iter_seek_to_last_t p_rocksdb_iter_seek_to_last = NULL;
static fn_rocksdb_iter_seek_t p_rocksdb_iter_seek = NULL;
static fn_rocksdb_iter_next_t p_rocksdb_iter_next = NULL;
static fn_rocksdb_iter_prev_t p_rocksdb_iter_prev = NULL;
static fn_rocksdb_iter_key_t p_rocksdb_iter_key = NULL;
static fn_rocksdb_iter_value_t p_rocksdb_iter_value = NULL;
static fn_rocksdb_iter_get_error_t p_rocksdb_iter_get_error = NULL;

typedef struct {
    rocksdb_t* db;
    rocksdb_writeoptions_t* write_opts;
    rocksdb_readoptions_t* read_opts;
} RocksdbHandle;

typedef struct {
    rocksdb_iterator_t* iter;
} RocksdbIterHandle;

static ErlNifResourceType* ROCKSDB_RES_TYPE = NULL;
static ErlNifResourceType* ROCKSDB_ITER_RES_TYPE = NULL;

static void rocksdb_handle_destructor(ErlNifEnv* env, void* obj) {
    (void)env;
    RocksdbHandle* h = (RocksdbHandle*)obj;
    if (h->db) {
        if (p_rocksdb_close) p_rocksdb_close(h->db);
        h->db = NULL;
    }
    if (h->write_opts) {
        if (p_rocksdb_writeoptions_destroy) p_rocksdb_writeoptions_destroy(h->write_opts);
        h->write_opts = NULL;
    }
    if (h->read_opts) {
        if (p_rocksdb_readoptions_destroy) p_rocksdb_readoptions_destroy(h->read_opts);
        h->read_opts = NULL;
    }
}

static void rocksdb_iter_handle_destructor(ErlNifEnv* env, void* obj) {
    (void)env;
    RocksdbIterHandle* ih = (RocksdbIterHandle*)obj;
    if (ih->iter) {
        if (p_rocksdb_iter_destroy) p_rocksdb_iter_destroy(ih->iter);
        ih->iter = NULL;
    }
}

static int init_rocksdb_dll(void) {
    char dllPath[MAX_PATH];
    HMODULE hNif = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&init_rocksdb_dll, &hNif)) {
        GetModuleFileNameA(hNif, dllPath, MAX_PATH);
        char* p = strrchr(dllPath, '\\');
        if (!p) p = strrchr(dllPath, '/');
        if (p) {
            *(p + 1) = '\0';
            strcat(dllPath, "rocksdb.dll");
        }
    } else {
        strcpy(dllPath, "rocksdb.dll");
    }

    HMODULE hMod = LoadLibraryExA(dllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hMod) {
        hMod = LoadLibraryA(dllPath);
    }
    if (!hMod) {
        hMod = LoadLibraryA("rocksdb.dll");
    }
    if (!hMod) return 0;

    p_rocksdb_options_create = (fn_rocksdb_options_create_t)GetProcAddress(hMod, "rocksdb_options_create");
    p_rocksdb_options_destroy = (fn_rocksdb_options_destroy_t)GetProcAddress(hMod, "rocksdb_options_destroy");
    p_rocksdb_options_set_create_if_missing = (fn_rocksdb_options_set_create_if_missing_t)GetProcAddress(hMod, "rocksdb_options_set_create_if_missing");
    p_rocksdb_writeoptions_create = (fn_rocksdb_writeoptions_create_t)GetProcAddress(hMod, "rocksdb_writeoptions_create");
    p_rocksdb_writeoptions_destroy = (fn_rocksdb_writeoptions_destroy_t)GetProcAddress(hMod, "rocksdb_writeoptions_destroy");
    p_rocksdb_readoptions_create = (fn_rocksdb_readoptions_create_t)GetProcAddress(hMod, "rocksdb_readoptions_create");
    p_rocksdb_readoptions_destroy = (fn_rocksdb_readoptions_destroy_t)GetProcAddress(hMod, "rocksdb_readoptions_destroy");
    p_rocksdb_open = (fn_rocksdb_open_t)GetProcAddress(hMod, "rocksdb_open");
    p_rocksdb_close = (fn_rocksdb_close_t)GetProcAddress(hMod, "rocksdb_close");
    p_rocksdb_put = (fn_rocksdb_put_t)GetProcAddress(hMod, "rocksdb_put");
    p_rocksdb_get = (fn_rocksdb_get_t)GetProcAddress(hMod, "rocksdb_get");
    p_rocksdb_delete = (fn_rocksdb_delete_t)GetProcAddress(hMod, "rocksdb_delete");
    p_rocksdb_free = (fn_rocksdb_free_t)GetProcAddress(hMod, "rocksdb_free");

    p_rocksdb_create_iterator = (fn_rocksdb_create_iterator_t)GetProcAddress(hMod, "rocksdb_create_iterator");
    p_rocksdb_iter_destroy = (fn_rocksdb_iter_destroy_t)GetProcAddress(hMod, "rocksdb_iter_destroy");
    p_rocksdb_iter_valid = (fn_rocksdb_iter_valid_t)GetProcAddress(hMod, "rocksdb_iter_valid");
    p_rocksdb_iter_seek_to_first = (fn_rocksdb_iter_seek_to_first_t)GetProcAddress(hMod, "rocksdb_iter_seek_to_first");
    p_rocksdb_iter_seek_to_last = (fn_rocksdb_iter_seek_to_last_t)GetProcAddress(hMod, "rocksdb_iter_seek_to_last");
    p_rocksdb_iter_seek = (fn_rocksdb_iter_seek_t)GetProcAddress(hMod, "rocksdb_iter_seek");
    p_rocksdb_iter_next = (fn_rocksdb_iter_next_t)GetProcAddress(hMod, "rocksdb_iter_next");
    p_rocksdb_iter_prev = (fn_rocksdb_iter_prev_t)GetProcAddress(hMod, "rocksdb_iter_prev");
    p_rocksdb_iter_key = (fn_rocksdb_iter_key_t)GetProcAddress(hMod, "rocksdb_iter_key");
    p_rocksdb_iter_value = (fn_rocksdb_iter_value_t)GetProcAddress(hMod, "rocksdb_iter_value");
    p_rocksdb_iter_get_error = (fn_rocksdb_iter_get_error_t)GetProcAddress(hMod, "rocksdb_iter_get_error");

    return (p_rocksdb_open && p_rocksdb_put && p_rocksdb_get && p_rocksdb_delete && p_rocksdb_close &&
            p_rocksdb_create_iterator && p_rocksdb_iter_destroy && p_rocksdb_iter_valid &&
            p_rocksdb_iter_seek_to_first && p_rocksdb_iter_seek_to_last && p_rocksdb_iter_seek &&
            p_rocksdb_iter_next && p_rocksdb_iter_prev && p_rocksdb_iter_key &&
            p_rocksdb_iter_value && p_rocksdb_iter_get_error);
}

static ERL_NIF_TERM nif_open(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    char path[1024];
    if (enif_get_string(env, argv[0], path, sizeof(path), ERL_NIF_LATIN1) <= 0) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "bad_path"));
    }

    rocksdb_options_t* opts = p_rocksdb_options_create();
    p_rocksdb_options_set_create_if_missing(opts, 1);

    char* err = NULL;
    rocksdb_t* db = p_rocksdb_open(opts, path, &err);
    p_rocksdb_options_destroy(opts);

    if (err != NULL) {
        ERL_NIF_TERM err_term = enif_make_string(env, err, ERL_NIF_LATIN1);
        p_rocksdb_free(err);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), err_term);
    }

    RocksdbHandle* h = (RocksdbHandle*)enif_alloc_resource(ROCKSDB_RES_TYPE, sizeof(RocksdbHandle));
    h->db = db;
    h->write_opts = p_rocksdb_writeoptions_create();
    h->read_opts = p_rocksdb_readoptions_create();

    ERL_NIF_TERM res_term = enif_make_resource(env, h);
    enif_release_resource(h);

    return enif_make_tuple2(env, enif_make_atom(env, "ok"), res_term);
}

static ERL_NIF_TERM nif_close(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbHandle* h = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_RES_TYPE, (void**)&h) || !h->db) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_handle"));
    }

    p_rocksdb_close(h->db);
    h->db = NULL;
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_put(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbHandle* h = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_RES_TYPE, (void**)&h) || !h->db) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_handle"));
    }

    ErlNifBinary key, val;
    if (!enif_inspect_binary(env, argv[1], &key) || !enif_inspect_binary(env, argv[2], &val)) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "bad_argument"));
    }

    char* err = NULL;
    p_rocksdb_put(h->db, h->write_opts, (const char*)key.data, key.size, (const char*)val.data, val.size, &err);

    if (err != NULL) {
        ERL_NIF_TERM err_term = enif_make_string(env, err, ERL_NIF_LATIN1);
        p_rocksdb_free(err);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), err_term);
    }

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_get(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbHandle* h = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_RES_TYPE, (void**)&h) || !h->db) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_handle"));
    }

    ErlNifBinary key;
    if (!enif_inspect_binary(env, argv[1], &key)) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "bad_argument"));
    }

    char* err = NULL;
    size_t val_len = 0;
    char* val = p_rocksdb_get(h->db, h->read_opts, (const char*)key.data, key.size, &val_len, &err);

    if (err != NULL) {
        ERL_NIF_TERM err_term = enif_make_string(env, err, ERL_NIF_LATIN1);
        p_rocksdb_free(err);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), err_term);
    }

    if (val == NULL) {
        return enif_make_atom(env, "not_found");
    }

    ErlNifBinary result_bin;
    enif_alloc_binary(val_len, &result_bin);
    memcpy(result_bin.data, val, val_len);
    p_rocksdb_free(val);

    return enif_make_tuple2(env, enif_make_atom(env, "ok"), enif_make_binary(env, &result_bin));
}

static ERL_NIF_TERM nif_delete(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbHandle* h = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_RES_TYPE, (void**)&h) || !h->db) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_handle"));
    }

    ErlNifBinary key;
    if (!enif_inspect_binary(env, argv[1], &key)) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "bad_argument"));
    }

    char* err = NULL;
    p_rocksdb_delete(h->db, h->write_opts, (const char*)key.data, key.size, &err);

    if (err != NULL) {
        ERL_NIF_TERM err_term = enif_make_string(env, err, ERL_NIF_LATIN1);
        p_rocksdb_free(err);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), err_term);
    }

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_get_all(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbHandle* h = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_RES_TYPE, (void**)&h) || !h->db) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_handle"));
    }

    rocksdb_iterator_t* iter = p_rocksdb_create_iterator(h->db, h->read_opts);
    if (!iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "iterator_creation_failed"));
    }

    p_rocksdb_iter_seek_to_first(iter);

    ERL_NIF_TERM list = enif_make_list(env, 0);

    while (p_rocksdb_iter_valid(iter)) {
        size_t klen = 0, vlen = 0;
        const char* k = p_rocksdb_iter_key(iter, &klen);
        const char* v = p_rocksdb_iter_value(iter, &vlen);

        ERL_NIF_TERM key_term, val_term;
        unsigned char* key_data = enif_make_new_binary(env, klen, &key_term);
        memcpy(key_data, k, klen);

        unsigned char* val_data = enif_make_new_binary(env, vlen, &val_term);
        memcpy(val_data, v, vlen);

        ERL_NIF_TERM tuple = enif_make_tuple2(env, key_term, val_term);
        list = enif_make_list_cell(env, tuple, list);

        p_rocksdb_iter_next(iter);
    }

    char* err = NULL;
    p_rocksdb_iter_get_error(iter, &err);
    p_rocksdb_iter_destroy(iter);

    if (err != NULL) {
        ERL_NIF_TERM err_term = enif_make_string(env, err, ERL_NIF_LATIN1);
        p_rocksdb_free(err);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), err_term);
    }

    ERL_NIF_TERM result;
    enif_make_reverse_list(env, list, &result);
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), result);
}

static ERL_NIF_TERM nif_iter_create(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbHandle* h = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_RES_TYPE, (void**)&h) || !h->db) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_handle"));
    }

    rocksdb_iterator_t* iter = p_rocksdb_create_iterator(h->db, h->read_opts);
    if (!iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "iterator_creation_failed"));
    }

    RocksdbIterHandle* ih = (RocksdbIterHandle*)enif_alloc_resource(ROCKSDB_ITER_RES_TYPE, sizeof(RocksdbIterHandle));
    ih->iter = iter;

    ERL_NIF_TERM res_term = enif_make_resource(env, ih);
    enif_release_resource(ih);

    return enif_make_tuple2(env, enif_make_atom(env, "ok"), res_term);
}

static ERL_NIF_TERM nif_iter_destroy(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }

    p_rocksdb_iter_destroy(ih->iter);
    ih->iter = NULL;
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_iter_seek_to_first(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }
    p_rocksdb_iter_seek_to_first(ih->iter);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_iter_seek_to_last(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }
    p_rocksdb_iter_seek_to_last(ih->iter);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_iter_seek(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }
    ErlNifBinary key;
    if (!enif_inspect_binary(env, argv[1], &key)) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "bad_argument"));
    }
    p_rocksdb_iter_seek(ih->iter, (const char*)key.data, key.size);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_iter_next(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }
    p_rocksdb_iter_next(ih->iter);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_iter_prev(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }
    p_rocksdb_iter_prev(ih->iter);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM nif_iter_valid(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }
    unsigned char valid = p_rocksdb_iter_valid(ih->iter);
    return enif_make_atom(env, valid ? "true" : "false");
}

static ERL_NIF_TERM nif_iter_current(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    (void)argc;
    RocksdbIterHandle* ih = NULL;
    if (!enif_get_resource(env, argv[0], ROCKSDB_ITER_RES_TYPE, (void**)&ih) || !ih->iter) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "invalid_iter_handle"));
    }

    if (!p_rocksdb_iter_valid(ih->iter)) {
        char* err = NULL;
        p_rocksdb_iter_get_error(ih->iter, &err);
        if (err != NULL) {
            ERL_NIF_TERM err_term = enif_make_string(env, err, ERL_NIF_LATIN1);
            p_rocksdb_free(err);
            return enif_make_tuple2(env, enif_make_atom(env, "error"), err_term);
        }
        return enif_make_atom(env, "invalid");
    }

    size_t klen = 0, vlen = 0;
    const char* k = p_rocksdb_iter_key(ih->iter, &klen);
    const char* v = p_rocksdb_iter_value(ih->iter, &vlen);

    ERL_NIF_TERM key_term, val_term;
    unsigned char* key_data = enif_make_new_binary(env, klen, &key_term);
    memcpy(key_data, k, klen);

    unsigned char* val_data = enif_make_new_binary(env, vlen, &val_term);
    memcpy(val_data, v, vlen);

    return enif_make_tuple2(env, enif_make_atom(env, "ok"), enif_make_tuple2(env, key_term, val_term));
}

static ErlNifFunc nif_funcs[] = {
    {"nif_open", 1, nif_open, 0},
    {"nif_close", 1, nif_close, 0},
    {"nif_put", 3, nif_put, 0},
    {"nif_get", 2, nif_get, 0},
    {"nif_delete", 2, nif_delete, 0},
    {"nif_get_all", 1, nif_get_all, 0},
    {"nif_iter_create", 1, nif_iter_create, 0},
    {"nif_iter_destroy", 1, nif_iter_destroy, 0},
    {"nif_iter_seek_to_first", 1, nif_iter_seek_to_first, 0},
    {"nif_iter_seek_to_last", 1, nif_iter_seek_to_last, 0},
    {"nif_iter_seek", 2, nif_iter_seek, 0},
    {"nif_iter_next", 1, nif_iter_next, 0},
    {"nif_iter_prev", 1, nif_iter_prev, 0},
    {"nif_iter_valid", 1, nif_iter_valid, 0},
    {"nif_iter_current", 1, nif_iter_current, 0}
};

static int load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info) {
    (void)priv_data;
    (void)load_info;

    if (!init_rocksdb_dll()) {
        return -1;
    }

    ROCKSDB_RES_TYPE = enif_open_resource_type(
        env,
        NULL,
        "RocksdbHandle",
        rocksdb_handle_destructor,
        ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
        NULL
    );

    ROCKSDB_ITER_RES_TYPE = enif_open_resource_type(
        env,
        NULL,
        "RocksdbIterHandle",
        rocksdb_iter_handle_destructor,
        ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
        NULL
    );

    return (ROCKSDB_RES_TYPE != NULL && ROCKSDB_ITER_RES_TYPE != NULL) ? 0 : -1;
}

ERL_NIF_INIT(Elixir.Rocksdb, nif_funcs, load, NULL, NULL, NULL)
