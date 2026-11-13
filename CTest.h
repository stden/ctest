#ifndef CTEST_H
#define CTEST_H

#include <math.h>
#include <setjmp.h> // jmp_buf
#include <errno.h>

// a - actual
// e - expected
// v - boolean condition
// m - message
#define CU_PASS(m) { CTest(1, ("PASS(" #m ")"), __FILE__, __LINE__); }
#define CU_ASSERT(v) { CTest((v), #v, __FILE__, __LINE__); }
#define CU_ASSERT_FATAL(v) { CTestFatal((v), #v, __FILE__,__LINE__); }
#define CU_FAIL(m) { CTest(0, ("CU_FAIL(" #m ")"), __FILE__, __LINE__); }
#define CU_FAIL_FATAL(m) { CTestFatal(0, ("CU_FAIL_FATAL(" #m ")"), __FILE__, __LINE__); }
#define CU_ASSERT_EQUAL(a, e) { CTestInt(a, e, ("CU_ASSERT_EQUAL(" #a "," #e ")"), __FILE__,__LINE__); }
#define CU_ASSERT_EQUAL_FATAL(a, e) { CTestFatal(((a) == (e)), ("CU_ASSERT_EQUAL_FATAL(" #a "," #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_EQUAL_FATALX(a, e, m) { CTestFatal(((a) == (e)), m, __FILE__, __LINE__); }
#define CU_ASSERT_NOT_EQUAL(a, e) { CTest(((a) != (e)), ("CU_ASSERT_NOT_EQUAL(" #a "," #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_NOT_EQUAL_FATAL(a, e) { CTestFatal(((a) != (e)), ("CU_ASSERT_NOT_EQUAL_FATAL(" #a "," #e ")"), __FILE__, __LINE__); }

#define CU_ASSERT_PTR_EQUAL(a, e) { CTest(((void*)(a) == (void*)(e)), ("CU_ASSERT_PTR_EQUAL(" #a "," #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_EQUAL_FATAL(a, e) { CTestFatal(((void*)(a) == (void*)(e)), ("CU_ASSERT_PTR_EQUAL_FATAL(" #a "," #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_NOT_EQUAL(a, e) { CTest(((void*)(a) != (void*)(e)), ("CU_ASSERT_PTR_NOT_EQUAL(" #a "," #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_NOT_EQUAL_FATAL(a, e) { CTestFatal(((void*)(a) != (void*)(e)), ("CU_ASSERT_PTR_NOT_EQUAL_FATAL(" #a "," #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_NULL(v) { CTest((NULL == (void*)(v)), ("CU_ASSERT_PTR_NULL(" #v")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_NULL_FATAL(v) { CTestFatal((NULL == (void*)(v)), ("CU_ASSERT_PTR_NULL_FATAL(" #v")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_NOT_NULL(v) { CTest((NULL != (void*)(v)), ("CU_ASSERT_PTR_NOT_NULL(" #v")"), __FILE__, __LINE__); }
#define CU_ASSERT_PTR_NOT_NULL_FATAL(v) { CTestFatal((NULL != (void*)(v)), ("CU_ASSERT_PTR_NOT_NULL_FATAL(" #v")"), __FILE__,__LINE__); }

#define CU_ASSERT_STRING_EQUAL(a, e) { CTestStrings(a,e,("CU_ASSERT_STRING_EQUAL(" #a ","  #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_STRING_EQUAL_MESSAGE(a, e, m) { CTestStrings(a,e,("CU_ASSERT_STRING_EQUAL_MESSAGE(" #a ","  #e ") " m), __FILE__, __LINE__); }
#define CU_ASSERT_STRING_EQUAL_FATAL(a, e) { CTestStringsFatal(!(strcmp((const char*)(a), (const char*)(e))), ("CU_ASSERT_STRING_EQUAL_FATAL(" #a ","  #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_STRING_NOT_EQUAL(a, e) { CTest((strcmp((const char*)(a), (const char*)(e))), ("CU_ASSERT_STRING_NOT_EQUAL(" #a ","  #e ")"), __FILE__, __LINE__); }
#define CU_ASSERT_STRING_NOT_EQUAL_FATAL(a, e) { CTestFatal((strcmp((const char*)(a), (const char*)(e))), ("CU_ASSERT_STRING_NOT_EQUAL_FATAL(" #a ","  #e ")"), __FILE__, __LINE__); }

#define CU_ASSERT_NSTRING_EQUAL(a, e, count) { CTest(!(strncmp((const char*)(a), (const char*)(e), (size_t)(count))), ("CU_ASSERT_NSTRING_EQUAL(" #a ","  #e "," #count ")"), __FILE__, __LINE__); }
#define CU_ASSERT_NSTRING_EQUAL_FATAL(a, e, count) { CTestFatal(!(strncmp((const char*)(a), (const char*)(e), (size_t)(count))), ("CU_ASSERT_NSTRING_EQUAL_FATAL(" #a ","  #e "," #count ")"), __FILE__, __LINE__); }
#define CU_ASSERT_NSTRING_NOT_EQUAL(a, e, count) { CTest((strncmp((const char*)(a), (const char*)(e), (size_t)(count))), ("CU_ASSERT_NSTRING_NOT_EQUAL(" #a ","  #e "," #count ")"), __FILE__, __LINE__); }
#define CU_ASSERT_NSTRING_NOT_EQUAL_FATAL(a, e, count) { CTestFatal((strncmp((const char*)(a), (const char*)(e), (size_t)(count))), ("CU_ASSERT_NSTRING_NOT_EQUAL_FATAL(" #a ","  #e "," #count ")"), __FILE__, __LINE__); }

#define CU_ASSERT_DOUBLE_EQUAL(a, e, eps) { CTest(((fabs((double)(a) - (e)) <= fabs((double)(eps)))), ("CU_ASSERT_DOUBLE_EQUAL(" #a ","  #e "," #eps ")"), __FILE__, __LINE__); }
#define CU_ASSERT_DOUBLE_EQUAL_FATAL(a, e, eps) { CTestFatal(((fabs((double)(a) - (e)) <= fabs((double)(eps)))), ("CU_ASSERT_DOUBLE_EQUAL_FATAL(" #a ","  #e "," #eps ")"), __FILE__, __LINE__); }
#define CU_ASSERT_DOUBLE_NOT_EQUAL(a, e, eps) { CTest(((fabs((double)(a) - (e)) > fabs((double)(eps)))), ("CU_ASSERT_DOUBLE_NOT_EQUAL(" #a ","  #e "," #eps ")"), __FILE__, __LINE__); }
#define CU_ASSERT_DOUBLE_NOT_EQUAL_FATAL(a, e, eps) { CTestFatal(((fabs((double)(a) - (e)) > fabs((double)(eps)))), ("CU_ASSERT_DOUBLE_NOT_EQUAL_FATAL(" #a ","  #e "," #eps ")"), __FILE__, __LINE__); }

#define CU_ASSERT_FILES_EQUAL(a, e) { CTestFiles(a, e, ("CU_ASSERT_FILES_EQUAL(" #a ","  #e ")"),__FILE__,__LINE__); }

#define TEST(suite, msg, test) ( CTest_add_test(suite, msg" - "#test, (CTestFunc)test, __FILE__, __LINE__) )
#define TEST_SUITE(name, init, clean) ( CTest_add_suite(name, init, clean, __FILE__, __LINE__) )

void CTest_cleanup_registry();

typedef int (*CTest_suite_function)(void);
typedef void (*CTestFunc)(void);        // Signature for a testing function in a test case

// Basic assert
int CTest(int condition, const char* message, const char* file, const int line);
int CTestFatal(int condition, const char* message, const char* file, const int line);

typedef struct CTestCase {
    char*           name;
    int             active;
    CTestFunc       test;
    jmp_buf*        jumpBuf; // Jump buffer for setjmp/longjmp test abort mechanism
    struct CTestCase* prev, *next;
} CTestCase;

typedef struct CTestSuite {
    char*             name;
    int               active;    // Flag for whether suite is executed during a run
    CTestCase*        test;      // Pointer to the 1st test in the suite
    CTest_suite_function initialize;  // Pointer to the suite initialization function
    CTest_suite_function cleanup;     // Pointer to the suite cleanup function

    unsigned int      number_of_tests;  // Number of tests in the suite.
    struct CTestSuite* prev, *next;
} CTestSuite;

CTestSuite* CTest_add_suite(const char* name, CTest_suite_function init, CTest_suite_function clean, const char* file, const int line);

CTestCase* CTest_add_test(CTestSuite* suite, const char* name, CTestFunc testFunction, const char* file, const int line);

void CTest_initialize_registry();
void CTest_run_all_tests();
void CTest_run_tests();

void CTestStrings(const char* actual, // Actual string
                  const char* expected, // Expected string
                  const char* message, // Message
                  const char* file, // Source file
                  const int line // Source line
                 );

void CTestInt(uint64_t a, uint64_t e, const char* message, const char* file, const int line);

// Compare files
void CTestFiles(char* filename_actual, char* filename_expected, const char* message, const char* file, const int line);

// Save string to file
void str_to_file(const char* str, const char* filename);

// Load file to string
char* FileToStr(const char* filename);

#endif // CTEST_H
