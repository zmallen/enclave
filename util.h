#ifndef UTIL_DOT_H_
#define	UTIL_DOT_H_

size_t bsd_strlcpy(char *, const char *, size_t);
int  may_read(int, void *, size_t);
void must_read(int, void *, size_t);
void must_write(int, void *, size_t);

#define	MAX_PATH 512

#endif
