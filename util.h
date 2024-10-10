#ifndef _UTIL_H_
#define _UTIL_H_

/* --- portable basics --- */
#ifndef NULL
#define NULL 0L
#endif

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif

/* path separator */
#if defined(_WIN32) || defined(__MINGW32__) || defined(WIN32)
#define DIR_SEP '\\'
#else
#define DIR_SEP '/'
#endif

/* min/max helpers */
#ifndef max
#define max(A,B) (((A)>(B)) ? (A) : (B))
#define min(A,B) (((A)>(B)) ? (B) : (A))
#endif

/* round() macro fallback (if math.h didn't provide one we use the int version) */
#ifndef round
#define round(X) (((X) >= 0) ? (int)((X)+0.5) : (int)((X)-0.5))
#endif

#ifndef MAXPATHLENGTH
#define MAXPATHLENGTH 256
#endif

#endif /* _UTIL_H_ */
