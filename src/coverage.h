
#if defined(COVERAGE_TEST)
# include <assert.h>
#endif

#if defined(__GNUC__) && 0
# define likely(X)    __builtin_expect((X),1)
# define unlikely(X)  __builtin_expect((X),0)
#else
# define likely(X)    !!(X)
# define unlikely(X)  !!(X)
#endif

# if 0 // apparently these macros are introducing more problems than they solve
#if defined(COVERAGE_TEST)
# define ALWAYS(X)      (1)
# define NEVER(X)       (0)
//# define ALWAYS(X)      ((X)?0:(assert(0),0))
//# define NEVER(X)       ((X)?(assert(0),0):0)
#else
# define ALWAYS(X)      (likely(X))
# define NEVER(X)       (unlikely(X))
#endif
#endif
