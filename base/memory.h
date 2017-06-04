#ifndef MEMEORY_H
#define MEMORY_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if 0
#include <sys/types.h>		/* for off_t */

#define NT_OVERFLOWLEN		2048
#define NT_UNDERFLOWLEN		NT_OVERFLOWLEN

typedef struct
{
    unsigned int id;
    char *start;
    char *end;
    char *alloc_fn;
} _nt_info;

#define XTN(a)			((char*)(a)-(NT_OVERFLOWLEN+sizeof(_nt_info)))

#define xs_malloc(a)		nt_malloc(a, __FILE__, __LINE__)
#define xs_calloc(a, b)		nt_calloc(a,b, __FILE__, __LINE__)
#define xs_realloc(a, b)	nt_realloc(XTN(a),b, __FILE__, __LINE__)
#define xs_free(a)		nt_free(XTN(a), __FILE__, __LINE__)

#define xs_memcpy(a,b,c,d,e)	nt_memcpy(XTN(a),b,XTN(c),d,e, __FILE__, __LINE__)
#define xs_memset(a,b,c,d)	nt_memset(XTN(a),b,c,d, __FILE__, __LINE__)
#define xs_memcmp(a,b,c,d,e)	nt_memcmp(XTN(a),b,XTN(c),d,e, __FILE__, __LINE__)

#define xs_strdup(a,b)		nt_strdup(XTN(a),b, __FILE__, __LINE__)
#define xs_strndup(a,b,c)	nt_strndup(XTN(a),b,c, __FILE__, __LINE__)
#define xs_strcpy(a,b,c,d)	nt_strcpy(XTN(a),b,XTN(c),d, __FILE__, __LINE__)
#define xs_strncpy(a,b,c,d,e)	nt_strncpy(XTN(a),b,XTN(c),d,e, __FILE__, __LINE__)
#define xs_strcmp(a,b,c,d)	nt_strcmp(XTN(a),b,XTN(c),d, __FILE__, __LINE__)
#define xs_strncmp(a,b,c,d,e)	nt_strncmp(XTN(a),b,XTN(c),d,e, __FILE__, __LINE__)


void *nt_malloc(size_t size, const char *fname, unsigned int line);
void nt_free(void *ptr, const char *fname, unsigned int line);
void *nt_calloc(size_t nmemb, size_t size, const char *fname, unsigned int line);
void *nt_realloc(void *ptr, size_t size, const char *fname, unsigned int line);

void *nt_memcpy(void *dest, off_t o1, const void *src, off_t o2, size_t n, const char *fname, unsigned int line);
void *nt_memset(void *s, off_t o, int c, size_t n, const char *fname, unsigned int line);
int nt_memcmp(const void *s1, off_t o1, const void *s2, off_t o2, size_t n, const char *fname, unsigned int line);
#endif


char *nt_strdup(const char *s, off_t o, const char *fname, unsigned int line);
char *nt_strndup(const char *s, off_t o, size_t n, const char *fname, unsigned int line);
char *nt_strcpy(char *dest, off_t o1, const char *src, off_t o2, const char *fname, unsigned int line);
char *nt_strncpy(char *dest, off_t o1, const char *src, off_t o2, size_t n, const char *fname, unsigned int line);
int nt_strcmp(const char *s1, off_t o1, const char *s2, off_t o2, const char *fname, unsigned int line);
int nt_strncmp(const char *s1, off_t o1, const char *s2, off_t o2, size_t n, const char *fname, unsigned int line);

char *strnstr(const char*, const char*, size_t);
char *strncasestr(const char*, const char*, size_t);
char* stradd(char*, char*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif


#endif /* MEMORY_H */

