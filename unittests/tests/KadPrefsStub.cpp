// Stub for CPreferences::s_KadProtocol10 and s_KadStrictAichPublishers so unit tests
// that compile Entry.cpp directly can link without pulling in
// all of Preferences.cpp.
#include <Preferences.h>

bool CPreferences::s_KadProtocol10 = false;
bool CPreferences::s_KadStrictAichPublishers = false;
