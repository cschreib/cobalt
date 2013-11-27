#ifndef LOCK_FREE_QUEUE_HPP
#define LOCK_FREE_QUEUE_HPP

#include <atomic>
#include <utility>

// Note : implementation is from
// http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448

template<typename T>
class lock_free_queue {
    struct node {
        node() : next(nullptr) {}
        template<typename U>
        node(U&& t) : data(std::forward<U>(t)), next(nullptr) {}

        T     data;
        node* next;
    };

    node* first_;
    std::atomic<node*> dummy_, last_;

public :
    lock_free_queue() {
        // Always keep a dummy separator between head and tail
        first_ = last_ = dummy_ = new node();
    }

    ~lock_free_queue() {
        // Clear the whole queue
        while (first_ != nullptr) {
            node* temp = first_;
            first_ = temp->next;
            delete temp;
        }
    }

    // Disable copy
    lock_free_queue(const lock_free_queue& q) = delete;
    lock_free_queue& operator = (const lock_free_queue& q) = delete;

    // To be called by the 'producer' thread
    template<typename U>
    void push(U&& t) {
        // Add the new item to the queue
        (*last_).next = new node(std::forward<U>(t));
        last_ = (*last_).next;

        // Clear consumed items
        while (first_ != dummy_) {
            node* temp = first_;
            first_ = temp->next;
            delete temp;
        }
    }

    // To be called by the 'consumer' thread
    bool pop(T& t) {
        // Return false if queue is empty
        if (dummy_!= last_) {
            // Consume the value
            t = std::move((*dummy_).next->data);
            dummy_ = (*dummy_).next;
            return true;
        } else {
            return false;
        }
    }

    // To be called by the 'consumer' thread
    bool empty() const {
        return dummy_ == last_;
    }

    // This method should not be used in concurrent situations
    void clear() {
        while (first_ != dummy_) {
            node* temp = first_;
            first_ = temp->next;
            delete temp;
        }

        first_ = dummy_.load();

        while (first_->next != nullptr) {
            node* temp = first_;
            first_ = temp->next;
            delete temp;
        }

        last_ = dummy_ = first_;
    }
};

#endif
