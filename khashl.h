/* The MIT License

   Copyright (c) 2019-2023 by Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef __AC_KHASHL_H
#define __AC_KHASHL_H

#include "hash.h"

#define AC_VERSION_KHASHL_H "0.2"

typedef uint32_t khint32_t;
typedef uint64_t khint64_t;

typedef khint32_t khint_t;
typedef khint_t khiter_t;

#define kh_inline inline /* portably handled elsewhere */
#define KH_LOCAL static kh_inline MAYBE_UNUSED

#ifndef kcalloc
#define kcalloc(N,Z) xcalloc(N,Z)
#endif
#ifndef kfree
#define kfree(P) free(P)
#endif

/****************************
 * Simple private functions *
 ****************************/

#define __kh_used(flag, i)       (flag[i>>5] >> (i&0x1fU) & 1U)
#define __kh_set_used(flag, i)   (flag[i>>5] |= 1U<<(i&0x1fU))
#define __kh_set_unused(flag, i) (flag[i>>5] &= ~(1U<<(i&0x1fU)))

#define __kh_fsize(m) ((m) < 32? 1 : (m)>>5)

static kh_inline khint_t __kh_h2b(khint_t hash, khint_t bits) { return hash * 2654435769U >> (32 - bits); }

/*******************
 * Hash table base *
 *******************/

#define __KHASHL_TYPE(HType, khkey_t) \
	typedef struct HType { \
		khint_t bits, count; \
		khint32_t *used; \
		khkey_t *keys; \
	} HType;

#define __KHASHL_PROTOTYPES(HType, prefix, khkey_t) \
	extern HType *prefix##_init(void); \
	extern void prefix##_destroy(HType *h); \
	extern void prefix##_clear(HType *h); \
	extern khint_t prefix##_getp(const HType *h, const khkey_t *key); \
	extern void prefix##_resize(HType *h, khint_t new_n_buckets); \
	extern khint_t prefix##_putp(HType *h, const khkey_t *key, int *absent); \
	extern void prefix##_del(HType *h, khint_t k);

#define __KHASHL_IMPL_BASIC(SCOPE, HType, prefix) \
	SCOPE HType *prefix##_init(void) { \
		return (HType*)kcalloc(1, sizeof(HType)); \
	} \
	SCOPE void prefix##_release(HType *h) { \
		kfree((void *)h->keys); kfree(h->used); \
	} \
	SCOPE void prefix##_destroy(HType *h) { \
		if (!h) return; \
		prefix##_release(h); \
		kfree(h); \
	} \
	SCOPE void prefix##_clear(HType *h) { \
		if (h && h->used) { \
			khint_t n_buckets = (khint_t)1U << h->bits; \
			memset(h->used, 0, __kh_fsize(n_buckets) * sizeof(khint32_t)); \
			h->count = 0; \
		} \
	}

#define __KHASHL_IMPL_GET(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	SCOPE khint_t prefix##_getp_core(const HType *h, const khkey_t *key, khint_t hash) { \
		khint_t i, last, n_buckets, mask; \
		if (!h->keys) return 0; \
		n_buckets = (khint_t)1U << h->bits; \
		mask = n_buckets - 1U; \
		i = last = __kh_h2b(hash, h->bits); \
		while (__kh_used(h->used, i) && !__hash_eq(h->keys[i], *key)) { \
			i = (i + 1U) & mask; \
			if (i == last) return n_buckets; \
		} \
		return !__kh_used(h->used, i)? n_buckets : i; \
	} \
	SCOPE khint_t prefix##_getp(const HType *h, const khkey_t *key) { return prefix##_getp_core(h, key, __hash_fn(*key)); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { return prefix##_getp_core(h, &key, __hash_fn(key)); }

#define __KHASHL_IMPL_RESIZE(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	SCOPE void prefix##_resize(HType *h, khint_t new_n_buckets) { \
		khint32_t *new_used = NULL; \
		khint_t j = 0, x = new_n_buckets, n_buckets, new_bits, new_mask; \
		while ((x >>= 1) != 0) ++j; \
		if (new_n_buckets & (new_n_buckets - 1)) ++j; \
		new_bits = j > 2? j : 2; \
		new_n_buckets = (khint_t)1U << new_bits; \
		if (h->count > (new_n_buckets>>1) + (new_n_buckets>>2)) return; /* noop, requested size is too small */ \
		new_used = (khint32_t*)kcalloc(__kh_fsize(new_n_buckets), sizeof(khint32_t)); \
		n_buckets = h->keys? (khint_t)1U<<h->bits : 0U; \
		if (n_buckets < new_n_buckets) { /* expand */ \
			REALLOC_ARRAY(h->keys, new_n_buckets); \
		} /* otherwise shrink */ \
		new_mask = new_n_buckets - 1; \
		for (j = 0; j != n_buckets; ++j) { \
			khkey_t key; \
			if (!__kh_used(h->used, j)) continue; \
			key = h->keys[j]; \
			__kh_set_unused(h->used, j); \
			while (1) { /* kick-out process; sort of like in Cuckoo hashing */ \
				khint_t i; \
				i = __kh_h2b(__hash_fn(key), new_bits); \
				while (__kh_used(new_used, i)) i = (i + 1) & new_mask; \
				__kh_set_used(new_used, i); \
				if (i < n_buckets && __kh_used(h->used, i)) { /* kick out the existing element */ \
					{ khkey_t tmp = h->keys[i]; h->keys[i] = key; key = tmp; } \
					__kh_set_unused(h->used, i); /* mark it as deleted in the old hash table */ \
				} else { /* write the element and jump out of the loop */ \
					h->keys[i] = key; \
					break; \
				} \
			} \
		} \
		if (n_buckets > new_n_buckets) /* shrink the hash table */ \
			REALLOC_ARRAY(h->keys, new_n_buckets); \
		kfree(h->used); /* free the working space */ \
		h->used = new_used, h->bits = new_bits; \
	}

#define __KHASHL_IMPL_PUT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	SCOPE khint_t prefix##_putp_core(HType *h, const khkey_t *key, khint_t hash, int *absent) { \
		khint_t n_buckets, i, last, mask; \
		n_buckets = h->keys? (khint_t)1U<<h->bits : 0U; \
		*absent = -1; \
		if (h->count >= (n_buckets>>1) + (n_buckets>>2)) { /* rehashing */ \
			prefix##_resize(h, n_buckets + 1U); \
			n_buckets = (khint_t)1U<<h->bits; \
		} /* TODO: to implement automatically shrinking; resize() already support shrinking */ \
		mask = n_buckets - 1; \
		i = last = __kh_h2b(hash, h->bits); \
		while (__kh_used(h->used, i) && !__hash_eq(h->keys[i], *key)) { \
			i = (i + 1U) & mask; \
			if (i == last) break; \
		} \
		if (!__kh_used(h->used, i)) { /* not present at all */ \
			h->keys[i] = *key; \
			__kh_set_used(h->used, i); \
			++h->count; \
			*absent = 1; \
		} else *absent = 0; /* Don't touch h->keys[i] if present */ \
		return i; \
	} \
	SCOPE khint_t prefix##_putp(HType *h, const khkey_t *key, int *absent) { return prefix##_putp_core(h, key, __hash_fn(*key), absent); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { return prefix##_putp_core(h, &key, __hash_fn(key), absent); }

#define __KHASHL_IMPL_DEL(SCOPE, HType, prefix, khkey_t, __hash_fn) \
	SCOPE int prefix##_del(HType *h, khint_t i) { \
		khint_t j = i, k, mask, n_buckets; \
		if (!h->keys) return 0; \
		n_buckets = (khint_t)1U<<h->bits; \
		mask = n_buckets - 1U; \
		while (1) { \
			j = (j + 1U) & mask; \
			if (j == i || !__kh_used(h->used, j)) break; /* j==i only when the table is completely full */ \
			k = __kh_h2b(__hash_fn(h->keys[j]), h->bits); \
			if ((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j))) \
				h->keys[i] = h->keys[j], i = j; \
		} \
		__kh_set_unused(h->used, i); \
		--h->count; \
		return 1; \
	}

#define KHASHL_DECLARE(HType, prefix, khkey_t) \
	__KHASHL_TYPE(HType, khkey_t) \
	__KHASHL_PROTOTYPES(HType, prefix, khkey_t)

/* compatibility wrappers to make khash -> khashl migration easier */
#define __KHASH_COMPAT(SCOPE, HType, prefix, khkey_t) \
	typedef HType HType##_t; \
	SCOPE HType *kh_init_##prefix(void) { return prefix##_init(); } \
	SCOPE void kh_release_##prefix(HType *h) { prefix##_release(h); } \
	SCOPE void kh_destroy_##prefix(HType *h) { prefix##_destroy(h); } \
	SCOPE void kh_clear_##prefix(HType *h) { prefix##_clear(h); } \
	SCOPE khint_t kh_get_##prefix(const HType *h, khkey_t key) { \
		return prefix##_get(h, key); \
	} \
	SCOPE void kh_resize_##prefix(HType *h, khint_t new_n_buckets) { \
		prefix##_resize(h, new_n_buckets); \
	} \
	SCOPE khint_t kh_put_##prefix(HType *h, khkey_t key, int *absent) { \
		return prefix##_put(h, key, absent); \
	} \
	SCOPE int kh_del_##prefix(HType *h, khint_t i) { \
		return prefix##_del(h, i); \
	}

#define KHASHL_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_TYPE(HType, khkey_t) \
	__KHASHL_IMPL_BASIC(SCOPE, HType, prefix) \
	__KHASHL_IMPL_GET(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_IMPL_RESIZE(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_IMPL_PUT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_IMPL_DEL(SCOPE, HType, prefix, khkey_t, __hash_fn)

/***************************
 * Ensemble of hash tables *
 ***************************/

typedef struct {
	khint_t sub, pos;
} kh_ensitr_t;

#define KHASHE_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	KHASHL_INIT(KH_LOCAL, HType##_sub, prefix##_sub, khkey_t, __hash_fn, __hash_eq) \
	typedef struct HType { \
		khint64_t count:54, bits:8; \
		HType##_sub *sub; \
	} HType; \
	SCOPE HType *prefix##_init(int bits) { \
		HType *g; \
		g = (HType*)kcalloc(1, sizeof(*g)); \
		g->bits = bits; \
		g->sub = (HType##_sub*)kcalloc(1U<<bits, sizeof(*g->sub)); \
		return g; \
	} \
	SCOPE void prefix##_destroy(HType *g) { \
		int t; \
		if (!g) return; \
		for (t = 0; t < 1<<g->bits; ++t) { kfree((void*)g->sub[t].keys); kfree(g->sub[t].used); } \
		kfree(g->sub); kfree(g); \
	} \
	SCOPE kh_ensitr_t prefix##_getp(const HType *g, const khkey_t *key) { \
		khint_t hash, low, ret; \
		kh_ensitr_t r; \
		HType##_sub *h; \
		hash = __hash_fn(*key); \
		low = hash & ((1U<<g->bits) - 1); \
		h = &g->sub[low]; \
		ret = prefix##_sub_getp_core(h, key, hash); \
		if (ret >= kh_end(h)) r.sub = low, r.pos = (khint_t)-1; \
		else r.sub = low, r.pos = ret; \
		return r; \
	} \
	SCOPE kh_ensitr_t prefix##_get(const HType *g, const khkey_t key) { return prefix##_getp(g, &key); } \
	SCOPE kh_ensitr_t prefix##_putp(HType *g, const khkey_t *key, int *absent) { \
		khint_t hash, low, ret; \
		kh_ensitr_t r; \
		HType##_sub *h; \
		hash = __hash_fn(*key); \
		low = hash & ((1U<<g->bits) - 1); \
		h = &g->sub[low]; \
		ret = prefix##_sub_putp_core(h, key, hash, absent); \
		if (*absent) ++g->count; \
		if (ret == 1U<<h->bits) r.sub = low, r.pos = (khint_t)-1; \
		else r.sub = low, r.pos = ret; \
		return r; \
	} \
	SCOPE kh_ensitr_t prefix##_put(HType *g, const khkey_t key, int *absent) { return prefix##_putp(g, &key, absent); } \
	SCOPE int prefix##_del(HType *g, kh_ensitr_t itr) { \
		HType##_sub *h = &g->sub[itr.sub]; \
		int ret; \
		ret = prefix##_sub_del(h, itr.pos); \
		if (ret) --g->count; \
		return ret; \
	}

/*****************************
 * More convenient interface *
 *****************************/

#define __kh_packed /* noop, we use -Werror=address-of-packed-member */
#define __kh_cached_hash(x) ((x).hash)

#define KHASHL_SET_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; } __kh_packed HType##_s_bucket_t; \
	static kh_inline khint_t prefix##_s_hash(HType##_s_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_s_eq(HType##_s_bucket_t x, HType##_s_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_s, HType##_s_bucket_t, prefix##_s_hash, prefix##_s_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_s_init(); } \
	SCOPE void prefix##_release(HType *h) { prefix##_s_release(h); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_s_destroy(h); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_s_clear(h); } \
	SCOPE void prefix##_resize(HType *h, khint_t new_n_buckets) { prefix##_s_resize(h, new_n_buckets); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_s_bucket_t t; t.key = key; return prefix##_s_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_s_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_s_bucket_t t; t.key = key; return prefix##_s_putp(h, &t, absent); } \
	__KHASH_COMPAT(SCOPE, HType, prefix, khkey_t)

#define KHASHL_MAP_INIT(SCOPE, HType, prefix, khkey_t, kh_val_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; kh_val_t val; } __kh_packed HType##_m_bucket_t; \
	static kh_inline khint_t prefix##_m_hash(HType##_m_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_m_eq(HType##_m_bucket_t x, HType##_m_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_m, HType##_m_bucket_t, prefix##_m_hash, prefix##_m_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_m_init(); } \
	SCOPE void prefix##_release(HType *h) { prefix##_m_release(h); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_m_destroy(h); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_m_clear(h); } \
	SCOPE void prefix##_resize(HType *h, khint_t new_n_buckets) { prefix##_m_resize(h, new_n_buckets); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_m_bucket_t t; t.key = key; return prefix##_m_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_m_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_m_bucket_t t; t.key = key; return prefix##_m_putp(h, &t, absent); } \
	__KHASH_COMPAT(SCOPE, HType, prefix, khkey_t)

#define KHASHL_CSET_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; khint_t hash; } __kh_packed HType##_cs_bucket_t; \
	static kh_inline int prefix##_cs_eq(HType##_cs_bucket_t x, HType##_cs_bucket_t y) { return x.hash == y.hash && __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_cs, HType##_cs_bucket_t, __kh_cached_hash, prefix##_cs_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_cs_init(); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_cs_destroy(h); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_cs_bucket_t t; t.key = key; t.hash = __hash_fn(key); return prefix##_cs_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_cs_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_cs_bucket_t t; t.key = key, t.hash = __hash_fn(key); return prefix##_cs_putp(h, &t, absent); }

#define KHASHL_CMAP_INIT(SCOPE, HType, prefix, khkey_t, kh_val_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; kh_val_t val; khint_t hash; } __kh_packed HType##_cm_bucket_t; \
	static kh_inline int prefix##_cm_eq(HType##_cm_bucket_t x, HType##_cm_bucket_t y) { return x.hash == y.hash && __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_cm, HType##_cm_bucket_t, __kh_cached_hash, prefix##_cm_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_cm_init(); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_cm_destroy(h); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_cm_bucket_t t; t.key = key; t.hash = __hash_fn(key); return prefix##_cm_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_cm_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_cm_bucket_t t; t.key = key, t.hash = __hash_fn(key); return prefix##_cm_putp(h, &t, absent); }

#define KHASHE_MAP_INIT(SCOPE, HType, prefix, khkey_t, kh_val_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; kh_val_t val; } __kh_packed HType##_m_bucket_t; \
	static kh_inline khint_t prefix##_m_hash(HType##_m_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_m_eq(HType##_m_bucket_t x, HType##_m_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHE_INIT(KH_LOCAL, HType, prefix##_m, HType##_m_bucket_t, prefix##_m_hash, prefix##_m_eq) \
	SCOPE HType *prefix##_init(int bits) { return prefix##_m_init(bits); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_m_destroy(h); } \
	SCOPE kh_ensitr_t prefix##_get(const HType *h, khkey_t key) { HType##_m_bucket_t t; t.key = key; return prefix##_m_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, kh_ensitr_t k) { return prefix##_m_del(h, k); } \
	SCOPE kh_ensitr_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_m_bucket_t t; t.key = key; return prefix##_m_putp(h, &t, absent); }

/**************************
 * Public macro functions *
 **************************/

#define kh_bucket(h, x) ((h)->keys[x])

/*! @function
  @abstract     Get the number of elements in the hash table
  @param  h     Pointer to the hash table
  @return       Number of elements in the hash table [khint_t]
 */
#define kh_size(h) ((h)->count)

#define kh_capacity(h) ((h)->keys? 1U<<(h)->bits : 0U)

/*! @function
  @abstract     Get the end iterator
  @param  h     Pointer to the hash table
  @return       The end iterator [khint_t]
 */
#define kh_end(h) kh_capacity(h)

/*! @function
  @abstract     Get key given an iterator
  @param  h     Pointer to the hash table
  @param  x     Iterator to the bucket [khint_t]
  @return       Key [type of keys]
 */
#define kh_key(h, x) ((h)->keys[x].key)

/*! @function
  @abstract     Get value given an iterator
  @param  h     Pointer to the hash table
  @param  x     Iterator to the bucket [khint_t]
  @return       Value [type of values]
  @discussion   For hash sets, calling this results in segfault.
 */
#define kh_val(h, x) ((h)->keys[x].val)

/*! @function
  @abstract     Alias of kh_val()
 */
#define kh_value(h, x) kh_val(h, x)

/*! @function
  @abstract     Test whether a bucket contains data.
  @param  h     Pointer to the hash table
  @param  x     Iterator to the bucket [khint_t]
  @return       1 if containing data; 0 otherwise [int]
 */
#define kh_exist(h, x) __kh_used((h)->used, (x))

#define kh_ens_key(g, x) kh_key(&(g)->sub[(x).sub], (x).pos)
#define kh_ens_val(g, x) kh_val(&(g)->sub[(x).sub], (x).pos)
#define kh_ens_exist(g, x) kh_exist(&(g)->sub[(x).sub], (x).pos)
#define kh_ens_is_end(x) ((x).pos == (khint_t)-1)
#define kh_ens_size(g) ((g)->count)

/**************************************
 * Common hash and equality functions *
 **************************************/

#define kh_eq_generic(a, b) ((a) == (b))
#define kh_eq_str(a, b) (strcmp((a), (b)) == 0)
#define kh_hash_dummy(x) ((khint_t)(x))

static kh_inline khint_t kh_hash_uint32(khint_t key) {
	key += ~(key << 15);
	key ^=  (key >> 10);
	key +=  (key << 3);
	key ^=  (key >> 6);
	key += ~(key << 11);
	key ^=  (key >> 16);
	return key;
}

static kh_inline khint_t kh_hash_uint64(khint64_t key) {
	key = ~key + (key << 21);
	key = key ^ key >> 24;
	key = (key + (key << 3)) + (key << 8);
	key = key ^ key >> 14;
	key = (key + (key << 2)) + (key << 4);
	key = key ^ key >> 28;
	key = key + (key << 31);
	return (khint_t)key;
}

#define KH_FNV_SEED 11

static kh_inline khint_t kh_hash_str(const char *s) { /* FNV1a */
	khint_t h = KH_FNV_SEED ^ 2166136261U;
	const unsigned char *t = (const unsigned char*)s;
	for (; *t; ++t)
		h ^= *t, h *= 16777619;
	return h;
}

static kh_inline khint_t kh_hash_bytes(int len, const unsigned char *s) {
	khint_t h = KH_FNV_SEED ^ 2166136261U;
	int i;
	for (i = 0; i < len; ++i)
		h ^= s[i], h *= 16777619;
	return h;
}

/*! @function
  @abstract     Get the start iterator
  @param  h     Pointer to the hash table
  @return       The start iterator [khint_t]
 */
#define kh_begin(h) (khint_t)(0)

/*! @function
  @abstract     Iterate over the entries in the hash table
  @param  h     Pointer to the hash table
  @param  kvar  Variable to which key will be assigned
  @param  vvar  Variable to which value will be assigned
  @param  code  Block of code to execute
 */
#define kh_foreach(h, kvar, vvar, code) { khint_t __i;		\
	for (__i = kh_begin(h); __i != kh_end(h); ++__i) {	\
		if (!kh_exist(h,__i)) continue;			\
		(kvar) = kh_key(h,__i);				\
		(vvar) = kh_val(h,__i);				\
		code;						\
	} }

/*! @function
  @abstract     Iterate over the values in the hash table
  @param  h     Pointer to the hash table
  @param  vvar  Variable to which value will be assigned
  @param  code  Block of code to execute
 */
#define kh_foreach_value(h, vvar, code) { khint_t __i;		\
	for (__i = kh_begin(h); __i != kh_end(h); ++__i) {	\
		if (!kh_exist(h,__i)) continue;			\
		(vvar) = kh_val(h,__i);				\
		code;						\
	} }

static inline unsigned int oidhash_by_value(struct object_id oid)
{
	return oidhash(&oid);
}

static inline int oideq_by_value(struct object_id a, struct object_id b)
{
	return oideq(&a, &b);
}

KHASHL_SET_INIT(KH_LOCAL, kh_oid_set, oid_set, struct object_id,
		oidhash_by_value, oideq_by_value)

KHASHL_MAP_INIT(KH_LOCAL, kh_oid_map, oid_map, struct object_id, void *,
		oidhash_by_value, oideq_by_value)

KHASHL_MAP_INIT(KH_LOCAL, kh_oid_pos, oid_pos, struct object_id, int,
		oidhash_by_value, oideq_by_value)

#endif /* __AC_KHASHL_H */
