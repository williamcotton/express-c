#include "../src/express.h"
#include <stdlib.h>
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void expressFuzz(tape_t *t) {
  t->test("send garbage", ^(tape_t *t) {
    t->sendData("garbage\r\n\r\n");
    t->sendData("GET / HTTP1.1\r\n\r\n");
    t->sendData("GETSDFDFDF / HTTP/1.1\r\n\r\n");
    t->sendData("GET HTTP/1.1\r\n\r\n");
    t->sendData("GET --- HTTP/1.1\r\n\r\n");
    t->sendData("\r\n\r\n");
    t->sendData("\r\n");
    t->sendData("");
    t->sendData("POST / HTTP/1.1\r\nContent-Type: "
                "application/json\r\nContent-Length: 50\r\n\r\ngarbage");
    t->sendData("POST / HTTP/1.1\r\nContent-Type: "
                "application/json\r\nContent-Length: "
                "9999999999999999999999999999\r\n\r\ngarbage");
    t->sendData(
        "POST / HTTP/1.1\r\nContent-Type: application/json\r\n\r\ngarbage");
    t->sendData("POST / HTTP/1.1\r\n\r\ngarbage");
    t->sendData("GET /qs?g=&a-?-&r&=b=a&ge HTTP1.1\r\n\r\n");

    t->test("send lots of garbage", ^(tape_t *t) {
      int multipliers[2] = {1, 64};

      for (int i = 0; i < 2; i++) {
        size_t longStringLen = 1024 * multipliers[i];
        log_info("Testing with %zu bytes", longStringLen);

        char *longString = malloc(longStringLen);

        t->randomString(longString, longStringLen);
        t->sendData(longString);

        memcpy(longString, "GET / HTTP/1.1\r\n", 16);
        t->sendData(longString);

        memcpy(longString, "GET / HTTP/1.1\r\nContent-Type: ", 30);
        t->sendData(longString);

        memcpy(longString,
               "GET / HTTP/1.1\r\nContent-Type: application/json\r\n", 48);
        t->sendData(longString);

        memcpy(longString,
               "POST / HTTP/1.1\r\nContent-Type: "
               "application/json\r\nContent-Length: 50\r\n\r\n",
               71);
        t->sendData(longString);

        memcpy(longString,
               "POST / HTTP/1.1\r\nContent-Type: "
               "application/json\r\nContent-Length: 16384\r\n\r\n",
               74);
        t->sendData(longString);

        t->randomString(longString, longStringLen);
        memcpy(longString, "GET / HTTP/1.1\r\nContent-Type: ", 30);
        memcpy(longString + (longStringLen - 30), "\r\n\r\n", 4);

        t->randomString(longString, longStringLen);
        memcpy(longString, "GET / HTTP/1.1\r\nCookie: ", 24);
        memcpy(longString + (longStringLen - 24), "\r\n\r\n", 4);

        t->randomString(longString, longStringLen);
        memcpy(longString, "GET /qs?", 8);
        memcpy(longString + (longStringLen - 26), " HTTP/1.1\r\n\r\n", 13);

        free(longString);
      }
    });
  });
}
#pragma clang diagnostic pop
