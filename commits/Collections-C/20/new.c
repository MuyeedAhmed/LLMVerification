/*
 * Collections-C
 * Copyright (C) 2013-2015 Srđan Panić <srdja.panic@gmail.com>
 *
 * This file is part of Collections-C.
 *
 * Collections-C is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Collections-C is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Collections-C.  If not, see <http://www.gnu.org/licenses/>.
 */

 #include "cc_queue.h"

 struct cc_queue_s {
     CC_Deque *d;
 
     void *(*mem_alloc)  (size_t size);
     void *(*mem_calloc) (size_t blocks, size_t size);
     void  (*mem_free)   (void *block);
 };
 
 /**
  * Initializes the fields of the CC_QueueConf struct to default values.
  *
  * @param[in, out] conf the configuration struct that is being initialized
  */
 void cc_queue_conf_init(CC_QueueConf *conf)
 {
     cc_deque_conf_init(conf);
 }
 
 /**
  * Creates a new empty queue and returns a status code.
  *
  * @param[out] out pointer to where the newly created CC_Queue is to be stored
  *
  * @return  CC_OK if the creation was successful, or CC_ERR_ALLOC if the
  * memory allocation for the new CC_Queue structure failed.
  */
 enum cc_stat cc_queue_new(CC_Queue **queue)
 {
     CC_QueueConf conf;
     cc_queue_conf_init(&conf);
     return cc_queue_new_conf(&conf, queue);
 }
 
 /**
  * Creates a new empty CC_Queue based on the specified CC_QueueConf object and
  * returns a status code.
  *
  * The CC_Queue is allocated using the allocators specified in the CC_QueueConf struct.
  * The allocation may fail if the underlying allocator fails.
  *
  * @param[in] conf CC_Queue configuration structure. All fields must be initialized
  *                 with appropriate values.
  * @param[out] out Pointer to where the newly created CC_Queue is to be stored.
  *
  * @return CC_OK if the creation was successful, or CC_ERR_ALLOC if the memory
  * allocation for the new CC_Queue structure failed.
  */
 enum cc_stat cc_queue_new_conf(CC_QueueConf const * const conf, CC_Queue **q)
 {
     CC_Queue *queue = conf->mem_calloc(1, sizeof(CC_Queue));
 
     if (!queue)
         return CC_ERR_ALLOC;
 
     CC_Deque *deque;
     enum cc_stat stat = cc_deque_new_conf(conf, &deque);
 
     if (stat) {
         conf->mem_free(queue);
         return CC_ERR_ALLOC;
     }
 
     queue->d          = deque;
     queue->mem_alloc  = conf->mem_alloc;
     queue->mem_calloc = conf->mem_calloc;
     queue->mem_free   = conf->mem_free;
 
     *q = queue;
 
     return CC_OK;
 }
 
 /**
  * Destroys the queue structure, but leaves the data it used to hold intact.
  *
  * @param[in] queue the queue that is to be destroyed
  */
 void cc_queue_destroy(CC_Queue *queue)
 {
     cc_deque_destroy(queue->d);
     queue->mem_free(queue);
 }
 
 /**
  * Destroys the queue structure along with all the data it holds.
  *
  * @note This function should not be called on a queue that has some of its
  *       elements allocated on the stack.
  *
  * @param[in] queue the queue that is to be destroyed
  */
 void cc_queue_destroy_cb(CC_Queue *queue, void (*cb) (void*))
 {
     cc_deque_destroy_cb(queue->d, cb);
     free(queue);
 }
 
 /**
  * Gets the element at the front of the queue and sets the out
  * parameter to its value.
  *
  * @param[in] queue the CC_Queue whose element is being returned
  * @param[out] out pointer to where the element is stored
  *
  * @return CC_OK if the element was found, or CC_ERR_OUT_OF_RANGE if the
  * CC_Queue is empty.
  */
 enum cc_stat cc_queue_peek(CC_Queue const * const queue, void **out)
 {
     return cc_deque_get_last(queue->d, out);
 }
 
 /**
  * Gets and removes the element at the front of the queue and optionally
  * sets the out parameter to the value of the removed element.
  *
  * @param[in] queue the queue on which this operation is performed
  * @param[out] out pointer to where the removed element is stored
  *
  * @return CC_OK if the element was found, or CC_ERR_OUT_OF_RANGE if the
  * CC_Queue is empty.
  */
 enum cc_stat cc_queue_poll(CC_Queue *queue, void **out)
 {
     return cc_deque_remove_last(queue->d, out);
 }
 
 /**
  * Appends an element to the back of the queue. This operation may
  * fail if the memory allocation for the new element fails.
  *
  * @param[in] queue the queue on which this operation is performed
  * @param[in] element the element being enqueued
  *
  * @return CC_OK if the element was successfully added, or CC_ERR_ALLOC
  * if the memory allocation for the new element failed.
  */
 enum cc_stat cc_queue_enqueue(CC_Queue *queue, void *element)
 {
     return cc_deque_add_first(queue->d, element);
 }
 
 /**
  * Returns the size of the specified queue. The size of the queue is
  * the number of elements contained within the queue.
  *
  * @param[in] queue the queue whose size is being returned
  *
  * @return the number of elements within the queue.
  */
 size_t cc_queue_size(CC_Queue const * const queue)
 {
     return cc_deque_size(queue->d);
 }
 
 /**
  * Applies the function fn to each element of the CC_Queue.
  *
  * @param[in] queue the queue on which this operation is performed
  * @param[in] fn the operation function that is to be invoked on each queue element
  */
 void cc_queue_foreach(CC_Queue *queue, void (*fn) (void*))
 {
     cc_deque_foreach(queue->d, fn);
 }
 
 /**
  * Initializes the iterator.
  *
  * @param[in] iter the iterator that is being initialized
  * @param[in] queue the queue to iterate over
  */
 void cc_queue_iter_init(CC_QueueIter *iter, CC_Queue *queue)
 {
     cc_deque_iter_init(&(iter->i), queue->d);
 }
 
 /**
  * Advances the iterator and sets the out parameter to the value of the
  * next element in the sequence.
  *
  * @param[in] iter the iterator that is being advanced
  * @param[out] out pointer to where the next element is set
  *
  * @return CC_OK if the iterator was advanced, or CC_ITER_END if the
  * end of the CC_Queue has been reached.
  */
 enum cc_stat cc_queue_iter_next(CC_QueueIter *iter, void **out)
 {
     return cc_deque_iter_next(&(iter->i), out);
 }
 
 /**
  * Replaces the last returned element by <code>cc_queue_iter_next()</code>
  * with the specified element and optionally sets the out parameter to
  * the value of the replaced element.
  *
  * @note This function should only ever be called after a call to <code>
  *       cc_queue_iter_next()</code>.
  *
  * @param[in] iter the iterator on which this operation is being performed
  * @param[in] element the replacement element
  * @param[out] out pointer to where the replaced element is stored, or NULL
  *                 if it is to be ignored
  *
  * @return  CC_OK if the element was replaced successfully, or
  * CC_ERR_OUT_OF_RANGE.
  */
 enum cc_stat cc_queue_iter_replace(CC_QueueIter *iter, void *replacement, void **out)
 {
     return cc_deque_iter_replace(&(iter->i), replacement, out);
 }
 
 /**
  * Initializes the zip iterator.
  *
  * @param[in] iter iterator that is being initialized
  * @param[in] q1   first queue
  * @param[in] q2   second queue
  */
 void cc_queue_zip_iter_init(QueueZipIter *iter, CC_Queue *q1, CC_Queue *q2)
 {
     cc_deque_zip_iter_init(&(iter->i), q1->d, q2->d);
 }
 
 /**
  * Outputs the next element pair in the sequence and advances the iterator.
  *
  * @param[in]  iter iterator that is being advanced
  * @param[out] out1 output of the first queue element
  * @param[out] out2 output of the second queue element
  *
  * @return CC_OK if a next element pair is returned, or CC_ITER_END if the end
  * of one of the queues has been reached.
  */
 enum cc_stat cc_queue_zip_iter_next(QueueZipIter *iter, void **out1, void **out2)
 {
     return cc_deque_zip_iter_next(&(iter->i), out1, out2);
 }
 
 /**
  * Replaces the last returned element pair by <code>cc_queue_zip_iter_next()</code>
  * with the specified replacement element pair.
  *
  * @param[in] iter  iterator on which this operation is being performed
  * @param[in]  e1   first queue's replacement element
  * @param[in]  e2   second queue's replacement element
  * @param[out] out1 output of the replaced element from the first queue
  * @param[out] out2 output of the replaced element from the second queue
  *
  * @return CC_OK if the element was successfully replaced, or CC_ERR_OUT_OF_RANGE.
  */
 enum cc_stat cc_queue_zip_iter_replace(QueueZipIter *iter, void *e1, void *e2, void **out1, void **out2)
 {
     return cc_deque_zip_iter_replace(&(iter->i), e1, e2, out1, out2);
 }
 
 
 size_t cc_queue_struct_size()
 {
     return cc_deque_struct_size();
 }