#include "filesystem.h"

#include <iostream>
using std::cout;
using std::endl;

#include "catch2/catch.hpp"

TEST_CASE("construct path") {
	CHECK(filesys::path("name///").string() == "name");
	CHECK(filesys::path("name/").string() == "name");
	CHECK(filesys::path("/name").string() == "/name");
}

TEST_CASE("copy construct path") {
	filesys::path mock1("name///");
	filesys::path mock2(mock1);
	CHECK(mock2.string() == "name");
}

TEST_CASE("concatinate paths") {
	SECTION("concat 1") {
		filesys::path mock("name");
		filesys::path new_path = mock / "a";
		CHECK(new_path.string() == "name/a");
	}

	SECTION("concat 2") {
		filesys::path mock1("name");
		filesys::path mock2("name");
		filesys::path new_path = mock1 / mock2;
		CHECK(new_path.string() == "name/name");
	}

	SECTION("concat 3") {
		filesys::path mock1("name");
		filesys::path mock2("//name");
		filesys::path new_path = mock1 / mock2;
		CHECK(new_path.string() == "name/name");
	}

	SECTION("concat 4") {
		filesys::path mock1("name//");
		filesys::path mock2("//name");
		filesys::path new_path = mock1 / mock2;
		CHECK(new_path.string() == "name/name");
	}

	SECTION("concat 5") {
		filesys::path mock1("name//");
		filesys::path mock2("//name//");
		filesys::path new_path = mock1 / mock2;
		CHECK(new_path.string() == "name/name");
	}

	SECTION("concat 6") {
		filesys::path mock("name///");
		filesys::path new_path = mock / "//a";
		CHECK(new_path.string() == "name/a");
	}
}

TEST_CASE("extensions") {
	SECTION("extension 1") {
		filesys::path mock("filename.cpp");
		CHECK(mock.extension().string() == ".cpp");
	}

	SECTION("extension 2") {
		filesys::path mock("filename.");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 3") {
		filesys::path mock("filename");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 4") {
		filesys::path mock(".");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 5") {
		filesys::path mock("filename.g");
		CHECK(mock.extension().string() == ".g");
	}

	SECTION("extension 6") {
		filesys::path mock("filename.frag");
		CHECK(mock.extension().string() == ".frag");
	}

	SECTION("extension 7") {
		filesys::path mock("filename/.frag");
		CHECK(mock.extension().string() == ".frag");
	}

	SECTION("extension 8") {
		filesys::path mock("foldername/..");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 9") {
		filesys::path mock("/");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 10") {
		filesys::path mock("/..");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 11") {
		filesys::path mock("../");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 12") {
		filesys::path mock("./");
		CHECK(mock.extension().string() == "");
	}

	SECTION("extension 13") {
		filesys::path mock("foldername/.");
		CHECK(mock.extension().string() == "");
	}
}

