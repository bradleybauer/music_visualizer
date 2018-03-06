#include <iostream>
using std::cout; using std::endl;
#include <string>
using std::string;

#include "Test.h"
#include "ShaderConfig.h"
#include "AudioProcess.h"

template<typename T>
void test(string name) {
    cout << ("                                                                     Test " + name) << endl;
    bool ok = T().test();
}

int main() {
    test<AudioProcessTest>("AudioProcess");
    test<ShaderConfigTest>("ShaderConfig");
	test<FilesystemTest>("Filesystem");
    return 0;
}