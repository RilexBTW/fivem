/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */
#pragma once
#include <atomic>
#include <malloc.h>
#include "hazard_pointers.h"
#include <mutex>
#include <chrono>
#include <vector>
#include <winternl.h>

constexpr static const long BUFFER_QUEUE_SIZE = 1024;

/**
 * <h1> Fetch-And-Add Array Queue </h1>
 *
 * Each node has one array but we don't search for a vacant entry. Instead, we
 * use FAA to obtain an index in the array, for enqueueing or dequeuing.
 *
 * There are some similarities between this queue and the basic queue in YMC:
 * http://chaoran.me/assets/pdf/wfq-ppopp16.pdf
 * but it's not the same because the queue in listing 1 is obstruction-free, while
 * our algorithm is lock-free.
 * In FAAArrayQueue eventually a new node will be inserted (using Michael-Scott's
 * algorithm) and it will have an item pre-filled in the first position, which means
 * that at most, after BUFFER_QUEUE_SIZE steps, one item will be enqueued (and it can then
 * be dequeued). This kind of progress is lock-free.
 *
 * Each entry in the array may contain one of three possible values:
 * - A valid item that has been enqueued;
 * - nullptr, which means no item has yet been enqueued in that position;
 * - taken, a special value that means there was an item but it has been dequeued;
 *
 * Enqueue algorithm: FAA + CAS(null,item)
 * Dequeue algorithm: FAA + CAS(item,taken)
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers (lock-free)
 * Uncontended enqueue: 1 FAA + 1 CAS + 1 HP
 * Uncontended dequeue: 1 FAA + 1 CAS + 1 HP
 *
 *
 * <p>
 * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
 * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
 * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
 * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class FAAArrayQueue 
{
private:
    template <typename T>
    struct QueueNode
    {
        std::atomic<int> deqidx;
        std::atomic<T*> items[BUFFER_QUEUE_SIZE];
        std::atomic<int> enqidx;
        std::atomic<QueueNode*> next;

        // Start with the first entry pre-filled and enqidx at 1
        QueueNode(T* item) : deqidx{ 0 }, enqidx{ 1 }, next{ nullptr }
        {
            items[0].store(item, std::memory_order_relaxed);
            for (unsigned i = 1; i < BUFFER_QUEUE_SIZE; i++)
            {
                items[i].store(nullptr, std::memory_order_relaxed);
            }
        }

        bool casNext(QueueNode* cmp, QueueNode* val)
        {
            return next.compare_exchange_strong(cmp, val);
        }
    };
private:
    using value_type = T*;
    using Node = QueueNode<T>;

    constexpr static const uintptr_t TAKEN_PTR = ~(uintptr_t)(0);

    bool casTail(Node *cmp, Node *val) 
    {
		return tail.compare_exchange_strong(cmp, val);
	}

    bool casHead(Node *cmp, Node *val) 
    {
        return head.compare_exchange_strong(cmp, val);
    }

    static value_type takenPtr()
    {
        return (value_type)TAKEN_PTR;
    }

    HazardPointersManager<Node> m_hazardManagers;

    static constexpr unsigned step_size = 11;
    static constexpr unsigned max_idx = step_size * 512;

    // Pointers to head and tail of the list
    alignas(64) std::atomic<Node*> head;
    alignas(64) std::atomic<Node*> tail;
public:
    using HazardToken = HazardPointer<Node>;

    FAAArrayQueue()
    {
        Node* sentinelNode = new Node(nullptr);
        sentinelNode->enqidx.store(0, std::memory_order_relaxed);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }

    ~FAAArrayQueue() 
    {
        while (pop() != nullptr); // Drain the queue
        delete head.load();           // Delete the last node
    }
    
    inline HazardToken getHazardToken()
    {
        return { m_hazardManagers };
    }
private:
    void enqueueInternal(value_type item, HazardToken& hazard)
    {
        if (item == nullptr)
        {
            return;
        }

        for (;;)
        {
            Node* lTail = hazard.record->setHazardPtr(tail);
            const int idx = lTail->enqidx.fetch_add(step_size, std::memory_order_relaxed);
            if (idx >= BUFFER_QUEUE_SIZE)
            { // This node is full
                if (lTail != tail.load(std::memory_order_acquire))
                {
                    continue;
                }

                Node* lNext = lTail->next.load(std::memory_order_relaxed);
                if (lNext == nullptr) 
                {
                    Node* newNode = m_hazardManagers.dequeueDelete();
                    if (!newNode)
                    {
                        newNode = (Node*)malloc(sizeof(Node));
                    }
                    newNode = new (newNode) Node(item);
                    if (lTail->casNext(nullptr, newNode))
                    {
                        casTail(lTail, newNode);
                        hazard.record->reset();
                        return;
                    }
                    delete newNode;
                } 
                else 
                {
                    casTail(lTail, lNext);
                }
                continue;
            }
            value_type itemNull = nullptr;
            if (lTail->items[idx].compare_exchange_strong(itemNull, item))
            {
                hazard.record->reset();
                return;
            }
        }
    }

    value_type dequeueInternal(HazardToken& hazard)
    {
        for (;;)
        {
            Node* lHead = hazard.record->setHazardPtr(head);

            const int popIdx = lHead->deqidx.load(std::memory_order_acquire);
            const auto pushIdx = lHead->enqidx.load(std::memory_order_relaxed);

            if (popIdx >= pushIdx && lHead->next.load(std::memory_order_relaxed) == nullptr)
            {
                break;
            }

            unsigned idx = lHead->deqidx.fetch_add(step_size, std::memory_order_release);
            // This node has been drained, check if there is another one
            if (idx > BUFFER_QUEUE_SIZE)
            {
                Node* lnext = lHead->next.load(std::memory_order_acquire);
                if (lnext == nullptr)
                {
                    //No more nodes in the queue
                    break; 
                }

                //if ( casHead(lHead, lnext))
                if (head.compare_exchange_strong(lHead, lnext, std::memory_order_release, std::memory_order_relaxed))
                {
                    hazard.record->reset();
                    m_hazardManagers.enqueueDelete(lHead);
                }
                continue;
            }

            value_type item = lHead->items[idx].exchange(takenPtr(), std::memory_order_relaxed);
            if (item == nullptr)
            {
                continue;
            }
            hazard.record->reset();
            return item;
        }
        hazard.record->reset();
        return nullptr;
    }
public:
    value_type pop()
    {
        HazardToken token{ m_hazardManagers };
        return dequeueInternal(token);
    }

    void push(value_type item)
    {
        HazardToken token{ m_hazardManagers };
        enqueueInternal(item, token);
    }

    [[nodiscard]] bool try_pop(value_type& item)
    {   
        HazardToken token{ m_hazardManagers };
        item = dequeueInternal(token);
        if (item == nullptr)
        {
            return false;
        }
        return true;
    }
};