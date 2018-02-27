#include "Test.h"

#include "filesystem.h"

#include <iostream>
using std::cout;
using std::endl;

bool FilesystemTest::test()
{
	bool is_ok = true;

	cout << "filesys construct 1:" << endl;
	is_ok &= construct1();

	cout << "filesys construct 2:" << endl;
	is_ok &= construct2();

	cout << "filesys construct 3:" << endl;
	is_ok &= construct3();

	cout << "filesys construct 4:" << endl;
	is_ok &= construct4();

	cout << "filesys concat test 1: " << endl;
	is_ok &= concat1();

	cout << "filesys concat test 2: " << endl;
	is_ok &= concat2();

	cout << "filesys multiple separators test 1: " << endl;
	is_ok &= concat_multiple_separators1();

	cout << "filesys multiple separators test 2: " << endl;
	is_ok &= concat_multiple_separators2();

	cout << "filesys multiple separators test 3: " << endl;
	is_ok &= concat_multiple_separators3();

	cout << "filesys multiple separators test 4: " << endl;
	is_ok &= concat_multiple_separators4();

	cout << "filesys extension 1:" << endl;
	is_ok &= extension1();

	cout << "filesys extension 2:" << endl;
	is_ok &= extension2();

	cout << "filesys extension 3:" << endl;
	is_ok &= extension3();

	cout << "filesys extension 4:" << endl;
	is_ok &= extension4();

	cout << "filesys extension 5:" << endl;
	is_ok &= extension5();

	cout << "filesys extension 6:" << endl;
	is_ok &= extension6();

	cout << "filesys extension 7:" << endl;
	is_ok &= extension7();

	cout << "filesys extension 8:" << endl;
	is_ok &= extension8();

	cout << "filesys extension 9:" << endl;
	is_ok &= extension9();

	cout << "filesys extension 10:" << endl;
	is_ok &= extension10();

	cout << "filesys extension 11:" << endl;
	is_ok &= extension11();

	cout << "filesys extension 12:" << endl;
	is_ok &= extension12();

	cout << "filesys extension 13:" << endl;
	is_ok &= extension13();

	return is_ok;
}

bool FilesystemTest::construct1()
{
	filesys::path mock("name///");
	if (mock.string() == "name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::construct2()
{
	filesys::path mock("name/");
	if (mock.string() == "name") {
		cout << PASS_MSG << endl;
		return false;
	}
	else {
		cout << FAIL_MSG << endl;
		return true;
	}
}

bool FilesystemTest::construct3()
{
	filesys::path mock("/name");
	if (mock.string() == "/name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::construct4()
{
	filesys::path mock1("name///");
	filesys::path mock2(mock1);
	if (mock2.string() == "name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::concat1()
{
	filesys::path mock("name");
	filesys::path new_path = mock / "a";
	if (new_path.string() == "name/a") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::concat2()
{
	filesys::path mock1("name");
	filesys::path mock2("name");
	filesys::path new_path = mock1 / mock2;
	if (new_path.string() == "name/name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::concat_multiple_separators1()
{
	filesys::path mock1("name");
	filesys::path mock2("//name");
	filesys::path new_path = mock1 / mock2;
	if (new_path.string() == "name/name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::concat_multiple_separators2()
{
	filesys::path mock1("name//");
	filesys::path mock2("//name");
	filesys::path new_path = mock1 / mock2;
	if (new_path.string() == "name/name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::concat_multiple_separators3()
{
	filesys::path mock1("name//");
	filesys::path mock2("//name//");
	filesys::path new_path = mock1 / mock2;
	if (new_path.string() == "name/name") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::concat_multiple_separators4()
{
	filesys::path mock("name///");
	filesys::path new_path = mock / "//a";
	if (new_path.string() == "name/a") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension1()
{
	filesys::path mock("filename.cpp");
	if (mock.extension().string() == ".cpp") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension2()
{
	filesys::path mock("filename.");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension3()
{
	filesys::path mock("filename");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension4()
{
	filesys::path mock(".");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension5()
{
	filesys::path mock("filename.g");
	if (mock.extension().string() == ".g") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension6()
{
	filesys::path mock("filename.frag");
	if (mock.extension().string() == ".frag") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension7()
{
	filesys::path mock("filename/.frag");
	if (mock.extension().string() == ".frag") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension8()
{
	filesys::path mock("foldername/..");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension9()
{
	filesys::path mock("/");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension10()
{
	filesys::path mock("/..");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension11()
{
	filesys::path mock("../");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension12()
{
	filesys::path mock("./");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

bool FilesystemTest::extension13()
{
	filesys::path mock("foldername/.");
	if (mock.extension().string() == "") {
		cout << PASS_MSG << endl;
		return true;
	}
	else {
		cout << FAIL_MSG << endl;
		return false;
	}
}

