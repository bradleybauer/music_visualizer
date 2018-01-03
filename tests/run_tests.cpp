#include <iostream>
using std::cout;
using std::endl;
using std::string;

#include "Test.h"
#include "ShaderConfig.h"
#include "audio_process.h"

template<typename T>
void test(string name) {
    cout << ("                                                                     Test " + name) << endl;
    bool ok = T().test();
}

int main() {
    test<AudioProcessTest>("audio_process");
    test<ShaderConfigTest>("ShaderConfig");
    return 0;
}