#include <windows.h>
#include <erl_nif.h>
#include <stdlib.h>
#include <string.h>

typedef struct rocksdb_t rocksdb_t;
typedef struct rocksdb_options_t rocksdb_options_t;
typedef struct rocksdb_writeoptions_t rocksdb_writeoptions_t;
typedef struct rocksdb_readoptions_t rocksdb_readoptions_t;

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

typedef struct {
    rocksdb_t* db;
    rocksdb_writeoptions_t* write_opts;
    rocksdb_readoptions_t* read_opts;
} RocksdbHandle;

static ErlNifResourceType* ROCKSDB_RES_TYPE = NULL;

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

    return (p_rocksdb_open && p_rocksdb_put && p_rocksdb_get && p_rocksdb_delete && p_rocksdb_close);
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

static ErlNifFunc nif_funcs[] = {
    {"nif_open", 1, nif_open, 0},
    {"nif_close", 1, nif_close, 0},
    {"nif_put", 3, nif_put, 0},
    {"nif_get", 2, nif_get, 0},
    {"nif_delete", 2, nif_delete, 0}
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

    return ROCKSDB_RES_TYPE == NULL ? -1 : 0;
}

ERL_NIF_INIT(Elixir.Rocksdb, nif_funcs, load, NULL, NULL, NULL)
