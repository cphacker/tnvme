#ifndef _TNVME_H_
#define _TNVME_H_

#include <stdio.h>
#include <string>
#include <vector>
#include "dnvme.h"

using namespace std;

#define ULONGLONG       unsigned long long
#define ULONG           unsigned long

#define APPNAME         "tnvme"
#define LEVEL           APPNAME
#define LOG_NRM(fmt, ...)    \
    fprintf(stderr, "%s: " fmt "\n", LEVEL, ## __VA_ARGS__);
#define LOG_ERR(fmt, ...)    \
    fprintf(stderr, "%s-err:%s:%d: " fmt "\n", LEVEL, __FILE__, __LINE__, ## __VA_ARGS__);
#ifdef DEBUG
#define LOG_DBG(fmt, ...)    \
    fprintf(stderr, "%s-dbg:%s:%d: " fmt "\n", LEVEL, __FILE__, __LINE__, ## __VA_ARGS__);
#else
#define LOG_DBG(fmt, ...)    ;
#endif

#define MAX_CHAR_PER_LINE_DESCRIPTION       66

typedef enum {
    SPECREV_10b,                // http://www.nvmexpress.org/ spec. rev. 1.0b
    SPECREVTYPE_FENCE           // always must be last element
} SpecRev;

struct TestRef {
    size_t  group;
    size_t  major;
    size_t  minor;
    TestRef() {group = 0; major = 0; minor = 0; }
    TestRef(size_t g, size_t j, size_t n) { group = g; major = j, minor = n; }
};

/**
 * req      group          major         minor     implies
 * -----------------------------------------------------------------------------
 * false     n/a            n/a           n/a      nothing has been requested
 * true   ==ULONG_MAX       n/a           n/a      request all groups all tests
 * true   !=ULONG_MAX  ==ULONG_MAX || ==ULONG_MAX  request spec'd group
 * true   !=ULONG_MAX  !=ULONG_MAX && !=ULONG_MAX  request spec'd test in group
 */
struct TestTarget {
    bool    req;                // requested by cmd line
    TestRef t;
};

struct InformativeGrp {
    bool    req;                // requested by cmd line
    size_t  grpInfoIdx;
};

struct RmmapIo {
    bool               req;     // requested by cmd line
    nvme_io_space      space;
    size_t             offset;
    size_t             size;
    nvme_acc_type      acc;
};

struct WmmapIo {
    bool               req;     // requested by cmd line
    nvme_io_space      space;
    size_t             offset;
    size_t             size;
    unsigned long long value;
    nvme_acc_type      acc;
};

struct CmdLine {
    bool            summary;
    bool            ignore;
    bool            reset;
    size_t          loop;
    SpecRev         rev;
    TestTarget      detail;
    TestTarget      test;
    string          device;
    InformativeGrp  informative;
    vector<TestRef> skiptest;
    RmmapIo         rmmap;
    WmmapIo         wmmap;
};


#endif
