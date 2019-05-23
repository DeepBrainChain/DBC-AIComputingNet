/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : test_id_generator_nodeid.cpp
* description       : id_generator_nodeid.cpp
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "id_generator.h"
#include "util.h"

BOOST_AUTO_TEST_CASE(test_id_generator_nodeid)
{
    node_info info;
    id_generator gen;
    (void) gen.generate_node_info(info);

    auto ret = gen.check_node_id(info.node_id);
    BOOST_TEST(ret == true);
    ret = gen.check_node_private_key(info.node_private_key);
    BOOST_TEST(ret == true);
}

BOOST_AUTO_TEST_CASE(test_id_generator_nonceid)
{
    id_generator gen;
    std::string nonce_id = gen.generate_nonce();
    BOOST_TEST(gen.check_base58_id(nonce_id) == true);
}

BOOST_AUTO_TEST_CASE(test_id_generator_taskid)
{
    id_generator gen;
    std::string task_id = gen.generate_task_id();
    BOOST_TEST(gen.check_base58_id(task_id) == true);
}

BOOST_AUTO_TEST_CASE(test_id_generator_sessionid)
{
    id_generator gen;
    std::string session_id = gen.generate_session_id();
    BOOST_TEST(gen.check_base58_id(session_id) == true);
}

// big data test
// test nonceid count = 10 000 000 000
const std::string TEST_PATH_NAME = "test";
const std::string TEST_FILE_NAME = "data";
constexpr uint32_t TEST_FILE_TOTAL = 1000;
constexpr uint64_t NONCE_TOTAL_COUNT = 10000;  // 10000000000 ,10000
constexpr uint32_t SPLIT_COUNT = 100; // 10000000 , 100 how long record write file
namespace bf = boost::filesystem;
namespace fs = boost::filesystem;

static void save_data(std::vector<std::string> *data, fs::path &node_dat_path)
{
    //create or open
    bf::ofstream ofs;
    fs::path file_name;

    for (uint32_t i = 0; i < TEST_FILE_TOTAL; i++)
    {
        file_name = node_dat_path / fs::path(TEST_FILE_NAME + std::to_string(i) + ".txt");
        ofs.open(file_name, std::ios_base::out | std::ios_base::app);
        for (auto iter : data[i])
        {
            ofs.write(iter.c_str(), iter.size());
            ofs << std::endl;
        }
        ofs.close();
    }
}

static void clear_data(std::vector<std::string> *data)
{
    for (uint32_t i = 0; i < TEST_FILE_TOTAL; i++)
    {
        data[i].clear();
    }
}

// sbbm hash
static uint32_t SDBMHash(const char *str)
{
    uint32_t hash = 0;
    while (*str)
    {
        // equivalent to:
        hash = 65599 * hash + (*str++);
        hash = (*str++) + (hash << 6) + (hash << 16) - hash;
    }
    return (hash & 0x7FFFFFFF);
}

static bool check_nonceid(const bf::path &file_name)
{
    std::string str;
    std::vector<std::string> nonceid;
    std::map<std::string, uint32_t> nonce_map;

    if (!bf::exists(file_name))
    {
        return false;
    }

    bf::ifstream ifs(file_name, std::ios_base::in);
    if (ifs)
    {
        while (!ifs.eof())
        {
            std::getline(ifs, str);
            nonce_map[str]++;
        }
    }

    ifs.close();

    // check the same noneid
    for (auto &iter : nonce_map)
    {
        if (iter.second > 1)
        {
            std::cout << "the same noneid :" << iter.first << " num =" << iter.second << std::endl;
            return true;
        }
    }

    return false;
}

BOOST_AUTO_TEST_CASE(test_nonceid_conflict_condition)
{
    fs::path node_dat_path;
    node_dat_path /= matrix::core::path_util::get_exe_dir();
    node_dat_path /= fs::path(TEST_PATH_NAME);

    if (fs::exists(node_dat_path))
    {
        (void) fs::remove_all(node_dat_path);
    }
    fs::create_directory(node_dat_path);
    std::cout << "nonceid conflict test start" << std::endl;

    // create 1000 files, 100W data hash save data into file
    std::vector<std::string> data[TEST_FILE_TOTAL];
    std::string nonce_id;
    uint32_t split_loop = 0;
    uint32_t hash_value = 0;
    id_generator gen;

    for (uint32_t loop = 0; loop < NONCE_TOTAL_COUNT; loop++)
    {
        nonce_id = gen.generate_nonce();
        hash_value = SDBMHash(nonce_id.c_str()) % TEST_FILE_TOTAL;
        data[hash_value].push_back(nonce_id);
        if (++split_loop >= SPLIT_COUNT)
        {
            save_data(data, node_dat_path);
            split_loop = 0;
            clear_data(data);
        }
    }

    // remind data save
    if (split_loop > 0)
    {
        std::cout << "save nonceid remainder:" << split_loop << std::endl;
        save_data(data, node_dat_path);
    }

    std::cout << "compare file nodeid start" << std::endl;
    // compare file nodeid is the same
    bf::path file_name;
    bool check_ret = true;
    uint32_t check_num = 0;
    for (check_num = 0; check_num < TEST_FILE_TOTAL; check_num++)
    {
        file_name = node_dat_path / fs::path(TEST_FILE_NAME + std::to_string(check_num) + ".txt");
        if ((check_ret = check_nonceid(file_name)))
            break;
    }

    std::cout << "nonceid conflict test end" << std::endl;
    BOOST_TEST(check_ret != true);
}


BOOST_AUTO_TEST_CASE(test_id_generator)
{
    int32_t ret;
    bool isOK;
    node_info info;
    std::vector<uint8_t> vch_node;
    id_generator gen;
    std::string message = "HELLO WORLD";
    std::string node_id;

    std::string valid_nodeid = "2gfpp3MAB4GHjoYppisLGxsAuiHDppvFCsM2GcdPKpx";
    std::string invalid_nodeid = "2gfpp3MAB4GHjoYppisLGxsAuiHDppvFCsM2GcdPKpz";

    std::string private_key = "29SjBR3HHvBSPMGBqhzjSywqhKEtztUhZCx2JnLvLMQyKpJ6mH8LZ62rnuWFbMxEfkbYNMGajaGF1HGV45A4SEzTR3E1qwUmxDy8uZ1obuNmteyk17PaXTsRqe3qRGfTdxPkMth4Dxa8k1M4DnCxZUg5g6kkdew7awr26vswkNZYkqMZwK9EjyTjUjsJhU9XgSE9JqQRWSVqFKB5EYBoXsrS5UJ21BsbojNEepUFiuDRRxS7fbi4QY7HjteXGB1BbeiboAxVyCicrF6eGXDLtPvaSV454PQLybQG1CYSpB5zx";
    std::string invalid_key = "29SjBR3HHvBSPMGBqhzjSywqhKEtztUhZCx2JnLvLMQyKpJ6mH8LZ62rnuWFbMxEfkbYNMGajaGF1HGV45A4SEzTR3E1qwUmxDy8uZ1obuNmteyk17PaXTsRqe3qRGfTdxPkMth4Dxa8k1M4DnCxZUg5g6kkdew7awr26vswkNZYkqMZwK9EjyTjUjsJhU9XgSE9JqQRWSVqFKB5EYBoXsrS5UJ21BsbojNEepUFiuDRRxS7fbi4QY7HjteXGB1BbeiboAxVyCicrF6eGXDLtPvaSV454PQLybQG1CYSpB5zX";

    ret = gen.generate_node_info(info);
    BOOST_TEST_CHECK(E_SUCCESS == ret, "gen.generate_node_info(info)");

    do
    {
        isOK = gen.check_node_id(info.node_id);
        BOOST_TEST_CHECK(isOK, "gen.check_node_id(info.node_id)");

        isOK = gen.check_node_id(valid_nodeid);
        BOOST_TEST_CHECK(isOK, "gen.check_node_id(info.node_id),<normal case>");

        isOK = gen.check_node_id(invalid_nodeid);
        BOOST_TEST_CHECK(isOK == false, "gen.check_node_id(info.node_id),<abnormal case>");
    } while (false);


    do
    {
        isOK = gen.check_base58_id(info.node_id);
        BOOST_TEST_CHECK(isOK, "gen.check_base58_id(info.node_id)");


        isOK = gen.check_base58_id(valid_nodeid);
        BOOST_TEST_CHECK(isOK, "gen.check_base58_id(info.node_id) <normal case>");

        isOK = gen.check_base58_id(invalid_nodeid);
        BOOST_TEST_CHECK(isOK == false, "gen.check_base58_id(info.node_id) <abnormal case>");

    } while (false);


    do
    {
        isOK = gen.check_node_private_key(info.node_private_key);
        BOOST_TEST_CHECK(isOK, "gen.check_node_private_key(info.node_private_key)");

        isOK = gen.check_node_private_key(private_key);
        BOOST_TEST_CHECK(isOK, "gen.check_node_private_key(info.node_private_key) <normal case>");

        isOK = gen.check_node_private_key(invalid_key);
        BOOST_TEST_CHECK(isOK == false, "gen.check_node_private_key(info.node_private_key) <abnormal case>");

    } while (false);


    do
    {
        isOK = gen.derive_node_id_by_private_key(info.node_private_key, node_id);
        BOOST_TEST_CHECK(isOK, "gen.derive_node_id_by_private_key(info.node_private_key,info.node_id)");

        isOK = gen.derive_node_id_by_private_key(private_key, node_id);
        BOOST_TEST_CHECK(isOK, "gen.derive_node_id_by_private_key(info.node_private_key,info.node_id)  <normal case> ");

        isOK = gen.derive_node_id_by_private_key(invalid_key, node_id);
        BOOST_TEST_CHECK(isOK == false,
                         "gen.derive_node_id_by_private_key(info.node_private_key,info.node_id) <abnormal case>");

    } while (false);


    std::string session_id = gen.generate_session_id();
    BOOST_TEST_CHECK(session_id.empty() == false, "gen.generate_session_id()");

    std::string nonce = gen.generate_nonce();
    BOOST_TEST_CHECK(nonce.empty() == false, "gen.generate_nonce()");


    std::string task_id = gen.generate_task_id();
    BOOST_TEST_CHECK(task_id.empty() == false, "gen.generate_task_id()");

    std::string _sign = gen.sign(message, info.node_private_key);
    BOOST_TEST_CHECK(_sign.empty() == false, "gen.sign(message,info.node_private_key)");

    node_id.clear();
    isOK = gen.derive_node_id_by_sign(message, _sign, node_id);
    BOOST_TEST_CHECK(node_id == info.node_id, "id_generator::derive_node_id_by_sign( message, sign,info.node_id)");


}