#if defined(__GLIBC__) && !defined(__FreeBSD_kernel__) &&  defined(__CONFIG_H__)
#  error test.h must be #included before system headers
#endif

int main() {
    int x = 1 + 2;
}
