#ifndef qassert_h
#define qassert_h

/** NASSERT macro disables all contract validations
* (assertions, preconditions, postconditions, and invariants).
*/
#ifdef NASSERT           /* NASSERT defined--DbC disabled */

#define DEFINE_THIS_FILE
#define ASSERT(ignore_)  ((void)0)
#define ALLEGE(test_)    ((void)(test_))

#else                    /* NASSERT not defined--DbC enabled */

#ifdef __cplusplus
  extern "C"
  {
#endif
   /* callback invoked in case of assertion failure */
   void onAssert__(char const *file, unsigned line);
#ifdef __cplusplus
}
#endif


#define DEFINE_THIS_FILE static char const THIS_FILE__[] = __FILE__;
DEFINE_THIS_FILE

#define ASSERT(test_) ((test_) ? (void)0 : onAssert__(THIS_FILE__, __LINE__))

#define ALLEGE(test_)    ASSERT(test_)

#endif                   /* NASSERT */

#define REQUIRE(test_)   ASSERT(test_)
#define ENSURE(test_)    ASSERT(test_)
#define INVARIANT(test_) ASSERT(test_)

#endif                   /* qassert_h */
