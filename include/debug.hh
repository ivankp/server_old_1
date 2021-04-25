#ifndef IVANP_DEBUG_HH
#define IVANP_DEBUG_HH

#ifndef NDEBUG

#include <iostream>

#define STR1(x) #x
#define STR(x) STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

#ifdef IVANP_STRING_HH
#define INFO(color,...) std::cout << \
  ivanp::cat("\033[" color "m",__VA_ARGS__,"\033[0m") << std::endl;
#else
#define INFO(color,...) ;
#endif

#else

#define TEST(var) ;
#define INFO(color,...) ;

#endif
#endif
