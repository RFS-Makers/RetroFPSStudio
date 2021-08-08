// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "json.h"

#include "testmain.h"


static char json_test[] = "{"
    "\"letterWidth\": 5,"
    "\"letterHeight\": 7,"
    "\"letters\": \"1234567890.,!?ABCFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-/\\\\:;$^()[]@'\\\"+✓⏎✗⌫■><&↑↓©\""
    "}";

START_TEST (test_json)
{
    jsonvalue *v = json_Parse(json_test);
    assert(v != NULL);
    json_Free(v);
}
END_TEST

TESTS_MAIN(test_json)
