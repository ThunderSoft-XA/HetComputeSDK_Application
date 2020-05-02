#pragma once

#include <atomic>
#include <mutex>

// for the hetcompute_atomic_thread_fence
#include <hetcompute/internal/atomic/atomicops.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/memorder.hh>

/// set to 1 to serialize the queues for debugging purposes
#define HETCOMPUTE_CLD_SERIALIZE 0

/// set to 1 to force sequential conisitency
#define HETCOMPUTE_CLD_FORCE_SEQ_CST 0

#if HETCOMPUTE_CLD_SERIALIZE
#define HETCOMPUTE_CLD_LOCK std::lock_guard<std::mutex> lock(_serializing_mutex)
#else
#define HETCOMPUTE_CLD_LOCK                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (0)
#endif

#if HETCOMPUTE_CLD_FORCE_SEQ_CST
#define HETCOMPUTE_CLD_MO(x) hetcompute::mem_order_seq_cst
#else
#define HETCOMPUTE_CLD_MO(x) x
#endif

template <typename T>
class chase_lev_array
{
public:
    explicit chase_lev_array(unsigned logsz = 12) : _logsz(logsz), _arr(new std::atomic<T>[ size_t(1) << logsz ]), _next(nullptr)
    {
    }

    ~chase_lev_array()
    {
        delete[] _arr;
    }

    size_t size() const
    {
        return size_t(1) << _logsz;
    }

    T get(size_t i, hetcompute::mem_order mo = HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed))
    {
        return _arr[i & (size() - 1)].load(mo);
    }

    void put(size_t i, T x, hetcompute::mem_order mo = HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed))
    {
        _arr[i & (size() - 1)].store(x, mo);
    }

    chase_lev_array<T>* resize(size_t bot, size_t top)
    {
        auto a = new chase_lev_array<T>(_logsz + 1);

        HETCOMPUTE_INTERNAL_ASSERT(top <= bot, "oops");

        for (size_t i = top; i < bot; ++i)
        {
            a->put(i, get(i));
        }
        return a;
    }

    void set_next(chase_lev_array<T>* next)
    {
        _next = next;
    }

    chase_lev_array<T>* get_next()
    {
        return _next;
    }
private:
    unsigned        _logsz;
    std::atomic<T>* _arr;

    /// maintain a next pointer to chain all allocated chase_lev_array nodes.
    chase_lev_array<T>* _next;

    // OMG!! g++ shut up already!!
    chase_lev_array<T>& operator=(const chase_lev_array<T>&);
    chase_lev_array<T>(const chase_lev_array<T>&);
};

template <typename T, bool Lepop = true>
class chase_lev_deque
{
public:
    static constexpr size_t s_abort = ~size_t(0);

    explicit chase_lev_deque(unsigned logsz = 12)
        : _top(1), // -jx 0?
          _bottom(1),
          _cl_array(new chase_lev_array<T>(logsz)),
          _serializing_mutex()
    {
        _original_head = _cl_array;
    }

    virtual ~chase_lev_deque()
    {
        /// delete all allocated nodes by traversing the chain beginning at _original_head
        HETCOMPUTE_INTERNAL_ASSERT(_original_head != nullptr, "Error. Head is nullptr");
        while (_original_head != nullptr)
        {
            auto cur       = _original_head;
            _original_head = cur->get_next();
            delete cur;
        }
    }

    /// Disallow copy construction, move construction and the overloaded assignment operators
    HETCOMPUTE_DELETE_METHOD(chase_lev_deque(chase_lev_deque const&));
    HETCOMPUTE_DELETE_METHOD(chase_lev_deque(chase_lev_deque&&));
    HETCOMPUTE_DELETE_METHOD(chase_lev_deque& operator=(chase_lev_deque const&));
    HETCOMPUTE_DELETE_METHOD(chase_lev_deque& operator=(chase_lev_deque&&));

    size_t take(T& x)
    {
        HETCOMPUTE_CLD_LOCK;

        // should be acquire?
        size_t b = _bottom.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        ///
        /// If we take() before the first push() then b will wrap on the
        /// store below and if a steal() happens we it will succeed and
        /// thing that it has grabbed something.
        ///
        /// Not sure what will happens when we properly wrap to 0?
        ///
        if (b == 0)
        {
            return 0;
        }

        b      = _bottom.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        auto a = _cl_array.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));

        --b;
        // this should be a release and we can get rid of the fence
        _bottom.store(b, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        hetcompute::internal::hetcompute_atomic_thread_fence(HETCOMPUTE_CLD_MO(hetcompute::mem_order_seq_cst));

        size_t t = _top.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));

        if (b < t)
        {
            // -JX Chase-Lev says t but Le-Pop says b+1?
            if (Lepop)
            {
                _bottom.store(b + 1, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
            }
            else
            {
                _bottom.store(t, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
            }
            return 0;
        }

        // Non-empty queue.
        // First store the current value of x in a local variable.
        // This is then written back into x if the take() method is unsuccessful.
        auto current_x = x;
        x              = a->get(b);
        if (b > t)
        {
            // more than one element
            return b - t;
        }

        size_t sz = 1;
        // Single last element in queue.
        // could use acquire_release instead of seq_cst
        if (!_top.compare_exchange_strong(t, t + 1, HETCOMPUTE_CLD_MO(hetcompute::mem_order_seq_cst), HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed)))
        {
            // Failed race.
            // Write current_x back into x to ensure consistency with other queues.
            x  = current_x;
            sz = 0;
        }

        // -JX Chase-Lev says t+1, but Le-Pop says b+1
        if (Lepop)
        {
            _bottom.store(b + 1, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        }
        else
        {
            _bottom.store(t + 1, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        }

        return sz;
    }

    size_t push(T x)
    {
        HETCOMPUTE_CLD_LOCK;
        size_t b = _bottom.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        size_t t = _top.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_acquire));
        auto   a = _cl_array.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));

        if (b >= t + a->size())
        {
            // Full queue.
            auto old_a = a;
            a          = a->resize(b, t);
            _cl_array.store(a, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
            /// Chain a to old_a so that it can be reclaimed
            old_a->set_next(a);
        }
        a->put(b, x);
        hetcompute::internal::hetcompute_atomic_thread_fence(HETCOMPUTE_CLD_MO(hetcompute::mem_order_release));
        _bottom.store(b + 1, HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));

        return b - t;
    }

    size_t steal(T& x)
    {
        HETCOMPUTE_CLD_LOCK;
        size_t t = _top.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_acquire));
        hetcompute::internal::hetcompute_atomic_thread_fence(HETCOMPUTE_CLD_MO(hetcompute::mem_order_seq_cst));
        size_t b = _bottom.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_acquire));

        if (t >= b)
        {
            return 0;
        }

        // Non-empty queue.
        // Again, as in the take() method, locally store the current value of x.
        auto current_x = x;
        auto a         = _cl_array.load(HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed));
        x              = a->get(t);
        if (!_top.compare_exchange_strong(t, t + 1, HETCOMPUTE_CLD_MO(hetcompute::mem_order_seq_cst), HETCOMPUTE_CLD_MO(hetcompute::mem_order_relaxed)))
        {
            // Failed race. Restore the value of x.
            x = current_x;
            return s_abort;
        }
        return b - t;
    }

    size_t unsafe_size(hetcompute::mem_order order = hetcompute::mem_order_relaxed) const
    {
        auto t = _top.load(HETCOMPUTE_CLD_MO(order));
        auto b = _bottom.load(HETCOMPUTE_CLD_MO(order));

        return b - t;
    }

private:
    // try and keep top and bottom far apart.
    std::atomic<size_t>              _top;
    std::atomic<size_t>              _bottom;
    std::atomic<chase_lev_array<T>*> _cl_array;
    std::mutex                       _serializing_mutex;
    /// Maintain a reference to the original head node of the deque. This is the head of the chain of
    /// allocated nodes, that is traversed for garbage collection.
    chase_lev_array<T>* _original_head;
};
