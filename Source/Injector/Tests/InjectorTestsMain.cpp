#include <cassert>

// Forward declarations of the test suites.
void RunHookManagerTests();
void RunInjectorInitTests();
void RunInjectorIntegrationTests();
void RunLoggingTests();

int main() {
  RunHookManagerTests();
  RunInjectorInitTests();
  RunLoggingTests();

#ifdef DS3OS_ENABLE_GAME_INTEGRATION_TESTS
  RunInjectorIntegrationTests();
#endif

  return 0;
}
