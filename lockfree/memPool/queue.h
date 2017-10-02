#include <atomic>
#include <memory>
#include <random>
#include <iostream>
#include <mutex>
#include <cassert>

namespace lockfree {
namespace memPool {

template< typename T >
void destroy_at( T *p ) {
    p->~T();
}

#ifndef RANDOM
#define RANDOM 0
#endif

#ifndef HOLDSIZE
#define HOLDSIZE 0
#endif

/*
* lock free queue
* The stack holds an memory pool, that provides the memory
* The memory provides more efficient allocation.
* To prevent ABA problem, every pointer contains also flag (on low bits)
* The queue contains always at least one element - sentinel.
* It's value is unimportant.
* Generally holds, that variable with _f contains flag,
* and thus can not be dereferenced.
*/
template< typename T, size_t PoolAllocatorSize = 2048 >
struct Queue {

    /*
    *Internal structure for holding inserted object
    */
    struct node {
        node( T value ) : _value( value ), _next( nullptr ) {
        }

        node() : _value( T() ), _next( nullptr ) {
        }

        T _value;
        // held node is just a pointer - can contains flag
        std::atomic< node * > _next;
    };

    Queue() : allocator(), head( allocator.construct()), tail( head.load()) {
    }

    /*
    * Method push.
    * returns bool - if the item was successfully inserted.
    * Insert fails only if the memory pool was full.
    */
    bool push( T value ) {
        node *toInsert = allocator.construct( std::move( value ));
        if ( !toInsert )
            return false;

        while ( true ) {
            auto last_f = tail.load();
            auto last = clear( last_f );
            auto next_f = last->_next.load();

            if ( last_f == tail ) {
                if ( next_f == nullptr ) {
                    //try to add me as last
                    if ( last->_next.compare_exchange_weak( next_f, toInsert )) {
                        //I was successful, so try to become the new tail
                        tail.compare_exchange_weak( last_f, toInsert );
                        //if i hasn't been successful it means, that some other node becomes tail
                        return true;
                    }
                } else {
                    //help other node to become a tail
                    tail.compare_exchange_weak( last_f, next_f );
                }
            }
        }
    }

    /*
    * Method pop
    * returns bool - if the item was successfully popped
    * pop fails only if the queue has been empty at given time
    */
    bool pop( T& out ) {
        while ( true ) {
            auto sentinel_f = head.load();
            assert( sentinel_f != nullptr );
            auto sentinel = clear( sentinel_f );
            auto last_f = tail.load();
            auto first_f = sentinel->_next.load();

            if ( sentinel_f == head ) {
                if ( sentinel_f == last_f ) {
                    if ( first_f == nullptr ) {
                        return false;
                    }
                    //help other thread to advance the tail of queue
                    tail.compare_exchange_weak( last_f, first_f );
                } else {
                    if ( head.compare_exchange_weak( sentinel_f, first_f )) {
                        assert( first_f != nullptr );
                        auto first = clear( first_f );
                        out = first->_value;
                        allocator.destruct( sentinel_f );
                        return true;
                    }
                }
            }
        }
    }

    bool empty() {
        return head == tail;
    }

#if HOLDSIZE
    size_t used() {
        return allocator._size;
    }

    size_t available() {
        return PoolAllocatorSize - allocator._size;
    }
#endif

    /*
    * Destructor: expects that no thread access the queue
    * during and after destructor is called
    * If some thread access the queue in destructor, it's considered as
    * undefined behaviour
    */
    ~Queue() {
        auto fst_t = head.load();
        while ( fst_t != nullptr ) {
            auto fst = clear( fst_t );
            auto next = fst->_next.load();
            allocator.destruct( fst_t );
            fst_t = next;
        }
        head = nullptr;
        tail = nullptr;
    }

private:

    /*
    * Pool allocator - the manager of memory in this queue
    * The ability of not having a need to allocate new memory
    * with malloc speeds up implementation.
    * To make this allocator thread safe, the "allocation" is performed
    * by setting an bit into atomic variable _flag.
    * To prevent ABA problem, the pointer obtains a flag.
    *
    * If RANDOM is defined - the place, where an thread starts for looking
    * for an empty place is selected randomly (uniformly)
    *
    * If SIZE is defined - the poolAllocator holds it's size - this can slower
    * the impl, as every thread need to access this variable many times
    */

    template< std::size_t size, typename Allocator = std::allocator< node>>
    class PoolAllocator {
    public:

        using value_type = node;
        using pointer = node *;
        // the number of bits in one size_t (expects to be 64)
        static constexpr size_t max = sizeof( size_t ) * 8;
        // number of chunks of atomic flags needed
        static constexpr size_t chunks = size / max;

        PoolAllocator() : _data( Allocator{ }.allocate( size )),
#if HOLDSIZE
                		  _size(0),
#endif
#if RANDOM
                          generator(),
                          distribution( 0, size )
#else
                          _start(0)
#endif
        {

            static_assert(( size % max ) == 0, "The size of PoolAllocator must be multiple of 64" );
            assert( _data );

            //clear flags
            for ( size_t i = 0; i < chunks; ++i ) {
                _flags[ i ] = 0;
            }

            //the data holds it's last flag for pointers: need to be set to 0 at the begining
            for (size_t i = 0; i < size; ++i) {
                int *flagHolder = reinterpret_cast<int *>(_data + i);
                *flagHolder = 0;
            }
        }

        pointer allocate() {
#if HOLDSIZE
            //eliminates threads from looking for empty place
            // in case that memory is full. happens mostly if
            // the size of queue is too small
            size_t freeSpace = size - _size.fetch_add(1) - 1;
            if (freeSpace <= 1) {
                --_size;
                return nullptr;
            }
#endif

#if RANDOM
            size_t hint = distribution(generator);
#else
            size_t hint = _start.fetch_add( 1 );
            hint %= size;
#endif
            while ( true ) {
                size_t chunkOfft = hint / max;
                for ( size_t i = chunkOfft; i < chunks + chunkOfft; ++i ) {
                    size_t chunk = i % chunks;
                    size_t value = 1;
                    size_t position = 0;
                    while ( position < hint % max ) {
                        value <<= 1;
                        ++position;
                    }
                    hint = 0; //next round continue with position 0
                    while ( position < max ) {
                        auto previous = _flags[ chunk ].fetch_or( value );
                        if ( previous & value ) {
                            value <<= 1;
                            ++position;
                        } else {
                            //got my allocated _data;
                            return _data + ( chunk * max + position );
                        }
                    }
                }
#if HOLDSIZE
                --_size;
#endif
                return nullptr;
            }
            return nullptr;
        }

        bool operator==( const PoolAllocator& other ) const {
            return other == *this;
        }

        bool operator!=( const PoolAllocator& other ) const {
            return !( *this == other );
        }

        /*
        * firstly, allocates (obtains) a new space in memory
        * secondly, constructs object with given arguments.
        * similar to emplace_back on vector.
        * flag is set, before returning an pointer.
        */
        template< class... Args >
        pointer construct( Args&& ... args ) {
            auto data = allocate();
            if ( !data )
                return nullptr;

            int lastFlag = *reinterpret_cast<int *>(data);
            new( data ) node( std::forward< Args >( args )... );

            data = addFlag( data, static_cast<size_t >(lastFlag + 1) );
            return data;
        }

        /*
        * firstly, clear pointer from its flag
        * secondly, call destructor on data,
        * and stores last flag for nex allocation
        * afterwards, the flag on allocated data is set to 0
        * this indicates that the memory is again available
        */
        void destruct( pointer data ) {
            int flag = 0;
            std::tie( data, flag ) = clearFlag( data );
            auto distance = data - _data;
            auto chunk = distance / max;
            auto position = distance % max;
            size_t binary = 1;
            for ( size_t i = 0; i < position; ++i ) {
                binary <<= 1;
            }

            destroy_at<node>( data );
            int *storeLastFlag = reinterpret_cast<int *>(data);
            *storeLastFlag = flag;
            _flags[ chunk ] &= ( ~binary );
#if HOLDSIZE
            --_size;
#endif
        }

        ~PoolAllocator() {
            //destroy unpoped values first
            for (size_t i = 0; i < chunks; ++i ) {
            	size_t value = 1;
                size_t position = 0;
                while( position < max ) {
                    if ( _flags[i] &  value ) {
                        destruct(_data + (i * max + position));
                    }
                    value <<= 1;
                    ++position;
                }
                assert(_flags[i] == 0);
            }
            Allocator{ }.deallocate( _data, size );
        }

    private:
        pointer _data;
        friend Queue;
        std::atomic< size_t > _flags[size / max];
#if HOLDSIZE
        std::atomic<size_t> _size;
#endif

#if RANDOM
        std::default_random_engine generator;
        std::uniform_int_distribution< int > distribution;
#else
        std::atomic< size_t > _start;
#endif
        /*
        * adds flag to pointer to node, defined by number % 3
        * as the flag is set to the two low bits, which are available to this
        * usage thaks to aligned memory, the flags can be only [0,3]
        */
        node *addFlag( node *in, size_t number = 1 ) {
            number %= 3;
            uintptr_t pointer = reinterpret_cast<uintptr_t>(in);
            pointer |= number;
            return reinterpret_cast<node *>(pointer);
        }

        /*
        * clears the flag from a pointer to node, and returns this flag.
        * expects the flag only on 2 low bits.
        */
        std::pair< node *, int > clearFlag( node *toClear ) {
            uintptr_t pointer = reinterpret_cast<uintptr_t>(toClear);
            uintptr_t clearFlag = 0xFFFFFFFFFFFFFFFC;

            int flag = static_cast<int>(pointer & ( ~clearFlag ));

            pointer &= clearFlag;
            return { reinterpret_cast<node *>(pointer), flag };
        }
    };

    PoolAllocator< PoolAllocatorSize > allocator;
    std::atomic< node * > head, tail;

    /*
   * clears the flag from a pointer to node.
   * expects the flag only on 2 low bits
   * used by queue directly, when the pointer need
   * to be dereferenced
   */
    node *clear( node *toClear ) {
        uintptr_t pointer = reinterpret_cast<uintptr_t>(toClear);

        uintptr_t clearFlag = 0xFFFFFFFFFFFFFFFC;

        pointer &= clearFlag;
        return reinterpret_cast<node *>(pointer);
    }
};

} //namespace memPool
} //namespace lockfree