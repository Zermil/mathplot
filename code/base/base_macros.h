#ifndef BASE_MACROS_H
#define BASE_MACROS_H

#ifndef NDEBUG
# define Assert(expr)                               \
    do {                                            \
        if (!(expr)) (*((volatile int *)0) = 0);    \
    } while(0)
#else
# define Assert(expr) ((void)(expr))
#endif

#define internal static
#define global static

#ifdef _MSC_VER
# define thread_var __declspec(thread)
#else
# error No thread_var variable defined for current compiler
#endif

#define UNUSED(x) ((void)(x))

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)
#define GB(x) ((x) << 30)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SIGN(a) ((a) < 0 ? -1 : 1)
#define BETWEEN(a, top, bot) ((bot) <= (a) && (a) <= (top))

// @Note: These are used when allocating, so we don't access memory we have not commited yet
#ifndef NDEBUG
extern "C" void __asan_poison_memory_region(void *, size_t);
extern "C" void __asan_unpoison_memory_region(void *, size_t);
# define ASAN_MEM_POISON(mem, size) __asan_poison_memory_region((mem), (size))
# define ASAN_MEM_UNPOISON(mem, size) __asan_unpoison_memory_region((mem), (size))
#else
# define ASAN_MEM_POISON(mem, size) (UNUSED(mem), UNUSED(size))
# define ASAN_MEM_UNPOISON(mem, size) (UNUSED(mem), UNUSED(size))
#endif

#define IS_POW2(val) (((val) & ((val) - 1)) == 0)
// @Note: 'val' becomes a multiple of 'pow', for more information you can read 'Hacker's Delight (2nd Edition) -- Chapter 3'
#define ALIGN_POW2(val, pow) (((val) + (pow) - 1) & (~((pow) - 1)))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#define AS_BOOL(x) ((x) != 0)
#define SWAP(a, b, t)                           \
    do {                                        \
        t temp = (a);                           \
        (a) = (b);                              \
        (b) = temp;                             \
    } while (0);

#define FREE_LIST_ALLOC(item) ((item) == 0 ? 0 : ((item) = (item)->next))
#define FREE_LIST_RELEASE(ffree, item) ((item)->next = (ffree), (ffree) = (item))

#define SLLQueuePush_N(first, last, item, next) \
    do {                                        \
        if ((first) == 0) {                     \
            (first) = (last) = (item);          \
            (item)->next = 0;                   \
        } else {                                \
            (last)->next = (item);              \
            (last) = (item);                    \
            (item)->next = 0;                   \
        }                                       \
    } while(0)

#define SLLQueuePop_N(first, last, next)        \
    do {                                        \
        if ((first) == (last)) {                \
            (first) = (last) = 0;               \
        } else {                                \
            (first) = (first)->next;            \
        }                                       \
    } while(0)

#define SLLQueuePush(first, last, item) SLLQueuePush_N((first), (last), (item), next)
#define SLLQueuePop(first, last) SLLQueuePop_N((first), (last), next)

#define SLLStackPush_N(first, item, next)       \
    do {                                        \
        (item)->next = (first);                 \
        (first) = (item);                       \
    } while(0)

#define SLLStackPop_N(first, next)              \
    do {                                        \
        (first) = (first)->next;                \
    } while(0)

#define SLLStackPush(first, item) SLLStackPush_N(first, item, next)
#define SLLStackPop(first) SLLStackPop_N(first, next)

#include <string.h>
#define MemoryZero(p, size) memset((p), 0, (size))
#define MemoryCopy(dst, src, size) memmove((dst), (src), (size))
#define MemoryCopyStruct(dst, src) MemoryCopy((dst), (src), MIN(sizeof(*(dst)), sizeof(*(src))))

#endif // BASE_MACROS_H
