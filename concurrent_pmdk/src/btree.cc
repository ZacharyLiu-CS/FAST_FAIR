//
//  btree.cc
//  PROJECT btree
//
//  Created by zhenliu on 17/08/2022.
//  Copyright (c) 2022 zhenliu <liuzhenm@mail.ustc.edu.cn>.
//

#include "btree.h"
namespace fastfair {

/*
 * class btree
 */
void btree::constructor(PMEMobjpool* pool)
{
  pop = pool;
  POBJ_NEW(pop, &root, page, NULL, NULL);
  D_RW(root)->constructor();
  height = 1;
}

void btree::setNewRoot(TOID(page) new_root)
{
  root = new_root;
  pmemobj_persist(pop, &root, sizeof(TOID(page)));
  ++height;
}

char* btree::btree_search(entry_key_t key)
{
  TOID(page)
  p = root;

  while (D_RO(p)->hdr.leftmost_ptr != NULL) {
    p.oid.off = (uint64_t)D_RW(p)->linear_search(key);
  }

  uint64_t t;
  while ((t = (uint64_t)D_RW(p)->linear_search(key)) == D_RO(p)->hdr.sibling_ptr.oid.off) {
    p.oid.off = t;
    if (!t) {
      break;
    }
  }

  if (!t) {
    printf("NOT FOUND %lu, t = %lx\n", key, t);
    return NULL;
  }

  return (char*)t;
}

// insert the key in the leaf node
void btree::btree_insert(entry_key_t key, char* right)
{
  TOID(page)
  p = root;

  while (D_RO(p)->hdr.leftmost_ptr != NULL) {
    p.oid.off = (uint64_t)D_RW(p)->linear_search(key);
  }

  if (!D_RW(p)->store(this, NULL, key, right, true, true)) { // store
    btree_insert(key, right);
  }
}

// store the key into the node at the given level
void btree::btree_insert_internal(char* left, entry_key_t key, char* right,
    uint32_t level)
{
  if (level > D_RO(root)->hdr.level)
    return;

  TOID(page)
  p = root;

  while (D_RO(p)->hdr.level > level)
    p.oid.off = (uint64_t)D_RW(p)->linear_search(key);

  if (!D_RW(p)->store(this, NULL, key, right, true, true)) {
    btree_insert_internal(left, key, right, level);
  }
}

void btree::btree_delete(entry_key_t key)
{
  TOID(page)
  p = root;

  while (D_RO(p)->hdr.leftmost_ptr != NULL) {
    p.oid.off = (uint64_t)D_RW(p)->linear_search(key);
  }

  uint64_t t;
  while ((t = (uint64_t)(D_RW(p)->linear_search(key))) == D_RW(p)->hdr.sibling_ptr.oid.off) {
    p.oid.off = t;
    if (!t)
      break;
  }

  if (t) {
    if (!D_RW(p)->remove(this, key)) {
      btree_delete(key);
    }
  } else {
    printf("not found the key to delete %lu\n", key);
  }
}

void btree::btree_delete_internal(entry_key_t key, char* ptr, uint32_t level,
    entry_key_t* deleted_key,
    bool* is_leftmost_node, page** left_sibling)
{
  if (level > D_RO(root)->hdr.level)
    return;

  TOID(page)
  p = root;

  while (D_RW(p)->hdr.level > level) {
    p.oid.off = (uint64_t)D_RW(p)->linear_search(key);
  }

  pthread_rwlock_wrlock(D_RW(p)->hdr.rwlock);

  if ((char*)D_RO(p)->hdr.leftmost_ptr == ptr) {
    *is_leftmost_node = true;
    pthread_rwlock_unlock(D_RW(p)->hdr.rwlock);
    return;
  }

  *is_leftmost_node = false;

  for (int i = 0; D_RO(p)->records[i].ptr != NULL; ++i) {
    if (D_RO(p)->records[i].ptr == ptr) {
      if (i == 0) {
        if ((char*)D_RO(p)->hdr.leftmost_ptr != D_RO(p)->records[i].ptr) {
          *deleted_key = D_RO(p)->records[i].key;
          *left_sibling = D_RO(p)->hdr.leftmost_ptr;
          D_RW(p)->remove(this, *deleted_key, false, false);
          break;
        }
      } else {
        if (D_RO(p)->records[i - 1].ptr != D_RO(p)->records[i].ptr) {
          *deleted_key = D_RO(p)->records[i].key;
          *left_sibling = (page*)D_RO(p)->records[i - 1].ptr;
          D_RW(p)->remove(this, *deleted_key, false, false);
          break;
        }
      }
    }
  }

  pthread_rwlock_unlock(D_RW(p)->hdr.rwlock);
}

// Function to search keys from "min" to "max"
void btree::btree_search_range(entry_key_t min, entry_key_t max,
    unsigned long* buf, uint32_t len = INT32_MAX)
{
  TOID(page)
  p = root;

  while (p.oid.off != 0) {
    if (D_RO(p)->hdr.leftmost_ptr != NULL) {
      // The current page is internal
      p.oid.off = (uint64_t)D_RW(p)->linear_search(min);
    } else {
      // Found a leaf
      D_RW(p)->linear_search_range(min, max, buf, len);

      break;
    }
  }
}

void btree::printAll()
{
  int total_keys = 0;
  TOID(page)
  leftmost = root;
  printf("root: %lx\n", root.oid.off);
  if (root.oid.off) {
    do {
      TOID(page)
      sibling = leftmost;
      while (sibling.oid.off) {
        if (D_RO(sibling)->hdr.level == 0) {
          total_keys += D_RO(sibling)->hdr.last_index + 1;
        }
        D_RW(sibling)->print();
        sibling = D_RO(sibling)->hdr.sibling_ptr;
      }
      printf("-----------------------------------------\n");
      leftmost.oid.off = (uint64_t)D_RO(leftmost)->hdr.leftmost_ptr;
    } while (leftmost.oid.off != 0);
  }

  printf("total number of keys: %d\n", total_keys);
}

void btree::randScounter()
{
  TOID(page)
  leftmost = root;
  srand(time(NULL));
  if (root.oid.off) {
    do {
      TOID(page)
      sibling = leftmost;
      while (sibling.oid.off) {
        D_RW(sibling)->hdr.switch_counter = rand() % 100;
        sibling = D_RO(sibling)->hdr.sibling_ptr;
      }
      leftmost.oid.off = (uint64_t)D_RO(leftmost)->hdr.leftmost_ptr;
    } while (leftmost.oid.off != 0);
  }
}
}
