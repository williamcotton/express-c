#include "../src/express.h"
#include "test-helpers.h"
#include <stdlib.h>
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void expressFuzz(tape_t *t) {
  t->test("send garbage", ^(tape_t *t) {
    sendData("garbage\r\n\r\n");
    sendData("GET / HTTP1.1\r\n\r\n");
    sendData("GETSDFDFDF / HTTP/1.1\r\n\r\n");
    sendData("GET HTTP/1.1\r\n\r\n");
    sendData("GET --- HTTP/1.1\r\n\r\n");
    sendData("\r\n\r\n");
    sendData("\r\n");
    sendData("");
    sendData("POST / HTTP/1.1\r\nContent-Type: "
             "application/json\r\nContent-Length: 50\r\n\r\ngarbage");
    sendData("POST / HTTP/1.1\r\nContent-Type: "
             "application/json\r\nContent-Length: "
             "9999999999999999999999999999\r\n\r\ngarbage");
    sendData(
        "POST / HTTP/1.1\r\nContent-Type: application/json\r\n\r\ngarbage");
    sendData("POST / HTTP/1.1\r\n\r\ngarbage");
    sendData("GET /qs?g=&a-?-&r&=b=a&ge HTTP1.1\r\n\r\n");

    t->test("send lots of garbage", ^(tape_t *t) {
      int multipliers[2] = {1, 64};

      for (int i = 0; i < 2; i++) {
        size_t longStringLen = 1024 * multipliers[i];
        log_info("Testing with %zu bytes", longStringLen);

        char *longString = malloc(longStringLen);

        randomString(longString, longStringLen);
        sendData(longString);

        memcpy(longString, "GET / HTTP/1.1\r\n", 16);
        sendData(longString);

        memcpy(longString, "GET / HTTP/1.1\r\nContent-Type: ", 30);
        sendData(longString);

        memcpy(longString,
               "GET / HTTP/1.1\r\nContent-Type: application/json\r\n", 48);
        sendData(longString);

        memcpy(longString,
               "POST / HTTP/1.1\r\nContent-Type: "
               "application/json\r\nContent-Length: 50\r\n\r\n",
               71);
        sendData(longString);

        memcpy(longString,
               "POST / HTTP/1.1\r\nContent-Type: "
               "application/json\r\nContent-Length: 16384\r\n\r\n",
               74);
        sendData(longString);

        randomString(longString, longStringLen);
        memcpy(longString, "GET / HTTP/1.1\r\nContent-Type: ", 30);
        memcpy(longString + (longStringLen - 30), "\r\n\r\n", 4);

        randomString(longString, longStringLen);
        memcpy(longString, "GET / HTTP/1.1\r\nCookie: ", 24);
        memcpy(longString + (longStringLen - 24), "\r\n\r\n", 4);

        randomString(longString, longStringLen);
        memcpy(longString, "GET /qs?", 8);
        memcpy(longString + (longStringLen - 26), " HTTP/1.1\r\n\r\n", 13);

        free(longString);
      }
    });
  });
}
#pragma clang diagnostic pop
