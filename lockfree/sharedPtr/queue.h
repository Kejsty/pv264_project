#pragma once
#include <memory>
#include <atomic>
#include <cassert>
#include <iostream>

namespace lockfree {
namespace sharedPtr {

/*
* parallel queue. Its lock-freeness depends
* on platform
* nodes holds as share pointers to prevent problem
* with memory leaks
*/
template< typename T >
struct Queue {

    struct node {
        node( T value ) : _value( value ), _next( nullptr ) {
        }

        T _value;
        std::shared_ptr< node > _next;
    };

    Queue() : head( new node( T())), tail( head ) {
        std::cerr << "This queue is ";
        if ( !atomic_is_lock_free( &head ))
            std::cerr << "not ";
        std::cerr << "lock-free\n";
    }

    bool push( T value ) {
        auto toInsert = std::make_shared< node >( std::move( value ));

        while ( true ) {
            auto last = atomic_load( &tail );
            auto next = atomic_load( &last->_next );

            if ( last == tail ) {
                if ( next == nullptr ) {
                    //try to add me as last
                    if ( atomic_compare_exchange_weak( &last->_next, &next, toInsert )) {
                        //I was successful, so I try to be the new tail
                        atomic_compare_exchange_weak( &tail, &last, toInsert );
                        //if i hasn't been successful it means, that some other node becomes tail
                        return true;
                    }
                } else {
                    //help other node to become a tail
                    atomic_compare_exchange_weak( &tail, &last, next );
                }
            }
        }
    }

    bool pop( T& out ) {
        while ( true ) {
            auto sentinel = atomic_load( &head );
            auto last = atomic_load( &tail );;
            auto first = atomic_load( &sentinel->_next );

            if ( sentinel == head ) {
                if ( sentinel == last ) {
                    if ( first == nullptr ) {
                        return false;
                    }
                    //help other thread to advance the tail of queue
                    atomic_compare_exchange_weak( &tail, &last, first );
                } else {
                    if ( atomic_compare_exchange_weak( &head, &sentinel, first )) {
                        out = first->_value;
                        return true;
                    }
                }
            }
        }
    }

private:
    std::shared_ptr< node > head, tail;
};

} //namespace sharedPtr
} //namespace lockfree