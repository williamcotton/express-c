#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

char *getStatusMessage(int status);

void statusMessageTests(tape_t *t) {
  t->test("statusMessage", ^(tape_t *t) {
    t->ok("100", strlen(getStatusMessage(100)));
    t->ok("101", strlen(getStatusMessage(101)));
    t->ok("102", strlen(getStatusMessage(102)));
    t->ok("103", strlen(getStatusMessage(103)));
    t->ok("200", strlen(getStatusMessage(200)));
    t->ok("201", strlen(getStatusMessage(201)));
    t->ok("202", strlen(getStatusMessage(202)));
    t->ok("203", strlen(getStatusMessage(203)));
    t->ok("204", strlen(getStatusMessage(204)));
    t->ok("205", strlen(getStatusMessage(205)));
    t->ok("206", strlen(getStatusMessage(206)));
    t->ok("207", strlen(getStatusMessage(207)));
    t->ok("208", strlen(getStatusMessage(208)));
    t->ok("226", strlen(getStatusMessage(226)));
    t->ok("300", strlen(getStatusMessage(300)));
    t->ok("301", strlen(getStatusMessage(301)));
    t->ok("302", strlen(getStatusMessage(302)));
    t->ok("303", strlen(getStatusMessage(303)));
    t->ok("304", strlen(getStatusMessage(304)));
    t->ok("305", strlen(getStatusMessage(305)));
    t->ok("306", strlen(getStatusMessage(306)));
    t->ok("307", strlen(getStatusMessage(307)));
    t->ok("308", strlen(getStatusMessage(308)));
    t->ok("400", strlen(getStatusMessage(400)));
    t->ok("401", strlen(getStatusMessage(401)));
    t->ok("402", strlen(getStatusMessage(402)));
    t->ok("403", strlen(getStatusMessage(403)));
    t->ok("404", strlen(getStatusMessage(404)));
    t->ok("405", strlen(getStatusMessage(405)));
    t->ok("406", strlen(getStatusMessage(406)));
    t->ok("407", strlen(getStatusMessage(407)));
    t->ok("408", strlen(getStatusMessage(408)));
    t->ok("409", strlen(getStatusMessage(409)));
    t->ok("410", strlen(getStatusMessage(410)));
    t->ok("411", strlen(getStatusMessage(411)));
    t->ok("412", strlen(getStatusMessage(412)));
    t->ok("413", strlen(getStatusMessage(413)));
    t->ok("414", strlen(getStatusMessage(414)));
    t->ok("415", strlen(getStatusMessage(415)));
    t->ok("416", strlen(getStatusMessage(416)));
    t->ok("417", strlen(getStatusMessage(417)));
    t->ok("418", strlen(getStatusMessage(418)));
    t->ok("421", strlen(getStatusMessage(421)));
    t->ok("422", strlen(getStatusMessage(422)));
    t->ok("423", strlen(getStatusMessage(423)));
    t->ok("424", strlen(getStatusMessage(424)));
    t->ok("425", strlen(getStatusMessage(425)));
    t->ok("426", strlen(getStatusMessage(426)));
    t->ok("428", strlen(getStatusMessage(428)));
    t->ok("429", strlen(getStatusMessage(429)));
    t->ok("431", strlen(getStatusMessage(431)));
    t->ok("451", strlen(getStatusMessage(451)));
    t->ok("500", strlen(getStatusMessage(500)));
    t->ok("501", strlen(getStatusMessage(501)));
    t->ok("502", strlen(getStatusMessage(502)));
    t->ok("503", strlen(getStatusMessage(503)));
    t->ok("504", strlen(getStatusMessage(504)));
    t->ok("505", strlen(getStatusMessage(505)));
    t->ok("506", strlen(getStatusMessage(506)));
    t->ok("507", strlen(getStatusMessage(507)));
    t->ok("508", strlen(getStatusMessage(508)));
    t->ok("510", strlen(getStatusMessage(510)));
    t->ok("511", strlen(getStatusMessage(511)));
    t->ok("0", strlen(getStatusMessage(0)));
  });
}
#pragma clang diagnostic pop
