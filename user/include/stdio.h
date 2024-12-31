#ifndef __STDIO_H__
#define __STDIO_H__

#define STDIN 0
#define STDOUT 1
#define STDERR 2

//#define TEST_START(x) puts(x)
#define TEST_START(x) puts("========== START ");puts(x);puts(" ==========\n");
#define TEST_END(x) puts("========== END ");puts(x);puts(" ==========\n");

#define stdin STDIN
#define stdout STDOUT
#define stderr STDERR

#define va_start(ap, last) (__builtin_va_start(ap, last))
#define va_arg(ap, type) (__builtin_va_arg(ap, type))
#define va_end(ap) (__builtin_va_end(ap))
#define va_copy(d, s) (__builtin_va_copy(d, s))
#define do_div(n, base) ({                                                       \
                          unsigned int __base = (base);                          \
                          unsigned int __rem;                                    \
                          if (((n) >> 32) == 0) {                                \
                            __rem = (unsigned int)(n) % __base;                  \
                            (n) = (unsigned int)(n) / __base;                    \
                          } else                                                 \
                            __rem = __div64_32((unsigned long *)(&(n)), __base); \
                          __rem;                                                 \
                        })

typedef __builtin_va_list va_list;
typedef unsigned long int uintmax_t;
typedef long int intmax_t;

#define ZEROPAD 1  /* pad with zero 填补0*/
#define SIGN 2  /* unsigned/signed long */
#define PLUS 4  /* show plus 显示+*/
#define SPACE 8  /* space if plus 加上空格*/
#define LEFT 16  /* left justified 左对齐*/
#define SPECIAL 32  /* 0x /0*/
#define LARGE 64  /* 用 'ABCDEF'/'abcdef' */

int getchar();
int putchar(int);
int puts(const char* s);
int printf(const char* fmt, ...);
int sprintf(char * buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);
static int skip_atoi(const char **s);
static char * number(char * str, unsigned long long num, int base, int size, int precision, int type);
unsigned int __div64_32(unsigned long *n, unsigned int base);
#endif // __STDIO_H__
