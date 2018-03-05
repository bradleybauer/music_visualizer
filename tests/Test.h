class FilesystemTest {
public:
	bool test();
private:
	bool construct1();
	bool construct2();
	bool construct3();
	bool construct4();
	bool concat1();
	bool concat2();
	bool concat_multiple_separators1();
	bool concat_multiple_separators2();
	bool concat_multiple_separators3();
	bool concat_multiple_separators4();

	bool extension1();
	bool extension2();
	bool extension3();
	bool extension4();
	bool extension5();
	bool extension6();
	bool extension7();
	bool extension8();
	bool extension9();
	bool extension10();
	bool extension11();
	bool extension12();
	bool extension13();
};

class AudioUtilityTest {
public:
	bool test();
private:
	bool adjust_reader();
	bool advance_index();
	bool get_harmonic_less_than();
};

class ShaderConfigTest {
public:
	bool test();
private:
	bool parse_valid1();
	bool parse_valid2();
	bool parse_valid3();
	bool parse_valid4();

	bool parse_invalid2();
	bool parse_invalid4();
	bool parse_invalid5();
	bool parse_invalid6();
	bool parse_invalid7();
	bool parse_invalid8();
	bool parse_invalid9();
	bool parse_invalid10();
	bool parse_invalid11();
	bool parse_invalid12();
	bool parse_invalid13();
	bool parse_invalid14();
	bool parse_invalid15();
};

class AudioProcessTest {
public:
	bool test();
private:
};

#include <string>
static const std::string PASS_MSG("                                                                          Pass");
static const std::string FAIL_MSG("                                                                          Fail");

// enable certain default constructors so mock classes are easier to make
#define TEST
