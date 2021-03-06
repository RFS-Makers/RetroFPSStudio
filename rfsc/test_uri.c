// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include <assert.h>
#include <check.h>

#include "uri.h"

#include "testmain.h"

START_TEST (test_uribasics)
{
    uriinfo *uri = uri_ParseEx("file:///a%20b", NULL);
    assert(strcmp(uri->protocol, "file") == 0);
    #if defined(_WIN32) || defined(_WIN64)
    assert(strcmp(uri->path, "\\a b") == 0);
    #else
    assert(strcmp(uri->path, "/a b") == 0);
    #endif
    uri_Free(uri);

    uri = uri_ParseEx("/a%20b", NULL);
    assert(strcmp(uri->protocol, "file") == 0);
    #if defined(_WIN32) || defined(_WIN64)
    assert(strcmp(uri->path, "\\a%20b") == 0);
    #else
    assert(strcmp(uri->path, "/a%20b") == 0);
    #endif
    uri_Free(uri);

    uri = uri_ParseEx("test.com:20/blubb", NULL);
    assert(!uri->protocol);
    assert(strcmp(uri->host, "test.com") == 0);
    assert(uri->port == 20);
    assert(strcmp(uri->path, "/blubb") == 0);
    uri_Free(uri);

    uri = uri_ParseEx("example.com:443", "https");
    assert(strcmp(uri->protocol, "https") == 0);
    assert(strcmp(uri->host, "example.com") == 0);
    assert(uri->port == 443);
    uri_Free(uri);

    uri = uri_ParseEx("http://blubb/", "https");
    assert(strcmp(uri->protocol, "http") == 0);
    assert(uri->port < 0);
    assert(strcmp(uri->host, "blubb") == 0);
    uri_Free(uri);
}
END_TEST

TESTS_MAIN(test_uribasics)
