#include<sulog/logger.h>

int main() {
    logger log("testlog");

    for (int i = 0; i < 10000; i++) {
        log.print("Start Write log %d loop",i);
    }
    errno = 6;
    log.error("The error is:");
    return 0;
}
