#ifndef STDIO_H_INCLUDED
#define STDIO_H_INCLUDED
#include <stdio.h>
#endif

#ifndef STDLIB_H_INCLUDED
#define STDLIB_H_INCLUDED
#include <stdlib.h>
#endif

#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED
#include <string.h>
#endif

#ifndef ASSERT_H_INCLUDED
#define ASSERT_H_INCLUDED
#include <assert.h>
#endif

#ifndef KLT_LOGGER_H_INCLUDED
#define KLT_LOGGER_H_INCLUDED
#define KLT_LOG_MODULE "xlts"
#include "klt_logger.h"
#endif

#ifndef XLTS_H_INCLUDED
#define XLTS_H_INCLUDED
#include "xlts.h"
#endif


void xlts_init(struct xlts *x) {
  x->num_original_ = x->num_original_allocated_ = 0;
  x->original_ = NULL;

  x->num_translated_ = x->num_translated_allocated_ = 0;
  x->translated_ = NULL;

  x->num_chunks_ = x->num_chunks_allocated_ = 0;
  x->chunks_ = NULL;
}

void xlts_cleanup(struct xlts *x) {
  if (x->original_) {
    free(x->original_);
  }
  if (x->translated_) {
    free(x->translated_);
  }
  if (x->chunks_) {
    free(x->chunks_);
  }
}

void xlts_reset(struct xlts *x) {
  x->num_original_ = 0;
  x->num_translated_ = 0;
  x->num_chunks_ = 0;
}

static int xlts_append_strings(size_t *num, size_t *num_allocated, char **p, size_t num_bytes, char *bytes) {
  size_t size_needed = *num + num_bytes;
  if (size_needed < num_bytes) {
    LOGERROR("Error: overflow on reallocation\n");
    return XLTS_INTERNAL_ERROR;
  }
  /* Add in null terminator */
  if (size_needed == SIZE_MAX) {
    LOGERROR("Error: overflow on reallocation\n");
    return XLTS_INTERNAL_ERROR;
  }
  size_needed++;

  if (size_needed < 128) {
    /* Start with a decent minimum */
    size_needed = 128;
  }
  if (size_needed > *num_allocated) {
    size_t size_to_allocate = *num_allocated * 2 + 1;
    if (size_to_allocate <= *num_allocated) {
      LOGERROR("Error: overflow on reallocation\n");
      return XLTS_INTERNAL_ERROR;
    }
    if (size_to_allocate < size_needed) {
      size_to_allocate = size_needed;
    }
    void *buf = realloc(*p, size_to_allocate);
    if (!buf) {
      LOGERROR("Error: no memory\n");
      return XLTS_INTERNAL_ERROR;
    }
    *p = (char *)buf;
    *num_allocated = size_to_allocate;
  }
  memcpy((*p) + *num, bytes, num_bytes);
  *num += num_bytes;
  (*p)[*num] = '\0';
  return 0;
}

static int xlts_append_chunk(struct xlts *x, xlts_chunk_type_t ct, int line, int col, size_t offset, size_t num_bytes) {
  if (x->num_chunks_) {
    struct xlts_chunk *c = x->chunks_ + x->num_chunks_ - 1;
    if (ct == XLTS_XLAT) {
      if ((c->ct_ == XLTS_XLAT) &&
          (c->line_ == 0) &&
          (c->col_ == 0) &&
          (c->offset_ ==0)) {
        /* Translated chunks piggy-back on this allocation routine but don't perform line, column or offset tracking. */
        c->num_translated_bytes_ += num_bytes;
        return 0;
      }
    }
    else if ((c->ct_ == ct) &&
        (c->line_ == line) &&
        ((c->col_ + c->num_original_bytes_) == col) &&
        ((c->offset_ + c->num_original_bytes_) == offset)) {
      /* Chunk to be appended is adjacent and on the same line as the last chunk.
       * Extend the last chunk */
      c->num_original_bytes_ += num_bytes;
      if (ct == XLTS_EQUAL) {
        c->num_translated_bytes_ += num_bytes;
      }
      return 0;
    }
  }
  /* Need a new chunk */
  if (x->num_chunks_ == x->num_chunks_allocated_) {
    size_t num_to_alloc = x->num_chunks_allocated_ * 2 + 1;
    if (num_to_alloc <= x->num_chunks_allocated_) {
      LOGERROR("Error: overflow on reallocation\n");
      return XLTS_INTERNAL_ERROR;
    }
    if (num_to_alloc > (SIZE_MAX / sizeof(struct xlts_chunk))) {
      LOGERROR("Error: overflow on reallocation\n");
      return XLTS_INTERNAL_ERROR;
    }
    if (num_to_alloc < 16) {
      /* Apply a reasonable minimum allocation size */
      num_to_alloc = 16;
    }
    size_t size_to_allocate = num_to_alloc * sizeof(struct xlts_chunk);
    void *p = realloc(x->chunks_, size_to_allocate);
    if (!p) {
      LOGERROR("Error: no memory\n");
      return XLTS_INTERNAL_ERROR;
    }
    x->chunks_ = (struct xlts_chunk *)p;
    x->num_chunks_allocated_ = num_to_alloc;
  }
  struct xlts_chunk *c = x->chunks_ + x->num_chunks_++;
  if (ct == XLTS_ORIGINAL) {
    c->ct_ = ct;
    c->line_ = line;
    c->col_ = col;
    c->offset_ = offset;
    c->num_original_bytes_ = num_bytes;
    c->num_translated_bytes_ = 0;
  }
  else if (ct == XLTS_EQUAL) {
    c->ct_ = ct;
    c->line_ = line;
    c->col_ = col;
    c->offset_ = offset;
    c->num_original_bytes_ = num_bytes;
    c->num_translated_bytes_ = num_bytes;
  }
  else if (ct == XLTS_XLAT) {
    c->ct_ = ct;
    c->line_ = 0;
    c->col_ = 0;
    c->offset_ = 0;
    c->num_original_bytes_ = 0;
    c->num_translated_bytes_ = num_bytes;
  }
  return 0;
}

int xlts_append_original(struct xlts *x, int line, int col, size_t offset, size_t num_bytes, char *bytes) {
  int r;
  size_t old_num_original = x->num_original_;
  r = xlts_append_strings(&x->num_original_, &x->num_original_allocated_, &x->original_, num_bytes, bytes);
  if (r) {
    return r;
  }
  r = xlts_append_chunk(x, XLTS_ORIGINAL, line, col, offset, num_bytes);
  if (r) {
    /* Roll back append on the string */
    if (x->original_) {
      /* Re-apply null terminator to original placement */
      x->original_[old_num_original] = '\0';
    }
    x->num_original_ = old_num_original;
    return r;
  }
  return 0;
}

int xlts_append_xlat(struct xlts *x, size_t num_bytes, char *bytes) {
  int r;
  size_t old_num_xlat = x->num_translated_;
  r = xlts_append_strings(&x->num_translated_, &x->num_translated_allocated_, &x->translated_, num_bytes, bytes);
  if (r) {
    return r;
  }
  r = xlts_append_chunk(x, XLTS_XLAT, 0, 0, 0, num_bytes);
  if (r) {
    /* Roll back append on the string */
    if (x->translated_) {
      x->translated_[old_num_xlat] = '\0';
    }
    x->num_translated_ = old_num_xlat;
    return r;
  }
  return 0;
}

int xlts_append_equal(struct xlts *x, int line, int col, size_t offset, size_t num_bytes, char *bytes) {
  int r;
  size_t old_num_original = x->num_original_;
  r = xlts_append_strings(&x->num_original_, &x->num_original_allocated_, &x->original_, num_bytes, bytes);
  if (!r) {
    size_t old_num_xlat = x->num_translated_;
    r = xlts_append_strings(&x->num_translated_, &x->num_translated_allocated_, &x->translated_, num_bytes, bytes);
    if (!r) {
      r = xlts_append_chunk(x, XLTS_EQUAL, line, col, offset, num_bytes);
      if (!r) {
        return 0;
      }
    }
    if (x->translated_) {
      x->translated_[old_num_xlat] = '\0';
    }
    x->num_translated_ = old_num_xlat;
  }
  if (x->original_) {
    x->original_[old_num_original] = '\0';
  }
  x->num_original_ = old_num_original;
  return r;
}

int xlts_append(struct xlts *dst, struct xlts *src) {
  int r;
  size_t idx;

  size_t old_num_original = dst->num_original_;
  r = xlts_append_strings(&dst->num_original_, &dst->num_original_allocated_, &dst->original_, src->num_original_, src->original_);
  if (!r) {
    size_t old_num_xlat = dst->num_translated_;
    r = xlts_append_strings(&dst->num_translated_, &dst->num_translated_allocated_, &dst->translated_, src->num_translated_, src->translated_);
    if (!r) {
      size_t old_num_chunks = dst->num_chunks_;
      struct xlts_chunk last_chunk;
      if (old_num_chunks) {
        last_chunk = dst->chunks_[old_num_chunks - 1];
      }
      for (idx = 0; idx < src->num_chunks_; ++idx) {
        struct xlts_chunk *c = src->chunks_ + idx;
        r = xlts_append_chunk(dst, c->ct_, c->line_, c->col_, c->offset_, (c->ct_ != XLTS_XLAT) ? c->num_original_bytes_ : c->num_translated_bytes_);
        if (r) {
          break;
        }
      }
      if (idx == src->num_chunks_) {
        /* success! */
        return 0;
      }
      /* failure, unwind everything */
      if (old_num_chunks) {
        /* Repair last chunk (if any) */
        dst->chunks_[old_num_chunks - 1] = last_chunk;
      }
      dst->num_chunks_ = old_num_chunks;
    }
    if (dst->translated_) {
      dst->translated_[old_num_xlat] = '\0';
    }
    dst->num_translated_ = old_num_xlat;
  }
  if (dst->original_) {
    dst->original_[old_num_original] = '\0';
  }
  dst->num_original_ = old_num_original;

  return r;
}

int xlts_append_left_translated(struct xlts *dst, struct xlts *src, size_t num_translated_bytes) {
  int r;
  size_t idx;
  size_t num_bytes_remaining = num_translated_bytes;
  char *ori = src->original_, *xlt = src->translated_;
  for (idx = 0; idx < src->num_chunks_; ++idx) {
    struct xlts_chunk *c = src->chunks_ + idx;
    if (num_bytes_remaining >= c->num_translated_bytes_) {
      /* Either we still need to move more bytes than there are bytes in this chunk, or, there 
       * are no translated bytes in this chunk */
      switch (c->ct_) {
      case XLTS_ORIGINAL:
        r = xlts_append_original(dst, c->line_, c->col_, c->offset_, c->num_original_bytes_, ori);
        ori += c->num_original_bytes_;
        break;
      case XLTS_EQUAL:
        r = xlts_append_equal(dst, c->line_, c->col_, c->offset_, c->num_original_bytes_, ori);
        ori += c->num_original_bytes_;
        xlt += c->num_translated_bytes_;
        break;
      case XLTS_XLAT:
        r = xlts_append_xlat(dst, c->num_translated_bytes_, xlt);
        xlt += c->num_translated_bytes_;
        break;
      default:
        assert(0 && "Unrecognized chunk type");
        r = XLTS_INTERNAL_ERROR;
        break;
      }
      if (r) {
        return r;
      }
      num_bytes_remaining -= c->num_translated_bytes_;
    }
    else {
      /* More translated bytes in chunk than there are bytes remaining */
      if (num_bytes_remaining) {
        switch (c->ct_) {
        case XLTS_ORIGINAL:
          r = xlts_append_original(dst, c->line_, c->col_, c->offset_, c->num_original_bytes_, ori);
          ori += c->num_original_bytes_;
          break;
        case XLTS_EQUAL:
          r = xlts_append_equal(dst, c->line_, c->col_, c->offset_, num_bytes_remaining, ori);
          ori += num_bytes_remaining;
          xlt += num_bytes_remaining;
          break;
        case XLTS_XLAT:
          r = xlts_append_xlat(dst, num_bytes_remaining, xlt);
          xlt += num_bytes_remaining;
          break;
        default:
          assert(0 && "Unrecognized chunk type");
          r = XLTS_INTERNAL_ERROR;
          break;
        }
        if (r) {
          return r;
        }
        num_bytes_remaining = 0;
      }
      return 0;
    }
  }

  /* num_translated_bytes equals or exceeds the number of actual translated bytes in src */
  return 0;
}

void xlts_strip_left_translated(struct xlts *x, size_t num_translated_bytes) {
  size_t idx;
  size_t num_bytes_remaining = num_translated_bytes;
  char *ori = x->original_;
  char *xlt = x->translated_;
  for (idx = 0; idx < x->num_chunks_; idx++) {
    struct xlts_chunk *c = x->chunks_ + idx;
    if (num_bytes_remaining >= c->num_translated_bytes_) {
      ori += c->num_original_bytes_;
      xlt += c->num_translated_bytes_;
      num_bytes_remaining -= c->num_translated_bytes_;
    }
    else {
      if (num_bytes_remaining) {
        switch (c->ct_) {
        case XLTS_ORIGINAL:
          /* NOTE: Should not get here, num_translated_bytes_ should be 0 for XLTS_ORIGINAL, in which
           * case (num_bytes_remaining >= c->num_translated_bytes_) should always be true. */
          assert(0);
          break;
        case XLTS_EQUAL:
          c->num_original_bytes_ -= num_bytes_remaining;
          c->num_translated_bytes_ -= num_bytes_remaining;
          c->col_ += (int)num_bytes_remaining;
          ori += num_bytes_remaining;
          xlt += num_bytes_remaining;
          break;
        case XLTS_XLAT:
          c->num_translated_bytes_ -= num_bytes_remaining;
          xlt += num_bytes_remaining;
          break;
        }
      }
      memcpy(x->original_, ori, x->original_ + x->num_original_ - ori);
      x->num_original_ -= ori - x->original_;
      if (x->original_) {
        x->original_[x->num_original_] = '\0';
      }
      memcpy(x->translated_, xlt, x->translated_ + x->num_translated_ - xlt);
      x->num_translated_ -= xlt - x->translated_;
      if (x->translated_) {
        x->translated_[x->num_translated_] = '\0';
      }
      memcpy(x->chunks_, x->chunks_ + idx, sizeof(struct xlts_chunk) * (x->num_chunks_ - idx));
      x->num_chunks_ -= idx;
      return;
    }
  }
  /* num_translated_bytes exceeds total number of translated bytes, reset everything */
  xlts_reset(x);
  return;
}

int xlts_clamp_left_translated(struct xlts *x, size_t num_translated_bytes, struct xlts_clamp *clamp) {
  size_t idx;
  size_t num_bytes_remaining = num_translated_bytes;
  char *ori = x->original_;
  char *xlt = x->translated_;
  for (idx = 0; idx < x->num_chunks_; idx++) {
    struct xlts_chunk *c = x->chunks_ + idx;
    if (num_bytes_remaining >= c->num_translated_bytes_) {
      ori += c->num_original_bytes_;
      xlt += c->num_translated_bytes_;
      num_bytes_remaining -= c->num_translated_bytes_;
    }
    else {
      clamp->chunk_num_ = x->num_chunks_;
      if (num_bytes_remaining) {
        /* Current chunk will become the new tail */
        x->num_chunks_ = idx + 1;
        clamp->chunk_tail_ = *c;
        switch (c->ct_) {
        case XLTS_ORIGINAL:
          /* NOTE: Should not get here, num_translated_bytes_ should be 0 for XLTS_ORIGINAL, in which
           * case (num_bytes_remaining >= c->num_translated_bytes_) should always be true. */
          assert(0);
          break;
        case XLTS_EQUAL:
          c->num_original_bytes_ = num_bytes_remaining;
          c->num_translated_bytes_ = num_bytes_remaining;
          ori += num_bytes_remaining;
          xlt += num_bytes_remaining;
          break;
        case XLTS_XLAT:
          c->num_translated_bytes_ = num_bytes_remaining;
          xlt += num_bytes_remaining;
          break;
        }
      }
      else {
        if (idx) {
          /* Tail will be our predecessor, we finish prior to c chunk */
          clamp->chunk_tail_ = c[-1];
          x->num_chunks_ = idx;
        }
      }
      clamp->ori_num_ = x->num_original_;
      clamp->ori_term_ = ori ? *ori : '\0';
      clamp->xlt_num_ = x->num_translated_;
      clamp->xlt_term_ = xlt ? *xlt : '\0';

      if (ori) *ori = '\0';
      if (xlt) *xlt = '\0';
      x->num_original_ = ori - x->original_;
      x->num_translated_ = xlt - x->translated_;

      return 0;
    }
  }
  /* num_translated_bytes encompasses all of x on the left hand side. Make clamp a no-op */
  clamp->chunk_num_ = x->num_chunks_;
  if (x->num_chunks_) {
    clamp->chunk_tail_ = x->chunks_[x->num_chunks_ - 1];
  }
  clamp->ori_num_ = x->num_original_;
  clamp->ori_term_ = ori ? *ori : '\0';
  clamp->xlt_num_ = x->num_translated_;
  clamp->xlt_term_ = xlt ? *xlt : '\0';

  return 0;
}


void xlts_clamp_remove(struct xlts *x, struct xlts_clamp *clamp) {
  if (x->original_) {
    x->original_[x->num_original_] = clamp->ori_term_;
  }
  if (x->translated_) {
    x->translated_[x->num_translated_] = clamp->xlt_term_;
  }
  x->num_original_ = clamp->ori_num_;
  x->num_translated_ = clamp->xlt_num_;
  if (x->num_chunks_) {
    x->chunks_[x->num_chunks_ - 1] = clamp->chunk_tail_;
  }
  x->num_chunks_ = clamp->chunk_num_;
}

void xlts_swap(struct xlts *a, struct xlts *b) {
  struct xlts swap;
  swap = *a;
  *a = *b;
  *b = swap;
}
