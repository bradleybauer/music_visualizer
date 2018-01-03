class Test {
public:
	virtual bool test() = 0;
};

class AudioProcessTest : Test {
public:
	bool test();
private:
	bool adjust_reader();
	bool advance_index();
	bool get_harmonic_less_than();
};

class ShaderConfigTest : Test {
public:
	bool test();
private:
	bool parse_valid1();
	bool parse_valid2();
	bool parse_valid3();
	bool parse_valid4();

	bool parse_invalid1();
	bool parse_invalid2();
	bool parse_invalid3();
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

#include <string>
static const std::string PASS_MSG("                                                                          Pass");
static const std::string FAIL_MSG("                                                                          Fail");

// enable default constructors so mock classes are easier to make
#define TEST
