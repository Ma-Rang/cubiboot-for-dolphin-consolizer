#include <stddef.h>
#include <stdarg.h>

// #define USE_NATIVE_SPRINTF

#ifndef USE_NATIVE_SPRINTF
int rpl_vsnprintf(char *, size_t, const char *, va_list);
int rpl_snprintf(char *, size_t, const char *, ...);
int rpl_vasprintf(char **, const char *, va_list);
int rpl_asprintf(char **, const char *, ...);

#define vsnprintf rpl_vsnprintf
#define snprintf rpl_snprintf
#define vasprintf rpl_vasprintf
#define asprintf rpl_asprintf
#define sprintf(dst, fmt, ...) snprintf(dst, 0xFFFFFFFF, fmt, ##__VA_ARGS__)
#else
int vsnprintf(char* s, size_t n, const char* format, va_list arg);
int vsprintf(char* s, const char* format, va_list arg);
int snprintf(char* s, size_t n, const char* format, ...);
int sprintf(char* s, const char* format, ...);

#endif

int memcmp(const void *m1, const void *m2, size_t n);
void *memmove(void *dst, const void *src, size_t length);
void *memcpy(void* dst, const void* src, size_t count);
void *memset (void *m, int c, size_t n);
char *strcat (char *__restrict s1, const char *__restrict s2);
char *strncpy (char *dst0, const char *src0, size_t count);
char *strcpy (char *dst0, const char *src0);
char *strrchr (const char *s, int i);
char *strchr (const char *s1, int i);
int strcasecmp (const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcmp (const char *s1, const char *s2);
size_t strlen (const char *str);
char *strtok (register char *__restrict s, register const char *__restrict delim);

typedef int cmp_t(const void *, const void *);
void qsort (void *a, size_t n, size_t es, cmp_t *cmp);
