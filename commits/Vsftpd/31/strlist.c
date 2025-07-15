/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * strlist.c
 */

/* Anti-lamer measures deployed, sir! */
#define PRIVATE_HANDS_OFF_alloc_len alloc_len
#define PRIVATE_HANDS_OFF_list_len list_len
#define PRIVATE_HANDS_OFF_p_nodes p_nodes

#define PRIVATE_HANDS_OFF_p_buf p_buf
#define PRIVATE_HANDS_OFF_len len
#define PRIVATE_HANDS_OFF_alloc_bytes alloc_bytes
#include "strlist.h"

#include "str.h"
#include "utility.h"
#include "sysutil.h"

#include <klee/klee.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct mystr_list_node
{
  struct mystr str;
  struct mystr sort_key_str;
};

/* File locals */
static const unsigned int kMaxStrlist = 10 * 1000 * 1000;

static struct mystr s_null_str;

static int sort_compare_func(const void* p1, const void* p2);
static int sort_compare_func_reverse(const void* p1, const void* p2);
static int sort_compare_common(const void* p1, const void* p2, int reverse);

void*
my_vsf_sysutil_malloc(unsigned int size)
{
  void* p_ret;
  /* Paranoia - what if we got an integer overflow/underflow? */
  if (size == 0 || size > INT_MAX)
  {
    // bug("zero or big size in vsf_sysutil_malloc");
  }  
  p_ret = malloc(size);
  if (p_ret == NULL)
  {
    // die("malloc");
  }
  return p_ret;
}

void*
my_vsf_sysutil_realloc(void* p_ptr, unsigned int size)
{
  void* p_ret;
  if (size == 0 || size > INT_MAX)
  {
    // bug("zero or big size in vsf_sysutil_realloc");
  }
  p_ret = realloc(p_ptr, size);
  if (p_ret == NULL)
  {
    // die("realloc");
  }
  return p_ret;
}

void
my_vsf_sysutil_memcpy(void* p_dest, const void* p_src, const unsigned int size)
{
  /* Safety */
  if (size == 0)
  {
    return;
  }
  /* Defense in depth */
  if (size > INT_MAX)
  {
    // die("possible negative value to memcpy?");
  }
  memcpy(p_dest, p_src, size);
}

void
my_vsf_sysutil_free(void* p_ptr)
{
  if (p_ptr == NULL)
  {
    // bug("vsf_sysutil_free got a null pointer");
  }
  free(p_ptr);
}

static void
my_s_setbuf(struct mystr* p_str, char* p_newbuf)
{
  if (p_str->p_buf != 0)
  {
    // bug("p_buf not NULL when setting it");
  }
  p_str->p_buf = p_newbuf;
}

void
my_str_free(struct mystr* p_str)
{
  if (p_str->p_buf != 0)
  {
    my_vsf_sysutil_free(p_str->p_buf);
  }
  p_str->p_buf = 0;
  p_str->len = 0;
  p_str->alloc_bytes = 0;
}

void
my_private_str_alloc_memchunk(struct mystr* p_str, const char* p_src,
                           unsigned int len)
{
  /* Make sure this will fit in the buffer */
  unsigned int buf_needed;
  if (len + 1 < len)
  {
    // bug("integer overflow");
  }
  buf_needed = len + 1;
  if (buf_needed > p_str->alloc_bytes)
  {
    my_str_free(p_str);
    my_s_setbuf(p_str, my_vsf_sysutil_malloc(buf_needed));
    p_str->alloc_bytes = buf_needed;
  }
  my_vsf_sysutil_memcpy(p_str->p_buf, p_src, len);
  p_str->p_buf[len] = '\0';
  p_str->len = len;
}

void
my_str_copy(struct mystr* p_dest, const struct mystr* p_src)
{
  my_private_str_alloc_memchunk(p_dest, p_src->p_buf, p_src->len);
}

void
str_list_free(struct mystr_list* p_list)
{
  unsigned int i;
  for (i=0; i < p_list->list_len; ++i)
  {
    str_free(&p_list->p_nodes[i].str);
    str_free(&p_list->p_nodes[i].sort_key_str);
  }
  p_list->list_len = 0;
  p_list->alloc_len = 0;
  if (p_list->p_nodes)
  {
    vsf_sysutil_free(p_list->p_nodes);
    p_list->p_nodes = 0;
  }
}

unsigned int
str_list_get_length(const struct mystr_list* p_list)
{
  return p_list->list_len;
}

int
str_list_contains_str(const struct mystr_list* p_list,
                      const struct mystr* p_str)
{
  unsigned int i;
  for (i=0; i < p_list->list_len; ++i)
  {
    if (str_equal(p_str, &p_list->p_nodes[i].str))
    {
      return 1;
    }
  }
  return 0;
}

void
str_list_add(struct mystr_list* p_list, const struct mystr* p_str,
             const struct mystr* p_sort_key_str)
{
  struct mystr_list_node* p_node;
  /* Expand the node allocation if we have to */
  if (p_list->list_len == p_list->alloc_len)
  {
    if (p_list->alloc_len == 0)
    {
      p_list->alloc_len = 32;
      p_list->p_nodes = my_vsf_sysutil_malloc(
          p_list->alloc_len * (unsigned int) sizeof(struct mystr_list_node));
    }
    else
    {
      p_list->alloc_len *= 2;
      if (p_list->alloc_len > kMaxStrlist)
      {
        // die("excessive strlist");
      }
      p_list->p_nodes = my_vsf_sysutil_realloc(
          p_list->p_nodes,
          p_list->alloc_len * (unsigned int) sizeof(struct mystr_list_node));
    }
  }
  p_node = &p_list->p_nodes[p_list->list_len];
  p_node->str = s_null_str;
  p_node->sort_key_str = s_null_str;
  my_str_copy(&p_node->str, p_str);
  if (p_sort_key_str)
  {
    my_str_copy(&p_node->sort_key_str, p_sort_key_str);
  }
  p_list->list_len++;
}

void chatgpt_str_list_add(struct mystr_list* p_list, const struct mystr* p_str, const struct mystr* p_sort_key_str)
{
    // Kill session if string list is over the limit
    if (p_list->list_len >= kMaxStrlist)
    {
        // perror("String list exceeded the limit");
        exit(1);
    }

    struct mystr_list_node* p_node;
    /* Expand the node allocation if we have to */
    if (p_list->list_len == p_list->alloc_len)
    {
        if (p_list->alloc_len == 0)
        {
            p_list->alloc_len = 32;
            p_list->p_nodes = my_vsf_sysutil_malloc(p_list->alloc_len * sizeof(struct mystr_list_node));
        }
        else
        {
            p_list->alloc_len *= 2;
            p_list->p_nodes = my_vsf_sysutil_realloc(p_list->p_nodes, p_list->alloc_len * sizeof(struct mystr_list_node));
        }
    }
    p_node = &p_list->p_nodes[p_list->list_len];
    p_node->str = s_null_str;
    p_node->sort_key_str = s_null_str;
    my_str_copy(&p_node->str, p_str);
    if (p_sort_key_str)
    {
        my_str_copy(&p_node->sort_key_str, p_sort_key_str);
    }
    p_list->list_len++;
}

void
str_list_sort(struct mystr_list* p_list, int reverse)
{
  if (!reverse)
  {
    vsf_sysutil_qsort(p_list->p_nodes, p_list->list_len,
                      sizeof(struct mystr_list_node), sort_compare_func);
  }
  else
  {
    vsf_sysutil_qsort(p_list->p_nodes, p_list->list_len,
                      sizeof(struct mystr_list_node),
                      sort_compare_func_reverse);
  }
}

static int
sort_compare_func(const void* p1, const void* p2)
{
  return sort_compare_common(p1, p2, 0);
}

static int
sort_compare_func_reverse(const void* p1, const void* p2)
{
  return sort_compare_common(p1, p2, 1);
}

static int
sort_compare_common(const void* p1, const void* p2, int reverse)
{
  const struct mystr* p_cmp1;
  const struct mystr* p_cmp2;
  const struct mystr_list_node* p_node1 = (const struct mystr_list_node*) p1;
  const struct mystr_list_node* p_node2 = (const struct mystr_list_node*) p2;
  if (!str_isempty(&p_node1->sort_key_str))
  {
    p_cmp1 = &p_node1->sort_key_str;
  }
  else
  {
    p_cmp1 = &p_node1->str;
  }
  if (!str_isempty(&p_node2->sort_key_str))
  {
    p_cmp2 = &p_node2->sort_key_str;
  }
  else
  {
    p_cmp2 = &p_node2->str;
  }

  if (reverse)
  {
    return str_strcmp(p_cmp2, p_cmp1);
  }
  else
  {
    return str_strcmp(p_cmp1, p_cmp2);
  }
}

const struct mystr*
str_list_get_pstr(const struct mystr_list* p_list, unsigned int indexx)
{
  if (indexx >= p_list->list_len)
  {
    bug("indexx out of range in str_list_get_str");
  }
  return &p_list->p_nodes[indexx].str;
}

int main() {
  struct mystr_list list;
  struct mystr str, sort_key_str;
  int MAX_STR_LEN = 16;
  char* str_buf;
  char* sort_key_buf;
  
  list.list_len = 0;
  list.alloc_len = 0;
  list.p_nodes = (void *)0;
  
  str_buf = (char *)malloc(MAX_STR_LEN);
  klee_make_symbolic(str_buf, MAX_STR_LEN, "str_buf");
  str_buf[MAX_STR_LEN - 1] = '\0';

  sort_key_buf = (char *)malloc(MAX_STR_LEN);
  klee_make_symbolic(sort_key_buf, MAX_STR_LEN, "sort_key_buf");
  sort_key_buf[MAX_STR_LEN - 1] = '\0';
  
  str.PRIVATE_HANDS_OFF_p_buf = str_buf;
  str.PRIVATE_HANDS_OFF_len = MAX_STR_LEN - 1;
  str.PRIVATE_HANDS_OFF_alloc_bytes = MAX_STR_LEN;
  
  sort_key_str.PRIVATE_HANDS_OFF_p_buf = sort_key_buf;
  sort_key_str.PRIVATE_HANDS_OFF_len = MAX_STR_LEN - 1;
  sort_key_str.PRIVATE_HANDS_OFF_alloc_bytes = MAX_STR_LEN;
  
  chatgpt_str_list_add(&list, &str, &sort_key_str);


  struct mystr_list list2;
  struct mystr str2, sort_key_str2;
  char* str_buf2 = (char *)malloc(MAX_STR_LEN);
  char* sort_key_buf2 = (char *)malloc(MAX_STR_LEN);

  list2.list_len = 0;
  list2.alloc_len = 0;
  list2.p_nodes = (void *)0;

  strcpy(str_buf2, str_buf);
  strcpy(sort_key_buf2, sort_key_buf);

  str2.PRIVATE_HANDS_OFF_p_buf = str_buf2;
  str2.PRIVATE_HANDS_OFF_len = MAX_STR_LEN - 1;
  str2.PRIVATE_HANDS_OFF_alloc_bytes = MAX_STR_LEN;

  sort_key_str2.PRIVATE_HANDS_OFF_p_buf = sort_key_buf2;
  sort_key_str2.PRIVATE_HANDS_OFF_len = MAX_STR_LEN - 1;
  sort_key_str2.PRIVATE_HANDS_OFF_alloc_bytes = MAX_STR_LEN;

  str_list_add(&list2, &str2, &sort_key_str2);

  klee_assert(str_list_get_length(&list) == str_list_get_length(&list2));
  
  return 0;
}