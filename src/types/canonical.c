#include "canonical.h"
#include "../module.h"
#include "../internal/alloc.h"
#include "../runtime.h"
#include "references.h"
#include "../values.h"
#include <string.h>

WASM_PRIVATE void wasm__free_functype(const wasm_allocator_t* a, wasm_functype_t* ft) {
    wasm__free(a, ft->params);
    wasm__free(a, ft->results);
    wasm__free(a, ft->param_reftypes);
    wasm__free(a, ft->result_reftypes);
    ft->params = NULL;
    ft->results = NULL;
    ft->param_reftypes = NULL;
    ft->result_reftypes = NULL;
    ft->num_params = 0;
    ft->num_results = 0;
}

WASM_PRIVATE int wasm__functype_is_unspecified(const wasm_functype_t* ft) {
    return ft && ft->num_params == UINT32_MAX && ft->num_results == UINT32_MAX;
}

WASM_PRIVATE int wasm__functype_is_zeroed(const wasm_functype_t* ft) {
    return ft && ft->num_params == 0 && ft->num_results == 0 &&
           ft->params == NULL && ft->results == NULL &&
           ft->param_reftypes == NULL && ft->result_reftypes == NULL;
}

WASM_PRIVATE void wasm__functype_mark_unspecified(const wasm_allocator_t* a, wasm_functype_t* ft) {
    wasm__free_functype(a, ft);
    ft->num_params = UINT32_MAX;
    ft->num_results = UINT32_MAX;
}

static void wasm__free_structtype(const wasm_allocator_t* a, wasm_structtype_t* st) {
    wasm__gc_struct_layout_free(a, st);
    wasm__free(a, st->fields);
    st->fields = NULL;
    st->num_fields = 0;
}

WASM_PRIVATE void wasm__free_comptype(const wasm_allocator_t* a, wasm_comptype_t* type) {
    wasm__free(a, type->supertypes);
    type->supertypes = NULL;
    type->num_supertypes = 0;
    wasm__free(a, type->display);
    type->display = NULL;
    type->depth = 0;

    switch (type->kind) {
        case WASM_COMP_FUNC:
            wasm__free_functype(a, &type->of.func);
            break;
        case WASM_COMP_STRUCT:
            wasm__free_structtype(a, &type->of.struct_);
            break;
        case WASM_COMP_ARRAY:
            break;
        default:
            break;
    }

    type->kind = WASM_COMP_FUNC;
    type->rec_group = 0;
    type->canonical_id = 0;
    type->is_final = 0;
}

WASM_PRIVATE wasm_error_t wasm__copy_functype(const wasm_allocator_t* a, wasm_functype_t* dst, const wasm_functype_t* src) {
    memset(dst, 0, sizeof(*dst));
    if (wasm__functype_is_unspecified(src)) {
        dst->num_params = UINT32_MAX;
        dst->num_results = UINT32_MAX;
        return WASM_OK;
    }
    dst->num_params = src->num_params;
    dst->num_results = src->num_results;

    if (src->num_params > 0) {
        dst->params = (wasm_valtype_t*)wasm__malloc_array(a, src->num_params, sizeof(wasm_valtype_t));
        if (dst->params) dst->param_reftypes = (wasm_reftype_t*)wasm__calloc(a, src->num_params, sizeof(wasm_reftype_t));
        if (!dst->params || !dst->param_reftypes) {
            wasm__free_functype(a, dst);
            return WASM_ERR_OOM;
        }
        if (src->params)
            memcpy(dst->params, src->params, src->num_params * sizeof(wasm_valtype_t));
        else
            memset(dst->params, 0, src->num_params * sizeof(wasm_valtype_t));
        if (src->param_reftypes)
            memcpy(dst->param_reftypes, src->param_reftypes, src->num_params * sizeof(wasm_reftype_t));
    }

    if (src->num_results > 0) {
        dst->results = (wasm_valtype_t*)wasm__malloc_array(a, src->num_results, sizeof(wasm_valtype_t));
        if (dst->results) dst->result_reftypes = (wasm_reftype_t*)wasm__calloc(a, src->num_results, sizeof(wasm_reftype_t));
        if (!dst->results || !dst->result_reftypes) {
            wasm__free_functype(a, dst);
            return WASM_ERR_OOM;
        }
        if (src->results)
            memcpy(dst->results, src->results, src->num_results * sizeof(wasm_valtype_t));
        else
            memset(dst->results, 0, src->num_results * sizeof(wasm_valtype_t));
        if (src->result_reftypes)
            memcpy(dst->result_reftypes, src->result_reftypes, src->num_results * sizeof(wasm_reftype_t));
    }

    return WASM_OK;
}

WASM_PRIVATE int wasm__functype_equal(const wasm_functype_t* lhs, const wasm_functype_t* rhs) {
    uint32_t i;

    if (lhs->num_params != rhs->num_params || lhs->num_results != rhs->num_results)
        return 0;
    for (i = 0; i < lhs->num_params; i++) {
        const wasm_reftype_t* lhs_ref = lhs->param_reftypes ? lhs->param_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->param_reftypes ? rhs->param_reftypes + i : NULL;
        if (lhs->params[i] != rhs->params[i]) return 0;
        if (wasm__is_ref_type(lhs->params[i]) || wasm__is_ref_type(rhs->params[i])) {
            if (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) {
                if (!wasm__reftype_equal(lhs_ref, rhs_ref)) return 0;
            }
        }
    }
    for (i = 0; i < lhs->num_results; i++) {
        const wasm_reftype_t* lhs_ref = lhs->result_reftypes ? lhs->result_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->result_reftypes ? rhs->result_reftypes + i : NULL;
        if (lhs->results[i] != rhs->results[i]) return 0;
        if (wasm__is_ref_type(lhs->results[i]) || wasm__is_ref_type(rhs->results[i])) {
            if (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) {
                if (!wasm__reftype_equal(lhs_ref, rhs_ref)) return 0;
            }
        }
    }
    return 1;
}

/* A rec group interned in a runtime; equal groups of every module in the runtime share one. */
struct wasm__type_group_t {
    struct wasm__type_group_t* next; /* next group in the same bucket */
    uint64_t hash;
    uint32_t refs;     /* rec groups of modules and type owners that are this group */
    uint32_t first_id; /* canonical id of the group's first type */
    uint32_t num_types;
    uint32_t num_words;
    uint32_t* words; /* the group's encoding, stored after the struct */
};

/* Words encoding a rec group, in which type indices are canonical ids or offsets within the group. */
typedef struct wasm__type_words_t {
    const wasm_allocator_t* allocator;
    uint32_t* words;
    uint32_t count;
    uint32_t capacity;
    int failed;
} wasm__type_words_t;

static void wasm__type_words_push(wasm__type_words_t* w, uint32_t word) {
    if (w->failed) return;
    if (w->count == w->capacity) {
        uint32_t capacity = w->capacity ? w->capacity * 2u : 64u;
        uint32_t* words;

        if (capacity < w->capacity) {
            w->failed = 1;
            return;
        }
        words = (uint32_t*)wasm__realloc_array(w->allocator, w->words, capacity, sizeof(*words));
        if (!words) {
            w->failed = 1;
            return;
        }
        w->words = words;
        w->capacity = capacity;
    }
    w->words[w->count++] = word;
}

static void wasm__encode_type_index(wasm__type_words_t* w, const wasm_module_t* mod, const wasm_recgroup_t* group,
                                    uint32_t type_idx) {
    if (type_idx >= group->first_type && type_idx - group->first_type < group->num_types) {
        wasm__type_words_push(w, 0u);
        wasm__type_words_push(w, type_idx - group->first_type);
    } else if (type_idx < mod->num_types && mod->types[type_idx].canonical_id != 0) {
        wasm__type_words_push(w, 1u);
        wasm__type_words_push(w, mod->types[type_idx].canonical_id);
    } else {
        wasm__type_words_push(w, 2u);
        wasm__type_words_push(w, type_idx);
    }
}

static void wasm__encode_clause(wasm__type_words_t* w, const wasm_module_t* mod, const wasm_recgroup_t* group,
                                uint32_t type_idx) {
    wasm__type_words_push(w, type_idx != UINT32_MAX);
    if (type_idx != UINT32_MAX) wasm__encode_type_index(w, mod, group, type_idx);
}

static void wasm__encode_valtype(wasm__type_words_t* w, const wasm_module_t* mod, const wasm_recgroup_t* group,
                                 wasm_valtype_t type, const wasm_reftype_t* ref) {
    if (!wasm__is_ref_type(type)) {
        wasm__type_words_push(w, (uint32_t)type);
        return;
    }
    if (!wasm__has_reftype_info(ref)) {
        wasm__type_words_push(w, 0x100u | 1u);
        wasm__type_words_push(w, (uint32_t)type);
        return;
    }
    wasm__type_words_push(w, 0x100u | (uint32_t)(ref->nullable != 0) | (uint32_t)(ref->exact != 0) << 1 |
                                 (uint32_t)(ref->has_type_index != 0) << 2);
    if (ref->has_type_index)
        wasm__encode_type_index(w, mod, group, ref->type_index);
    else
        wasm__type_words_push(w, (uint32_t)(ref->type ? ref->type : type));
}

static void wasm__encode_values(wasm__type_words_t* w, const wasm_module_t* mod, const wasm_recgroup_t* group,
                                uint32_t count, const wasm_valtype_t* types, const wasm_reftype_t* refs) {
    uint32_t i;

    wasm__type_words_push(w, count);
    for (i = 0; i < count; i++) wasm__encode_valtype(w, mod, group, types[i], refs ? refs + i : NULL);
}

static void wasm__encode_field(wasm__type_words_t* w, const wasm_module_t* mod, const wasm_recgroup_t* group,
                               const wasm_fieldtype_t* field) {
    wasm__type_words_push(w, (uint32_t)(field->is_mutable != 0));
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        wasm__type_words_push(w, 0x200u | (uint32_t)field->storage.of.packed_type);
        return;
    }
    wasm__encode_valtype(w, mod, group, field->storage.of.valtype, &field->storage.ref_type);
}

static void wasm__encode_recgroup(wasm__type_words_t* w, const wasm_module_t* mod, const wasm_recgroup_t* group) {
    uint32_t i;
    uint32_t j;

    w->count = 0;
    w->failed = 0;
    for (i = 0; i < group->num_types; i++) {
        const wasm_comptype_t* type = &mod->types[group->first_type + i];

        wasm__type_words_push(w, (uint32_t)type->kind);
        wasm__type_words_push(w, (uint32_t)(type->is_final != 0));
        wasm__type_words_push(w, type->num_supertypes);
        for (j = 0; j < type->num_supertypes; j++) wasm__encode_type_index(w, mod, group, type->supertypes[j]);
        wasm__encode_clause(w, mod, group, type->describes);
        wasm__encode_clause(w, mod, group, type->descriptor);
        switch (type->kind) {
            case WASM_COMP_FUNC:
                wasm__encode_values(w, mod, group, type->of.func.num_params, type->of.func.params,
                                    type->of.func.param_reftypes);
                wasm__encode_values(w, mod, group, type->of.func.num_results, type->of.func.results,
                                    type->of.func.result_reftypes);
                break;
            case WASM_COMP_STRUCT:
                wasm__type_words_push(w, type->of.struct_.num_fields);
                for (j = 0; j < type->of.struct_.num_fields; j++)
                    wasm__encode_field(w, mod, group, &type->of.struct_.fields[j]);
                break;
            case WASM_COMP_ARRAY:
                wasm__encode_field(w, mod, group, &type->of.array.field);
                break;
            case WASM_COMP_CONT:
                wasm__encode_type_index(w, mod, group, type->of.cont.func_type);
                break;
            default:
                break;
        }
    }
}

static uint64_t wasm__hash_words(const uint32_t* words, uint32_t count) {
    uint64_t hash = UINT64_C(0xCBF29CE484222325);
    uint32_t i;

    for (i = 0; i < count; i++) {
        hash ^= words[i];
        hash *= UINT64_C(0x100000001B3);
    }
    return hash ^ (hash >> 29);
}

static wasm_error_t wasm__type_groups_grow(wasm_runtime_t* rt) {
    uint32_t capacity = rt->type_group_buckets ? rt->type_group_buckets * 2u : 64u;
    wasm__type_group_t** buckets;
    uint32_t i;

    if (capacity < rt->type_group_buckets) return WASM_ERR_OOM;
    buckets = (wasm__type_group_t**)wasm__calloc(&rt->allocator, capacity, sizeof(*buckets));
    if (!buckets) return WASM_ERR_OOM;
    for (i = 0; i < rt->type_group_buckets; i++) {
        wasm__type_group_t* group = rt->type_groups[i];

        while (group) {
            wasm__type_group_t* next = group->next;
            uint32_t slot = (uint32_t)group->hash & (capacity - 1u);

            group->next = buckets[slot];
            buckets[slot] = group;
            group = next;
        }
    }
    wasm__free(&rt->allocator, rt->type_groups);
    rt->type_groups = buckets;
    rt->type_group_buckets = capacity;
    return WASM_OK;
}

static wasm_error_t wasm__intern_type_group(wasm_runtime_t* rt, const wasm__type_words_t* w, uint32_t num_types,
                                            wasm__type_group_t** out_group) {
    uint64_t hash = wasm__hash_words(w->words, w->count);
    wasm__type_group_t* group;
    uint32_t slot;

    if (rt->type_group_buckets) {
        for (group = rt->type_groups[(uint32_t)hash & (rt->type_group_buckets - 1u)]; group; group = group->next) {
            if (group->hash == hash && group->num_types == num_types && group->num_words == w->count &&
                memcmp(group->words, w->words, (size_t)w->count * sizeof(*w->words)) == 0) {
                if (group->refs == UINT32_MAX) return WASM_ERR_OOM;
                group->refs++;
                *out_group = group;
                return WASM_OK;
            }
        }
    }
    if (rt->next_type_id == 0) rt->next_type_id = 1;
    if (num_types > UINT32_MAX - rt->next_type_id) return WASM_ERR_OOM;
    if (rt->num_type_groups >= rt->type_group_buckets && wasm__type_groups_grow(rt) != WASM_OK) return WASM_ERR_OOM;
    group = (wasm__type_group_t*)wasm__malloc(&rt->allocator, sizeof(*group) + (size_t)w->count * sizeof(*w->words));
    if (!group) return WASM_ERR_OOM;
    group->hash = hash;
    group->refs = 1;
    group->first_id = rt->next_type_id;
    group->num_types = num_types;
    group->num_words = w->count;
    group->words = (uint32_t*)(group + 1);
    if (w->count) memcpy(group->words, w->words, (size_t)w->count * sizeof(*w->words));
    rt->next_type_id += num_types;
    slot = (uint32_t)hash & (rt->type_group_buckets - 1u);
    group->next = rt->type_groups[slot];
    rt->type_groups[slot] = group;
    rt->num_type_groups++;
    *out_group = group;
    return WASM_OK;
}

WASM_PRIVATE void wasm__release_type_groups(wasm_runtime_t* rt, wasm_recgroup_t* groups, uint32_t count) {
    uint32_t i;

    for (i = 0; i < count; i++) {
        wasm__type_group_t* group = groups[i].canonical;
        wasm__type_group_t** link;

        if (!group) continue;
        groups[i].canonical = NULL;
        if (--group->refs != 0) continue;
        for (link = &rt->type_groups[(uint32_t)group->hash & (rt->type_group_buckets - 1u)]; *link;
             link = &(*link)->next) {
            if (*link != group) continue;
            *link = group->next;
            break;
        }
        rt->num_type_groups--;
        wasm__free(&rt->allocator, group);
    }
}

WASM_PRIVATE void wasm__free_type_groups(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->type_group_buckets; i++) {
        while (rt->type_groups[i]) {
            wasm__type_group_t* next = rt->type_groups[i]->next;

            wasm__free(&rt->allocator, rt->type_groups[i]);
            rt->type_groups[i] = next;
        }
    }
    wasm__free(&rt->allocator, rt->type_groups);
    rt->type_groups = NULL;
    rt->type_group_buckets = 0;
    rt->num_type_groups = 0;
}

static wasm_error_t wasm__init_displays(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_types; i++) {
        wasm_comptype_t* type = &mod->types[i];
        const wasm_comptype_t* parent = NULL;
        uint32_t depth = 0;

        if (type->display || type->canonical_id == 0 || type->num_supertypes > 1) continue;
        if (type->num_supertypes == 1) {
            if (type->supertypes[0] >= i) continue;
            parent = &mod->types[type->supertypes[0]];
            if (!parent->display || parent->depth >= WASM__MAX_DISPLAY_DEPTH) continue;
            depth = parent->depth + 1u;
        }
        type->display = (uint32_t*)wasm__malloc_array(&mod->rt->allocator, (size_t)depth + 1u, sizeof(uint32_t));
        if (!type->display) return WASM_ERR_OOM;
        if (parent) memcpy(type->display, parent->display, (size_t)depth * sizeof(uint32_t));
        type->display[depth] = type->canonical_id;
        type->depth = depth;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__canonicalize_types(wasm_module_t* mod) {
    wasm__type_words_t words;
    wasm_error_t err = WASM_OK;
    uint32_t i;

    if (!mod) return WASM_OK;
    memset(&words, 0, sizeof(words));
    words.allocator = &mod->rt->allocator;
    for (i = 0; i < mod->num_rec_groups && err == WASM_OK; i++) {
        wasm_recgroup_t* group = &mod->rec_groups[i];
        uint32_t k;

        if (group->canonical) continue;
        if (group->first_type > mod->num_types || group->num_types > mod->num_types - group->first_type) continue;
        wasm__encode_recgroup(&words, mod, group);
        if (words.failed) {
            err = WASM_ERR_OOM;
            break;
        }
        err = wasm__intern_type_group(mod->rt, &words, group->num_types, &group->canonical);
        if (err != WASM_OK) break;
        for (k = 0; k < group->num_types; k++)
            mod->types[group->first_type + k].canonical_id = group->canonical->first_id + k;
    }
    wasm__free(words.allocator, words.words);
    if (err != WASM_OK) return err;
    return wasm__init_displays(mod);
}

WASM_PRIVATE int wasm__type_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx) {
    uint32_t lhs_id;

    if (!mod || lhs_idx >= mod->num_types || rhs_idx >= mod->num_types) return 0;
    if (lhs_idx == rhs_idx) return 1;
    lhs_id = mod->types[lhs_idx].canonical_id;
    return lhs_id != 0 && lhs_id == mod->types[rhs_idx].canonical_id;
}

WASM_PRIVATE int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype) {
    (void)mod;

    if (subtype == supertype) return 1;

    switch (subtype) {
        case WASM_TYPE_NONE:
            return supertype == WASM_TYPE_I31REF || supertype == WASM_TYPE_STRUCTREF ||
                   supertype == WASM_TYPE_ARRAYREF || supertype == WASM_TYPE_EQREF ||
                   supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return supertype == WASM_TYPE_EQREF || supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_EQREF:
            return supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_NOEXN:
            return supertype == WASM_TYPE_EXNREF;
        case WASM_TYPE_NOCONT:
            return supertype == WASM_TYPE_CONTREF;
        case WASM_TYPE_NOFUNC:
            return supertype == WASM_TYPE_FUNCREF;
        case WASM_TYPE_NOEXTERN:
            return supertype == WASM_TYPE_EXTERNREF;
        default:
            return 0;
    }
}

static int wasm__is_reftype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype) {
    if (!wasm__is_ref_type(subtype) || !wasm__is_ref_type(supertype)) return 0;
    return wasm__is_heap_subtype(mod, subtype, supertype);
}

WASM_PRIVATE int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype) {
    if (subtype == supertype) return 1;
    if (wasm__is_ref_type(subtype) && wasm__is_ref_type(supertype))
        return wasm__is_reftype_subtype(mod, subtype, supertype);
    return 0;
}

WASM_PRIVATE int wasm__is_storagetype_subtype(const wasm_module_t* mod,
                                        const wasm_storagetype_t* subtype,
                                        const wasm_storagetype_t* supertype) {
    if (subtype->kind != supertype->kind) return 0;
    if (subtype->kind == WASM_STORAGE_PACKED)
        return subtype->of.packed_type == supertype->of.packed_type;
    if ((wasm__has_reftype_info(&subtype->ref_type) || wasm__has_reftype_info(&supertype->ref_type)) &&
        wasm__is_ref_type(subtype->of.valtype) && wasm__is_ref_type(supertype->of.valtype))
        return wasm__reftype_is_subtype(mod, &subtype->ref_type, &supertype->ref_type);
    return wasm__is_valtype_subtype(mod, subtype->of.valtype, supertype->of.valtype);
}

static int wasm__is_fieldtype_subtype(const wasm_module_t* mod,
                                      const wasm_fieldtype_t* subtype,
                                      const wasm_fieldtype_t* supertype) {
    if (subtype->is_mutable != supertype->is_mutable) return 0;
    if (subtype->is_mutable)
        return wasm__is_storagetype_subtype(mod, &subtype->storage, &supertype->storage) &&
               wasm__is_storagetype_subtype(mod, &supertype->storage, &subtype->storage);
    return wasm__is_storagetype_subtype(mod, &subtype->storage, &supertype->storage);
}

WASM_PRIVATE int wasm__is_comptype_immediate_subtype(const wasm_module_t* mod,
                                               const wasm_comptype_t* subtype,
                                               const wasm_comptype_t* supertype) {
    uint32_t i;

    if (subtype->kind != supertype->kind) return 0;

    switch (subtype->kind) {
        case WASM_COMP_FUNC:
            if (subtype->of.func.num_params != supertype->of.func.num_params ||
                subtype->of.func.num_results != supertype->of.func.num_results)
                return 0;
            return wasm__functype_is_subtype(mod, &subtype->of.func, &supertype->of.func);
        case WASM_COMP_STRUCT:
            if (subtype->of.struct_.num_fields < supertype->of.struct_.num_fields) return 0;
            for (i = 0; i < supertype->of.struct_.num_fields; i++) {
                if (!wasm__is_fieldtype_subtype(mod,
                                                &subtype->of.struct_.fields[i],
                                                &supertype->of.struct_.fields[i]))
                    return 0;
            }
            return 1;
        case WASM_COMP_ARRAY:
            return wasm__is_fieldtype_subtype(mod,
                                              &subtype->of.array.field,
                                              &supertype->of.array.field);
        case WASM_COMP_CONT:
            return wasm__is_subtype(mod, subtype->of.cont.func_type, supertype->of.cont.func_type);
        default:
            return 0;
    }
}

WASM_PRIVATE_INLINE int wasm__is_subtype(const wasm_module_t* mod,
                                   uint32_t subtype_idx,
                                   uint32_t supertype_idx) {
    const wasm_comptype_t* subtype;
    uint32_t i;

    if (!mod || subtype_idx >= mod->num_types || supertype_idx >= mod->num_types) return 0;
    if (wasm__type_equal(mod, subtype_idx, supertype_idx)) return 1;

    subtype = &mod->types[subtype_idx];
    for (i = 0; i < subtype->num_supertypes; i++) {
        uint32_t parent_idx = subtype->supertypes[i];

        if (parent_idx >= mod->num_types) continue;
        if (wasm__is_subtype(mod, parent_idx, supertype_idx)) return 1;
    }

    return 0;
}

static wasm_functype_t* wasm__type_as_functype(wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_FUNC) return NULL;
    return &type->of.func;
}

static const wasm_functype_t* wasm__type_as_const_functype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_FUNC) return NULL;
    return &type->of.func;
}

WASM_PRIVATE wasm_functype_t* wasm__module_functype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_functype(&mod->types[type_idx]);
}

WASM_PRIVATE const wasm_functype_t* wasm__module_const_functype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_functype(&mod->types[type_idx]);
}

static const wasm_structtype_t* wasm__type_as_const_structtype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_STRUCT) return NULL;
    return &type->of.struct_;
}

static const wasm_arraytype_t* wasm__type_as_const_arraytype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_ARRAY) return NULL;
    return &type->of.array;
}

WASM_PRIVATE const wasm_structtype_t* wasm__module_const_structtype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_structtype(&mod->types[type_idx]);
}

WASM_PRIVATE const wasm_arraytype_t* wasm__module_const_arraytype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_arraytype(&mod->types[type_idx]);
}
