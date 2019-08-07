﻿#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_TEST_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_TEST_H

#include "../src/workspace.h"
#include "../src/file_manager_impl.h"
#include "../src/file_impl.h"

#include <iterator>
#include <algorithm>
#include <fstream>

using namespace hlasm_plugin::parser_library;

class workspace_test : public::diagnosable_impl, public testing::Test
{
	void collect_diags() const override {}

};

TEST_F(workspace_test, parse_lib_provider)
{
	file_manager_impl file_mngr;

#if _WIN32
	workspace ws("test\\library\\test_wks", file_mngr);
#else
	workspace ws("test/library/test_wks", file_mngr);
#endif
	ws.open();

	collect_diags_from_child(ws);
	collect_diags_from_child(file_mngr);
	EXPECT_EQ(diags().size(), (size_t)0);

	file_mngr.add_processor_file("test\\library\\test_wks\\correct");

#if _WIN32
	ws.did_open_file("test\\library\\test_wks\\correct");
	context::hlasm_context ctx_1("test\\library\\test_wks\\correct");
	context::hlasm_context ctx_2("test\\library\\test_wks\\correct");
#else
	ws.did_open_file("test/library/test_wks/correct");
	context::hlasm_context ctx_1("test/library/test_wks/correct");
	context::hlasm_context ctx_2("test/library/test_wks/correct");
#endif

	collect_diags_from_child(file_mngr);
	EXPECT_EQ(diags().size(), (size_t)0);

	diags().clear();

	ws.parse_library("MACRO1", ctx_1, library_data{ context::file_processing_type::MACRO,ctx_1.ids().add("MACRO1") });

	//test, that macro1 is parsed, once we are able to parse macros (mby in ctx)

	collect_diags_from_child(ws);
	EXPECT_EQ(diags().size(), (size_t) 0);

	ws.parse_library("not_existing", ctx_2, library_data{ context::file_processing_type::MACRO,ctx_1.ids().add("not_existing") });

}

class file_proc_grps : public file_impl
{
public:
	file_proc_grps() :file_impl("proc_grps.json") {}

	file_uri uri = "test_uri";

	virtual const file_uri & get_file_name() override
	{
		return uri;
	}

	virtual const std::string & get_text() override
	{
		return file;
	}

#ifdef _WIN32
	std::string file = R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [
        "C:\\Users\\Desktop\\ASLib",
        "lib",
        "libs\\lib2\\",
		"",
        {
          "run": "ftp \\192.168.12.145\\MyASLib",
          "list": "",
          "cache_expire":"",
          "location": "downLibs"
        }
      ]
    },
    {
      "name": "P2",
      "libs": [
        "C:\\Users\\Desktop\\ASLib",
        "P2lib",
        "P2libs\\libb",
        {
          "run": "ftp \\192.168.12.145\\MyASLib",
          "location": "\\downLibs"
        }
      ]
    }
  ]
})";
#else
	std::string file = R"({
		"pgroups": [
			{
				"name": "P1",
				"libs": [
					"/home/user/ASLib",
					"lib",
					"libs/lib2/",
			"",
					{
						"run": "ftp /192.168.12.145/MyASLib",
						"list": "",
						"cache_expire":"",
						"location": "downLibs"
					}
				]
			},
			{
				"name": "P2",
				"libs": [
					"/home/user/ASLib",
					"P2lib",
					"P2libs/libb",
					{
						"run": "ftp /192.168.12.145/MyASLib",
						"location": "downLibs"
					}
				]
			}
		]
	})";
#endif //_WIN32
};

class file_pgm_conf : public file_impl
{
public:
	file_pgm_conf() :file_impl("proc_grps.json") {}

	file_uri uri = "test_uri";

	virtual const file_uri & get_file_name() override
	{
		return uri;
	}

	virtual const std::string & get_text() override
	{
		return file;
	}

#if _WIN32
	std::string file = R"({
  "pgms": [
    {
      "program": "pgm1",
      "pgroup": "P1"
    },
    {
      "program": "pgms\\\\.*",
      "pgroup": "P2"
    }
  ]
})";
#else
	std::string file = R"({
  "pgms": [
    {
      "program": "pgm1",
      "pgroup": "P1"
    },
    {
      "program": "pgms/.*",
      "pgroup": "P2"
    }
  ]
})";
#endif
};

class file_manager_proc_grps_test : public file_manager_impl
{

public:
	file * add_file(const file_uri & uri) override
	{
		if (uri.substr(uri.size() - 14) == "proc_grps.json")
			return &proc_grps;
		else
			return &pgm_conf;
	}

	file_proc_grps proc_grps;
	file_pgm_conf pgm_conf;


	// Inherited via file_manager
	virtual void did_open_file(const std::string &, version_t, std::string) override {}
	virtual void did_change_file(const std::string &, version_t, const document_change *, size_t) override {}
	virtual void did_close_file(const std::string &) override {}

	// Inherited via file_manager
	virtual std::vector<file*> list_files() override { return {}; }
};

TEST(workspace, load_config_synthetic)
{
	file_manager_proc_grps_test file_manager;
	workspace ws("test_proc_grps_uri", "test_proc_grps_name", file_manager);

	ws.open();

	auto & pg = ws.get_proc_grp("P1");
	EXPECT_EQ("P1", pg.name());
#ifdef _WIN32
	std::string expected[4]{ "C:\\Users\\Desktop\\ASLib\\", "test_proc_grps_uri\\lib\\", "test_proc_grps_uri\\libs\\lib2\\", "test_proc_grps_uri\\" };
#else
	std::string expected[4]{ "/home/user/ASLib/", "test_proc_grps_uri/lib/", "test_proc_grps_uri/libs/lib2/", "test_proc_grps_uri/" };
#endif // _WIN32
	EXPECT_EQ(std::size(expected), pg.libraries().size());
	for (size_t i = 0; i < std::min(std::size(expected), pg.libraries().size()); ++i)
	{
		library_local * libl = dynamic_cast<library_local *>(pg.libraries()[i].get());
		ASSERT_NE(libl, nullptr);
		EXPECT_EQ(expected[i], libl->get_lib_path());
	}

	auto & pg2 = ws.get_proc_grp("P2");
	EXPECT_EQ("P2", pg2.name()); 
#ifdef _WIN32
	std::string expected2[3]{ "C:\\Users\\Desktop\\ASLib\\", "test_proc_grps_uri\\P2lib\\", "test_proc_grps_uri\\P2libs\\libb\\" };
#else
	std::string expected2[3]{ "/home/user/ASLib/", "test_proc_grps_uri/P2lib/", "test_proc_grps_uri/P2libs/libb/" };
#endif // _WIN32
	EXPECT_EQ(std::size(expected2), pg2.libraries().size());
	for (size_t i = 0; i < std::min(std::size(expected2), pg2.libraries().size()); ++i)
	{
		library_local * libl = dynamic_cast<library_local *>(pg2.libraries()[i].get());
		ASSERT_NE(libl, nullptr);
		EXPECT_EQ(expected2[i], libl->get_lib_path());
	}


	//test of pgm_conf and workspace::get_proc_grp_by_program
	#ifdef _WIN32
		auto & pg3 = ws.get_proc_grp_by_program("test_proc_grps_uri\\pgm1");
	#else
		auto & pg3 = ws.get_proc_grp_by_program("test_proc_grps_uri/pgm1");
	#endif
	EXPECT_EQ(pg3.libraries().size(), std::size(expected));
	for (size_t i = 0; i < std::min(std::size(expected), pg3.libraries().size()); ++i)
	{
		library_local * libl = dynamic_cast<library_local *>(pg3.libraries()[i].get());
		ASSERT_NE(libl, nullptr);
		EXPECT_EQ(expected[i], libl->get_lib_path());
	}

	//test of 
	#ifdef _WIN32
		auto & pg4 = ws.get_proc_grp_by_program("test_proc_grps_uri\\pgms\\anything");
	#else
			auto & pg4 = ws.get_proc_grp_by_program("test_proc_grps_uri/pgms/anything");
	#endif
	EXPECT_EQ(pg4.libraries().size(), std::size(expected2));
	for (size_t i = 0; i < std::min(std::size(expected2), pg4.libraries().size()); ++i)
	{
		library_local * libl = dynamic_cast<library_local *>(pg4.libraries()[i].get());
		ASSERT_NE(libl, nullptr);
		EXPECT_EQ(expected2[i], libl->get_lib_path());
	}
}





#endif
