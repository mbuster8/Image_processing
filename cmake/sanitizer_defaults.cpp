// Homebrew's GoogleTest, OpenCV and Qt are built without AddressSanitizer. libc++ annotates its containers
// only in instrumented code, so a std::string or std::vector that crosses into one of those libraries is
// reported as a "container-overflow" that is not real. Every other check stays on.
extern "C" const char* __asan_default_options() { return "detect_container_overflow=0"; }
