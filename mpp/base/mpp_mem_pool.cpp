/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define MODULE_TAG "mpp_mem_pool"

#include <string.h>

#include "mpp_err.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_list.h"

#include "mpp_mem_pool.h"

typedef struct MppMemPoolNode_t {
    void                *check;
    struct list_head    list;
    void                *ptr;
    size_t              size;
} MppMemPoolNode;

typedef struct MppMemPoolImpl_t {
    void                *check;
    size_t              size;
    pthread_mutex_t     lock;
    struct list_head    service_link;

    struct list_head    used;
    struct list_head    unused;
    RK_S32              used_count;
    RK_S32              unused_count;
} MppMemPoolImpl;

class MppMemPoolService
{
public:
    static MppMemPoolService* getInstance() {
        AutoMutex auto_lock(mLock);
        static MppMemPoolService pool_service;
        return &pool_service;
    }
    MppMemPoolImpl *get_pool(size_t size);

private:
    MppMemPoolService();
    ~MppMemPoolService();
    struct list_head    mLink;
    static Mutex        mLock;
};

Mutex MppMemPoolService::mLock;

MppMemPoolService::MppMemPoolService()
{
    INIT_LIST_HEAD(&mLink);
}

MppMemPoolService::~MppMemPoolService()
{
    if (!list_empty(&mLink)) {
        MppMemPoolImpl *pos, *n;
        MppMemPoolNode *node, *m;

        list_for_each_entry_safe(pos, n, &mLink, MppMemPoolImpl, service_link) {
            if (!list_empty(&pos->unused)) {
                list_for_each_entry_safe(node, m, &pos->unused, MppMemPoolNode, list) {
                    MPP_FREE(node);
                    pos->unused_count--;
                }
            }

            if (!list_empty(&pos->used)) {
                mpp_err_f("found %d used buffer size %d\n",
                          pos->used_count, pos->size);

                list_for_each_entry_safe(node, m, &pos->used, MppMemPoolNode, list) {
                    MPP_FREE(pos);
                    pos->used_count--;
                }
            }

            mpp_assert(!pos->used_count);
            mpp_assert(!pos->unused_count);
        }
    }
}

MppMemPoolImpl *MppMemPoolService::get_pool(size_t size)
{
    MppMemPoolImpl *pool = mpp_malloc(MppMemPoolImpl, 1);
    if (NULL == pool)
        return NULL;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&pool->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    pool->check = pool;
    pool->size = size;
    pool->used_count = 0;
    pool->unused_count = 0;

    INIT_LIST_HEAD(&pool->used);
    INIT_LIST_HEAD(&pool->unused);
    INIT_LIST_HEAD(&pool->service_link);
    AutoMutex auto_lock(mLock);
    list_add_tail(&pool->service_link, &mLink);

    return pool;
}

MppMemPool mpp_mem_pool_init(size_t size)
{
    return (MppMemPool)MppMemPoolService::getInstance()->get_pool(size);
}

void *mpp_mem_pool_get(MppMemPool pool)
{
    MppMemPoolImpl *impl = (MppMemPoolImpl *)pool;
    MppMemPoolNode *node = NULL;
    void* ptr = NULL;

    pthread_mutex_lock(&impl->lock);

    if (!list_empty(&impl->unused)) {
        node = list_first_entry(&impl->unused, MppMemPoolNode, list);
        if (node) {
            list_del_init(&node->list);
            list_add_tail(&node->list, &impl->used);
            impl->unused_count--;
            impl->used_count++;
            ptr = node->ptr;
            goto DONE;
        }
    }

    node = mpp_malloc_size(MppMemPoolNode, sizeof(MppMemPoolNode) + impl->size);
    if (NULL == node) {
        mpp_err_f("failed to create node from size %d pool\n", impl->size);
        goto DONE;
    }

    node->check = node;
    node->ptr = (void *)(node + 1);
    node->size = impl->size;
    INIT_LIST_HEAD(&node->list);
    list_add_tail(&node->list, &impl->used);
    impl->used_count++;
    ptr = node->ptr;

DONE:
    pthread_mutex_unlock(&impl->lock);
    if (node)
        memset(node->ptr, 0 , node->size);
    return ptr;
}

void mpp_mem_pool_put(MppMemPool pool, void *p)
{
    MppMemPoolImpl *impl = (MppMemPoolImpl *)pool;
    MppMemPoolNode *node = (MppMemPoolNode *)((RK_U8 *)p - sizeof(MppMemPoolNode));

    if (impl != impl->check) {
        mpp_err_f("invalid mem pool %p check %p\n", impl, impl->check);
        return ;
    }

    if (node != node->check) {
        mpp_err_f("invalid mem pool ptr %p node %p check %p\n",
                  p, node, node->check);
        return ;
    }

    pthread_mutex_lock(&impl->lock);

    list_del_init(&node->list);
    list_add(&node->list, &impl->unused);
    impl->used_count--;
    impl->unused_count++;

    pthread_mutex_unlock(&impl->lock);
}
