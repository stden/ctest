#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include "CTest.h"

int max_int(int count, ...) {
    int max = 0;
    int tmp = 0;
    int i = 0;
    va_list args;
    va_start(args, count);
    // First argument as initial value
    max = va_arg(args, int);

    // Rest arguments
    for(i = 0; i < count - 1; i++) {
        tmp = va_arg(args, int);

        if(tmp > max) {
            max = tmp;
        }
    }

    va_end(args);
    return max;
}


typedef struct CTestRegistry {
    unsigned int number_of_suites; // Number of registered suites in the registry
    unsigned int number_of_tests;  // Total number of registered tests in the registry
    CTestSuite*  suite;            // Pointer to the 1st suite in the test registry
} CTestRegistry;

// Types of failures occurring during test runs
typedef enum CTest_FailureTypes {
    CUF_SuiteInactive = 1,    // Inactive suite was run
    CUF_SuiteInitFailed,      // Suite initialization function failed
    CUF_SuiteCleanupFailed,   // Suite cleanup function failed
    CUF_TestInactive,         // Inactive test was run
    CUF_AssertFailed          // CTest assertion failed during test run
} CTest_FailureType;          // Failure type

// Data type for holding assertion failure information (linked list)
typedef struct CTest_FailureRecord {
    CTest_FailureType  type;        // Failure type
    unsigned int    line;           // Line number of failure
    char*           file;           // Name of file where failure occurred
    char*           message;        // Test condition which failed
    CTestCase*      test;           // Test containing failure
    CTestSuite*     suite;          // Suite containing test having failure
    struct CTest_FailureRecord* prev, *next;
} CTest_FailureRecord;

// Data type for holding statistics and assertion failures for a test run
typedef struct CTestRunSummary {
    unsigned int suites_run;        // Number of suites completed during run
    unsigned int suites_failed;     // Number of suites for which initialization failed
    unsigned int suites_inactive;   // Number of suites which were inactive
    unsigned int tests_run;         // Number of tests completed during run
    unsigned int tests_failed;      // Number of tests containing failed assertions
    unsigned int tests_inactive;    // Number of tests which were inactive (in active suites)
    unsigned int asserts;           // Number of assertions tested during run
    unsigned int asserts_failed;    // Number of failed assertions
    unsigned int failure_records;   // Number of failure records generated
    double       elapsed_time;      // Elapsed time for run in seconds
} CTestRunSummary;

int test_is_running = 0;

CTestSuite* cur_suite = NULL;
CTestCase* cur_test  = NULL;

// Global test registry
CTestRegistry registry = {0, 0, NULL};

CTestRunSummary summary = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

CTest_FailureRecord* failure_list = NULL;
CTest_FailureRecord* last_failure = NULL;

// Variable for storage of start time for test run
clock_t start_time;

// Allocate memory + sprintf
// Caller function must call free(str)
char* CT_avsprintf(const char* format, va_list args) {
    // Dynamically calculate length of buffer
    int bytes = vsnprintf(NULL, 0, format, args);
    char* buf = malloc(bytes + 1);
    vsprintf(buf, format, args); // Print to buf
    return buf;
}

char* CT_asprintf(const char* format, ...) {
    va_list args;

    va_start(args, format);
    char* buf = CT_avsprintf(format, args);
    va_end(args);

    return buf;
}

// Print both the console (encoded in console encoding) and file
void xprintf(const char* format, ...) {
    va_list args;

#ifdef WIN32
    // Print to file
    wchar_t szDirectory[MAX_PATH] = L"";
    wchar_t wName[MAX_PATH + 1] = {0};
    GetCurrentDirectory(sizeof(szDirectory) - 1, szDirectory);
    wsprintf(wName, L"%ls\\%ls", szDirectory, TEXT("debug\\TRACE.TXT"));

    FILE* log = _wfopen(wName, TEXT("a"));
#else
    FILE* log = fopen("CONSOLE.TXT", "a");
#endif

    va_start(args, format);
    char* buf = CT_avsprintf(format, args);
    va_end(args);

    if(log) {
        fputs(buf, log);
        fclose(log);
    }

#ifdef WIN32
    // UTF-8 => UTF-16
    // Determine the length of the buffer
    int len = MultiByteToWideChar(CP_UTF8, 0, buf, -1, 0, 0);
    wchar_t* lpWideCharStr = (wchar_t*)malloc(sizeof(wchar_t) * len + 1);
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, lpWideCharStr, len);
    // UTF-16 => DOS (CP866) for Windows console
    int len2 = WideCharToMultiByte(CP_OEMCP, 0, lpWideCharStr, -1, 0, 0, 0, 0);
    char* dos = (char*)malloc(sizeof(char) * len2);
    WideCharToMultiByte(CP_OEMCP, // CodePage
                        0, // dwFlags
                        lpWideCharStr, // lpWideCharStr - Pointer to the Unicode string to convert
                        -1, // cchWideChar
                        dos, // lpMultiByteStr - Pointer to a buffer that receives the converted string.
                        len2, // cbMultiByte - Size, in bytes, of the buffer indicated by lpMultiByteStr
                        0, // lpDefaultChar
                        NULL // lpUsedDefaultChar
                       );
    printf(dos);
    free(lpWideCharStr);
    free(dos);
#else
    printf(buf);
#endif

    free(buf);
}

// Fatal error
void error(const char* message) {
    xprintf("ERROR: %s\n", message);
    exit(1);
}

void add_failure(CTest_FailureRecord** ppFailure, CTest_FailureType type, unsigned int line, const char* szCondition, const char* file, CTestSuite* suite, CTestCase* test) {
    CTest_FailureRecord* fn = NULL;
    CTest_FailureRecord* temp = NULL;

    assert(NULL != ppFailure);

    fn = (CTest_FailureRecord*)malloc(sizeof(CTest_FailureRecord));

    if(NULL == fn) {
        return;
    }

    fn->file = NULL;
    fn->message = NULL;

    if(NULL != file) {
        fn->file = (char*)malloc(strlen(file) + 1);

        if(NULL == fn->file) {
            free(fn);
            return;
        }

        strcpy(fn->file, file);
    }

    if(NULL != szCondition) {
        fn->message = (char*)malloc(strlen(szCondition) + 1);

        if(NULL == fn->message) {
            if(NULL != fn->file) {
                free(fn->file);
            }

            free(fn);
            return;
        }

        strcpy(fn->message, szCondition);
    }

    fn->type = type;
    fn->line = line;
    fn->test = test;
    fn->suite = suite;
    fn->next = NULL;
    fn->prev = NULL;

    temp = *ppFailure;

    if(NULL != temp) {
        while(NULL != temp->next) {
            temp = temp->next;
        }

        temp->next = fn;
        fn->prev = temp;
    } else {
        *ppFailure = fn;
    }

    ++(summary.failure_records);

    last_failure = fn;
}

// Basic assert
int CTest(int condition, const char* message, const char* file, const int line) {
    assert(NULL != cur_suite);
    assert(NULL != cur_test);

    ++summary.asserts;

    if(!condition) {
        ++summary.asserts_failed;
        add_failure(&failure_list, CUF_AssertFailed, line, message, file, cur_suite, cur_test);
    }

    return condition;
}

// Basic assert
int CTestFatal(int condition, const char* message, const char* file, const int line) {
    assert(NULL != cur_suite);
    assert(NULL != cur_test);

    ++summary.asserts;

    if(!condition) {
        ++summary.asserts_failed;
        add_failure(&failure_list, CUF_AssertFailed, line, message, file, cur_suite, cur_test);

        if(NULL != cur_test->jumpBuf) {
            longjmp(*(cur_test->jumpBuf), 1);
        }
    }

    return condition;
}

void CTestStrings(const char* actual, // Actual string
                  const char* expected, // Expected string
                  const char* message, // Message
                  const char* file, const int line
                 ) {
    if(NULL == actual) {
        char* buf = CT_asprintf("%s\n ACTUAL is NULL", message);
        CTest(0, buf, file, line);
        free(buf);
        return;
    }

    int res = strcmp(actual, expected);
    int i;
    char sa[80] = {0}, se[80] = {0};

    if(res) {
        const char* a = actual, *e = expected;

        while((*a != '\0') && (*e != '\0') && (*a == *e)) {
            a++;
            e++;
        }

        // Comparison two strings in two neighboring lines
        for(i = 0; i < 79; i++) {
            sa[i] = *a;
            se[i] = *e;
            a++;
            e++;
        }

        sa[i] = '\0';
        se[i] = '\0';
        char* buf = CT_asprintf("%s\n === actual ===\n\"%s\"\n === expected===\n\"%s\"\n[a]%s\n[e]%s", message, actual, expected, sa, se);
        CTest(!res, buf, file, line);
        free(buf);
    }

    CTest(!res, message, file, line);
}

// Compare integers
void CTestInt(uint64_t a, uint64_t e, const char* message, const char* file, const int line) {
    if(a != e) {
        char* buf = CT_asprintf("actual=%I64d expected=%I64d %s\n", a, e, message);
        CTest(0, buf, file, line);
        free(buf);
    }

    CTest(1, message, file, line);
}

// == Compare files ==
// Input parameters:
//   filename_actual - actual file
//   filename_expected - file standard to verify
// Return:
//   error message or NULL - OK
char* compare_files(char* filename_actual, char* filename_expected) {
    FILE* a = fopen(filename_actual, "r");

    if(!a) {
        return CT_asprintf("File \"%s\" not found!\n", filename_actual);
    }

    FILE* b = fopen(filename_expected, "r");

    if(!b) {
        return CT_asprintf("File \"%s\" not found!\n", filename_expected);
    }

    int pos = 0;
    char ca, ce;

    while(!feof(a) && !feof(b)) {
        ca = fgetc(a);
        ce = fgetc(b);
        pos++;

        if(feof(a) || feof(b)) {
            break;
        }

        if(ca != ce) {
            fclose(a);
            fclose(b);
            return CT_asprintf("\"%s\" \"%s\" diff pos %d '%c'!='%c'\n", filename_actual, filename_expected, pos, ca, ce);
        }
    }

    if(feof(a) && !feof(b)) {
        fclose(a);
        fclose(b);
        return CT_asprintf("size(\"%s\") < size(\"%s\")\n", filename_actual, filename_expected);
    }

    if(!feof(a) && feof(b)) {
        fclose(a);
        fclose(b);
        return CT_asprintf("size(\"%s\") > size(\"%s\")\n", filename_actual, filename_expected);
    }

    fclose(a);
    fclose(b);
    return NULL;
}

// == Files compare ==
// Input parameters:
//   filename_actual - file with the results of processing
//   filename_expected - expected file
//   uiLine - program line with ASSERT
//   message - condition description
//   strFile - sorce file name
//   bFatal - stop testings if this test fails
void CTestFiles(char* filename_actual, char* filename_expected, const char* message, const char* file, const int line) {
    assert(NULL != cur_suite);
    assert(NULL != cur_test);

    ++summary.asserts;

    // Run file comparison
    char* error = compare_files(filename_actual, filename_expected);

    if(NULL != error) {
        ++summary.asserts_failed;

        // Prepair error message
        char* msg = CT_asprintf("%s %s", message, error);
        add_failure(&failure_list, CUF_AssertFailed, line, msg, file, cur_suite, cur_test);
        free(msg);
        free(error);
    }
}

void CTestFilesFatal(char* filename_actual, char* filename_expected, const char* message, const char* file, const int line) {
    assert(NULL != cur_suite);
    assert(NULL != cur_test);

    ++summary.asserts;

    // Run file comparison
    char* error = compare_files(filename_actual, filename_expected);

    if(NULL != error) {
        ++summary.asserts_failed;

        // Prepair error message
        char* msg = CT_asprintf("%s %s", message, error);
        add_failure(&failure_list, CUF_AssertFailed, line, msg, file, cur_suite, cur_test);
        free(msg);
        free(error);

        if(NULL != cur_test->jumpBuf) {
            longjmp(*(cur_test->jumpBuf), 1);
        }
    }
}

double get_elapsed_time(void) {
    if(test_is_running) {
        return ((double)clock() - (double)start_time) / (double)CLOCKS_PER_SEC;
    } else {
        return summary.elapsed_time;
    }
}

// Pointer to the currently running suite
CTestSuite* running_suite = NULL;

/** Handler function called at completion of each test.
 *  @param test   The test being run.
 *  @param suite  The suite containing the test.
 *  @param pFailure Pointer to the 1st failure record for this test.
 */
void basic_test_complete_message_handler(const CTestCase* test, const CTestSuite* suite, CTest_FailureRecord* pFailureList) {
    CTest_FailureRecord* failure = pFailureList;
    int i;
#ifdef WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
#endif

    assert(NULL != suite);
    assert(NULL != test);

    if(NULL == failure) {
#ifdef WIN32
        // Save attributes
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        SetConsoleTextAttribute(hConsole, 10);
#endif
        xprintf("OK");
#ifdef WIN32
        SetConsoleTextAttribute(hConsole, csbi.wAttributes);
#endif
    } else {
#ifdef WIN32
        // Save attributes
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        SetConsoleTextAttribute(hConsole, 12);
#endif
        xprintf("FAIL");
#ifdef WIN32
        SetConsoleTextAttribute(hConsole, csbi.wAttributes);
#endif

        for(i = 1 ; (NULL != failure) ; failure = failure->next, i++) {
            xprintf("\n    %d. %s:%u  - %s", i,
                    (NULL != failure->file) ? failure->file : "",
                    failure->line,
                    (NULL != failure->message) ? failure->message : "");
        }
    }
}

void run_single_test(CTestCase* test) {
    volatile unsigned int start_failures;
    /* keep track of the last failure BEFORE running the test */
    CTest_FailureRecord* pLastFailure = last_failure;
    jmp_buf buf;

    assert(NULL != cur_suite);
    assert(0 != cur_suite->active);
    assert(NULL != test);

    start_failures = summary.failure_records;

    cur_test = test;

    // Before test
    assert(NULL != cur_suite);
    assert(NULL != cur_test);
    assert(NULL != cur_test->name);

    if((NULL == running_suite) || (running_suite != cur_suite)) {
        assert(NULL != cur_suite->name);
        xprintf("\n--== %s ==--", cur_suite->name);
        xprintf("\n  * %s.. ", cur_test->name);
        running_suite = cur_suite;
    } else {
        xprintf("\n  * %s.. ", cur_test->name);
    }

    /* run test if it is active */
    if(0 != test->active) {

        /* set jmp_buf and run test */
        test->jumpBuf = &buf;

        if(0 == setjmp(buf)) {
            if(NULL != test->test) {
                (*test->test)();
            }
        }

        summary.tests_run++;
    } else {
        summary.tests_inactive++;

        add_failure(&failure_list, CUF_TestInactive, 0, "Test inactive", "CTest System", cur_suite, cur_test);
    }

    // if additional failures have occurred..
    if(summary.failure_records > start_failures) {
        summary.tests_failed++;

        if(NULL != pLastFailure) {
            pLastFailure = pLastFailure->next;  /* was a previous failure, so go to next one */
        } else {
            pLastFailure = failure_list;       /* no previous failure - go to 1st one */
        }
    } else {
        pLastFailure = NULL;                   /* no additional failure - set to NULL */
    }

    basic_test_complete_message_handler(cur_test, cur_suite, pLastFailure);

    test->jumpBuf = NULL;
    cur_test = NULL;
}

// Runs all tests in a specified suite
void run_single_suite(CTestSuite* suite) {
    CTestCase* test = NULL;
    unsigned int nStartFailures;

    /* keep track of the last failure BEFORE running the test */
    CTest_FailureRecord* pLastFailure = last_failure;

    assert(NULL != suite);

    nStartFailures = summary.failure_records;

    cur_test = NULL;
    cur_suite = suite;

    /* run suite if it's active */
    if(suite->active) {

        /* run the suite initialization function, if any */
        if((NULL != suite->initialize) && (0 != (*suite->initialize)())) {
            /* init function had an error - call handler, if any */
            assert(NULL != suite);
            assert(NULL != suite->name);
            xprintf("\nWARNING - Suite initialization failed for '%s'.", suite->name);

            summary.suites_failed++;
            add_failure(&failure_list, CUF_SuiteInitFailed, 0, "Suite Initialization failed - Suite Skipped", "CTest System", suite, NULL);

            error("Suite initialization failed");
            return;
        } else { /* reach here if no suite initialization, or if it succeeded */
            test = suite->test;

            while(NULL != test) {
                if(0 != test->active) {
                    run_single_test(test);
                } else {
                    summary.tests_inactive++;
                    add_failure(&failure_list, CUF_TestInactive, 0, "Test inactive", "CTest System", suite, test);
                }

                test = test->next;
            }

            summary.suites_run++;

            /* call the suite cleanup function, if any */
            if((NULL != suite->cleanup) && (0 != (*suite->cleanup)())) {
                assert(NULL != suite);
                assert(NULL != suite->name);
                xprintf("\nWARNING - Suite cleanup failed for '%s'.", suite->name);

                summary.suites_failed++;
                add_failure(&failure_list, CUF_SuiteCleanupFailed, 0, "Suite cleanup failed.", "CTest System", suite, NULL);
            }
        }
    } else { /* otherwise record inactive suite and failure if appropriate */
        summary.suites_inactive++;

        add_failure(&failure_list, CUF_SuiteInactive, 0, "Suite inactive", "CTest System", suite, NULL);
    }

    //if additional failures have occurred..
    if(summary.failure_records > nStartFailures) {
        if(NULL != pLastFailure) {
            pLastFailure = pLastFailure->next;  /* was a previous failure, so go to next one */
        } else {
            pLastFailure = failure_list;       /* no previous failure - go to 1st one */
        }
    } else {
        pLastFailure = NULL;                   /* no additional failure - set to NULL */
    }

    cur_suite = NULL;
}

void cleanup_failure_list() {
    CTest_FailureRecord* cur = NULL;
    CTest_FailureRecord* next = NULL;

    cur = failure_list;

    while(NULL != cur) {

        if(NULL != cur->message) {
            free(cur->message);
        }

        if(NULL != cur->file) {
            free(cur->file);
        }

        next = cur->next;
        free(cur);
        cur = next;
    }

    failure_list = NULL;
}

void clear_previous_results() {
    summary.suites_run = 0;
    summary.suites_failed = 0;
    summary.suites_inactive = 0;
    summary.tests_run = 0;
    summary.tests_failed = 0;
    summary.tests_inactive = 0;
    summary.asserts = 0;
    summary.asserts_failed = 0;
    summary.failure_records = 0;
    summary.elapsed_time = 0.0;

    if(NULL != failure_list) {
        cleanup_failure_list(failure_list);
    }

    last_failure = NULL;
}

size_t number_width(int number) {
    char buf[33];

    snprintf(buf, 33, "%d", number);
    buf[32] = '\0';
    return strlen(buf);
}

char* CU_get_run_results_string() {
    size_t width[9];
    size_t len;
    char* result;

    width[0] = strlen("Run Summary:");
    width[1] = max_int(5, 6, strlen("Type"), strlen("suites"), strlen("tests"), strlen("asserts")) + 1;
    width[2] = max_int(5, 6, strlen("Total"), number_width(registry.number_of_suites), number_width(registry.number_of_tests), number_width(summary.asserts)) + 1;
    width[3] = max_int(5, 6, strlen("Ran"), number_width(summary.suites_run), number_width(summary.tests_run), number_width(summary.asserts)) + 1;
    width[4] = max_int(5, 6, strlen("Passed"), strlen("n/a"), number_width(summary.tests_run - summary.tests_failed), number_width(summary.asserts - summary.asserts_failed)) + 1;
    width[5] = max_int(5, 6, strlen("Failed"), number_width(summary.suites_failed), number_width(summary.tests_failed), number_width(summary.asserts_failed)) + 1;
    width[6] = max_int(5, 6, strlen("Inactive"), number_width(summary.suites_inactive), number_width(summary.tests_inactive), strlen("n/a")) + 1;

    width[7] = strlen("Elapsed time = ");
    width[8] = strlen(" seconds");

    len = 13 + 4 * (width[0] + width[1] + width[2] + width[3] + width[4] + width[5] + width[6]) + width[7] + width[8] + 1 + 2;
    result = (char*)malloc(len);

    if(NULL != result) {
        snprintf(result, len, "\n%*s%*s%*s%*s%*s%*s%*s\n"   /* if you change this, be sure  */
                 "%*s%*s%*u%*u%*s%*u%*u\n"   /* to change the calculation of */
                 "%*s%*s%*u%*u%*u%*u%*u\n"   /* len above!                   */
                 "%*s%*s%*u%*u%*u%*u%*s\n\n"
                 "%*s%8.3f%*s",
                 width[0], "Run Summary:",
                 width[1], "Type",
                 width[2], "Total",
                 width[3], "Ran",
                 width[4], "Passed",
                 width[5], "Failed",
                 width[6], "Inactive",
                 width[0], " ",
                 width[1], "suites",
                 width[2], registry.number_of_suites,
                 width[3], summary.suites_run,
                 width[4], "n/a",
                 width[5], summary.suites_failed,
                 width[6], summary.suites_inactive,
                 width[0], " ",
                 width[1], "tests",
                 width[2], registry.number_of_tests,
                 width[3], summary.tests_run,
                 width[4], summary.tests_run - summary.tests_failed,
                 width[5], summary.tests_failed,
                 width[6], summary.tests_inactive,
                 width[0], " ",
                 width[1], "asserts",
                 width[2], summary.asserts,
                 width[3], summary.asserts,
                 width[4], summary.asserts - summary.asserts_failed,
                 width[5], summary.asserts_failed,
                 width[6], "n/a",
                 width[7], "Elapsed time = ", get_elapsed_time(),  /* makes sure time is updated */
                 width[8], " seconds"
                );
        result[len - 1] = '\0';
    }

    return result;
}

void all_tests_complete_report() {
    printf("\n\n");

    char* summary_string;
    summary_string = CU_get_run_results_string();

    if(NULL != summary_string) {
        xprintf("%s", summary_string);
        free(summary_string);
    } else {
        xprintf("An error occurred printing the run results.");
    }

    printf("\n");
}

void CTest_run_all_tests() {
    CTestSuite* suite = NULL;

    /* Clear results from the previous run */
    clear_previous_results(&failure_list);

    /* test run is starting - set flag */
    test_is_running = 1;
    start_time = clock();

    for(suite = registry.suite; suite; suite = suite->next) {
        run_single_suite(suite);
    }

    /* test run is complete - clear flag */
    test_is_running = 0;
    summary.elapsed_time = ((double)clock() - (double)start_time) / (double)CLOCKS_PER_SEC;

    all_tests_complete_report(failure_list);
}

void CTest_run_suite(CTestSuite* suite) {
    /* Clear results from the previous run */
    clear_previous_results(&failure_list);

    assert(NULL != suite);

    /* test run is starting - set flag */
    test_is_running = 1;
    start_time = clock();

    run_single_suite(suite);

    /* test run is complete - clear flag */
    test_is_running = 0;
    summary.elapsed_time = ((double)clock() - (double)start_time) / (double)CLOCKS_PER_SEC;

    /* run handler for overall completion, if any */
    all_tests_complete_report(failure_list);
}

int strcmp_ignore_case(const char* src, const char* dest) {
    assert(NULL != src);
    assert(NULL != dest);

    while(('\0' != *src) && ('\0' != *dest) && (toupper(*src) == toupper(*dest))) {
        src++;
        dest++;
    }

    return (int)(*src - *dest);
}

CTestCase* get_test_by_name(const char* szTestName, CTestSuite* suite) {
    CTestCase* test = NULL;
    CTestCase* pCur = NULL;

    assert(NULL != suite);
    assert(NULL != szTestName);

    pCur = suite->test;

    while(NULL != pCur) {
        if((NULL != pCur->name) && (!strcmp_ignore_case(pCur->name, szTestName))) {
            test = pCur;
            break;
        }

        pCur = pCur->next;
    }

    return test;
}

void run_test(CTestSuite* suite, CTestCase* test) {
    /* Clear results from the previous run */
    clear_previous_results(&failure_list);

    assert(NULL != suite);
    assert(NULL != test);

    if(0 == suite->active) {
        summary.suites_inactive++;

        add_failure(&failure_list, CUF_SuiteInactive, 0, "Suite inactive", "CTest System", suite, NULL);
    } else if((NULL == test->name) || (NULL == get_test_by_name(test->name, suite))) {
        error("Test not registered in specified suite.");
        return;
    } else {
        /* test run is starting - set flag */
        test_is_running = 1;
        start_time = clock();

        cur_test = NULL;
        cur_suite = suite;

        /* run the suite initialization function, if any */
        if((NULL != suite->initialize) && (0 != (*suite->initialize)())) {
            /* init function had an error - call handler, if any */
            assert(NULL != suite);
            assert(NULL != suite->name);
            xprintf("\nWARNING - Suite initialization failed for '%s'.", suite->name);

            summary.suites_failed++;
            add_failure(&failure_list, CUF_SuiteInitFailed, 0, "Suite Initialization failed - Suite Skipped", "CTest System", suite, NULL);

            error("Suite initialization function failed.");
            return;
        }
        /* reach here if no suite initialization, or if it succeeded */
        else {
            run_single_test(test);

            /* run the suite cleanup function, if any */
            if((NULL != suite->cleanup) && (0 != (*suite->cleanup)())) {
                /* cleanup function had an error - call handler, if any */
                assert(NULL != suite);
                assert(NULL != suite->name);
                xprintf("\nWARNING - Suite cleanup failed for '%s'.", suite->name);

                summary.suites_failed++;
                add_failure(&failure_list, CUF_SuiteCleanupFailed, 0, "Suite cleanup failed.", "CTest System", suite, NULL);
            }
        }

        /* test run is complete - clear flag */
        test_is_running = 0;
        summary.elapsed_time = ((double)clock() - (double)start_time) / (double)CLOCKS_PER_SEC;

        /* run handler for overall completion, if any */
        all_tests_complete_report(failure_list);

        cur_suite = NULL;
    }
}

void CTest_run_tests() {
    running_suite = NULL;
    CTest_run_all_tests();
}

void basic_run_suite(CTestSuite* suite) {
    assert(NULL != suite);
    running_suite = NULL;
    return CTest_run_suite(suite);
}

void basic_run_test(CTestSuite* suite, CTestCase* test) {
    assert(NULL != suite);
    assert(NULL != test);
    running_suite = NULL;
    return run_test(suite, test);
}

static void cleanup_test(CTestCase* test) {
    assert(NULL != test);

    if(NULL != test->name) {
        free(test->name);
    }

    test->name = NULL;
}

void cleanup_suite(CTestSuite* suite) {
    CTestCase* cur = NULL;
    CTestCase* next = NULL;

    assert(NULL != suite);

    cur = suite->test;

    while(NULL != cur) {
        next = cur->next;

        cleanup_test(cur);

        free(cur);
        cur = next;
    }

    if(NULL != suite->name) {
        free(suite->name);
    }

    suite->name = NULL;
    suite->test = NULL;
    suite->number_of_tests = 0;
}

void cleanup_test_registry() {
    CTestSuite* cur = NULL;
    CTestSuite* next = NULL;

    for(cur = registry.suite; NULL != cur; cur = next) {
        next = cur->next;
        cleanup_suite(cur);
        free(cur);
    }

    registry.suite = NULL;
    registry.number_of_suites = 0;
    registry.number_of_tests = 0;
}

void CTest_cleanup_registry() {
    assert(!test_is_running);

    cleanup_test_registry();

    clear_previous_results();
}

void CTest_initialize_registry() {
    assert(!test_is_running);
    CTest_cleanup_registry();
}

CTestSuite* create_suite(const char* name, CTest_suite_function init, CTest_suite_function clean) {
    CTestSuite* suite = (CTestSuite*)malloc(sizeof(CTestSuite));
    assert(NULL != suite);
    assert(NULL != name);

    if(NULL != suite) {
        suite->name = (char*)malloc(strlen(name) + 1);

        if(NULL != suite->name) {
            strcpy(suite->name, name);
            suite->active = 1;
            suite->initialize = init;
            suite->cleanup = clean;
            suite->test = NULL;
            suite->next = NULL;
            suite->prev = NULL;
            suite->number_of_tests = 0;
        } else {
            free(suite);
            suite = NULL;
        }
    }

    return suite;
}

void insert_suite(CTestSuite* suite) {
    CTestSuite* cur = NULL;

    assert(NULL != suite);

    cur = registry.suite;

    assert(cur != suite);

    suite->next = NULL;
    registry.number_of_suites++;

    // if this is the 1st suite to be added..
    if(NULL == cur) {
        registry.suite = suite;
        suite->prev = NULL;
    }
    // otherwise, add it to the end of the linked list..
    else {
        while(NULL != cur->next) {
            cur = cur->next;
            assert(cur != suite);
        }

        cur->next = suite;
        suite->prev = cur;
    }
}

int suite_exists(const char* suite_name) {
    CTestSuite* suite = NULL;

    assert(NULL != suite_name);

    suite = registry.suite;

    while(NULL != suite) {
        if((NULL != suite->name) && (!strcmp_ignore_case(suite_name, suite->name))) {
            return 1;
        }

        suite = suite->next;
    }

    return 0;
}

CTestSuite* CTest_add_suite(const char* name, CTest_suite_function init, CTest_suite_function clean, const char* file, const int line) {
    CTestSuite* ret = NULL;
    assert(!test_is_running);

    if(NULL == name) {
        error("Suite name cannot be NULL.");
    } else {
        ret = create_suite(name, init, clean);

        if(NULL == ret) {
            cleanup_test_registry();
            xprintf("Memory allocation failed\n");
            exit(1);
        }

        assert(NULL != ret);

        if(suite_exists(name)) {
            xprintf("WARNING: Suite with same name \"%s\" %s:%d\n", name, file, line);
        }

        insert_suite(ret);
    }

    return ret;
}

CTestCase* create_test(const char* name, CTestFunc testFunction) {
    CTestCase* test = (CTestCase*)malloc(sizeof(CTestCase));
    assert(NULL != testFunction);
    assert(NULL != name);

    if(NULL == test) {
        xprintf("Memory allocation failed\n");
        cleanup_test_registry();
        exit(1);
    }

    assert(NULL != test);

    if(NULL != test) {
        test->name = (char*)malloc(strlen(name) + 1);

        if(NULL != test->name) {
            strcpy(test->name, name);
            test->active = 1;
            test->test = testFunction;
            test->jumpBuf = NULL;
            test->next = NULL;
            test->prev = NULL;
        } else {
            free(test);
            test = NULL;
        }
    }

    return test;
}

void insert_test(CTestSuite* suite, CTestCase* test) {
    CTestCase* pCurTest = NULL;

    assert(NULL != suite);
    assert(NULL != test);
    assert(NULL == test->next);
    assert(NULL == test->prev);

    pCurTest = suite->test;

    assert(pCurTest != test);

    suite->number_of_tests++;

    // if this is the 1st suite to be added..
    if(NULL == pCurTest) {
        suite->test = test;
        test->prev = NULL;
    } else {
        while(NULL != pCurTest->next) {
            pCurTest = pCurTest->next;
            assert(pCurTest != test);
        }

        pCurTest->next = test;
        test->prev = pCurTest;
    }
}

/**
 *  Check whether a test having a specified name is already registered in a given suite.
 *  @return 1 if test exists in the suite, 0 otherwise.
 */
int test_exists(CTestSuite* suite, const char* test_name) {
    CTestCase* test = NULL;

    assert(NULL != suite);
    assert(NULL != test_name);

    test = suite->test;

    while(NULL != test) {
        if((NULL != test->name) && (!strcmp_ignore_case(test_name, test->name))) {
            return 1;
        }

        test = test->next;
    }

    return 0;
}

CTestCase* CTest_add_test(CTestSuite* suite, const char* name, CTestFunc testFunction, const char* file, const int line) {
    CTestCase* test = NULL;

    assert(!test_is_running);

    if(NULL == suite) {
        xprintf("NULL suite not allowed. %s:%d\n", file, line);
        exit(1);
    } else if(NULL == name) {
        xprintf("Test name cannot be NULL. %s:%d\n", file, line);
        exit(1);
    } else if(NULL == testFunction) {
        xprintf("NULL == testFunction %s:%d\n", file, line);
        exit(1);
    } else {
        test = create_test(name, testFunction);

        if(NULL == test) {
            cleanup_test_registry();
            exit(1);
        }

        assert(NULL != test);

        registry.number_of_tests++;

        if(test_exists(suite, name)) {
            xprintf("WARNING: Duplicate test case name not allowed \"%s\" %s:%d\n", name, file, line);
        }

        insert_test(suite, test);
    }

    assert(NULL != test);
    return test;
}

// Save string to file
void str_to_file(const char* str, const char* filename) {
    FILE* f = fopen(filename, "w");
    fputs(str, f);
    fclose(f);
}

// Load file to string
char* FileToStr(const char* filename) {
    FILE* f = fopen(filename, "r");

    if(!f) {
        xprintf("ERROR: File \"%s\" not exists!\n", filename);
        return NULL; // File not exists!
    }

    fseek(f, 0, SEEK_END);
    int fileSize = ftell(f);
    char* buf = malloc(fileSize + 1);
    rewind(f);
    int len = fread(buf, sizeof(char), fileSize, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

