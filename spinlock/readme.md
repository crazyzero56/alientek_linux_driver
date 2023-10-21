# 將補釘打到linux 記得加--ignore-whitespace 不然會patch fail.
patch -p1 --ignore-whitespace --dry-run < gpioled.patch
