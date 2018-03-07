#include <gtest/gtest.h>

#include <paper/core/core.hpp>
#include <fstream>

TEST (wallet, no_key)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    paper::keypair key1;
    paper::private_key prv1;
    ASSERT_TRUE (wallet.fetch (key1.pub, prv1));
    ASSERT_TRUE (wallet.valid_password ());
}

TEST (wallet, retrieval)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    paper::keypair key1;
    ASSERT_TRUE (wallet.valid_password ());
    wallet.insert (key1.prv);
    paper::private_key prv1;
    ASSERT_FALSE (wallet.fetch (key1.pub, prv1));
    ASSERT_TRUE (wallet.valid_password ());
    ASSERT_EQ (key1.prv, prv1);
    wallet.password.values [0]->bytes [16] ^= 1;
    paper::private_key prv2;
    ASSERT_TRUE (wallet.fetch (key1.pub, prv2));
    ASSERT_FALSE (wallet.valid_password ());
}

TEST (wallet, empty_iteration)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    auto i (wallet.begin ());
    auto j (wallet.end ());
    ASSERT_EQ (i, j);
}

TEST (wallet, one_item_iteration)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    paper::keypair key1;
    wallet.insert (key1.prv);
    for (auto i (wallet.begin ()), j (wallet.end ()); i != j; ++i)
    {
        ASSERT_EQ (key1.pub, i->first);
        ASSERT_EQ (key1.prv, i->second.prv (wallet.wallet_key (), wallet.salt ().owords [0]));
    }
}

TEST (wallet, two_item_iteration)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    paper::keypair key1;
    paper::keypair key2;
    wallet.insert (key1.prv);
    wallet.insert (key2.prv);
    std::vector <paper::public_key> keys1;
    std::vector <paper::private_key> keys2;
    for (auto i (wallet.begin ()), j (wallet.end ()); i != j; ++i)
    {
        keys1.push_back (i->first);
        keys2.push_back (i->second.prv (wallet.wallet_key (), wallet.salt ().owords [0]));
    }
    ASSERT_EQ (2, keys1.size ());
    ASSERT_EQ (2, keys2.size ());
    ASSERT_NE (keys1.end (), std::find (keys1.begin (), keys1.end (), key1.pub));
    ASSERT_NE (keys2.end (), std::find (keys2.begin (), keys2.end (), key1.prv));
    ASSERT_NE (keys1.end (), std::find (keys1.begin (), keys1.end (), key2.pub));
    ASSERT_NE (keys2.end (), std::find (keys2.begin (), keys2.end (), key2.prv));
}

TEST (wallet, insufficient_spend)
{
    bool init1;
    paper::wallet wallet (init1, boost::filesystem::unique_path ());
    ASSERT_FALSE (init1);
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init2;
    paper::ledger ledger (init2, init, store);
    ASSERT_FALSE (init2);
    paper::keypair key1;
    std::vector <std::unique_ptr <paper::send_block>> blocks;
    ASSERT_TRUE (wallet.generate_send (ledger, key1.pub, 500, blocks));
}

TEST (wallet, one_spend)
{
    bool init1;
    paper::wallet wallet (init1, boost::filesystem::unique_path ());
    ASSERT_FALSE (init1);
    wallet.insert (paper::test_genesis_key.prv);
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init2;
    paper::ledger ledger (init2, init, store);
    ASSERT_FALSE (init2);
    paper::genesis genesis;
    genesis.initialize (store);
    paper::frontier frontier1;
    store.latest_get (paper::test_genesis_key.pub, frontier1);
    paper::keypair key2;
    std::vector <std::unique_ptr <paper::send_block>> blocks;
    ASSERT_FALSE (wallet.generate_send (ledger, key2.pub, std::numeric_limits <paper::uint128_t>::max (), blocks));
    ASSERT_EQ (1, blocks.size ());
    auto & send (*blocks [0]);
    ASSERT_EQ (frontier1.hash, send.hashables.previous);
    ASSERT_EQ (0, send.hashables.balance.number ());
    ASSERT_FALSE (paper::validate_message (paper::test_genesis_key.pub, send.hash (), send.signature));
    ASSERT_EQ (key2.pub, send.hashables.destination);
}

TEST (wallet, DISABLED_two_spend)
{/*
    paper::keypair key1;
    paper::keypair key2;
    paper::uint256_union password;
    paper::wallet wallet (0, paper::wallet_temp);
    wallet.insert (key1.pub, key1.prv, password);
    wallet.insert (key2.pub, key2.prv, password);
    paper::block_store store (paper::block_store_temp);
    paper::ledger ledger (store);
    paper::genesis genesis1 (key1.pub, 100);
    genesis1.initialize (store);
    paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    paper::genesis genesis2 (key2.pub, 400);
    genesis2.initialize (store);
    paper::frontier frontier2;
    ASSERT_FALSE (store.latest_get (key2.pub, frontier2));
    paper::keypair key3;
    std::vector <std::unique_ptr <paper::send_block>> blocks;
    ASSERT_FALSE (wallet.generate_send (ledger, key3.pub, 500, password, blocks));
    ASSERT_EQ (2, blocks.size ());
    ASSERT_TRUE (std::all_of (blocks.begin (), blocks.end (), [] (std::unique_ptr <paper::send_block> const & block) {return block->hashables.balance == 0;}));
    ASSERT_TRUE (std::all_of (blocks.begin (), blocks.end (), [key3] (std::unique_ptr <paper::send_block> const & block) {return block->hashables.destination == key3.pub;}));
    ASSERT_TRUE (std::any_of (blocks.begin (), blocks.end (), [key1] (std::unique_ptr <paper::send_block> const & block) {return !paper::validate_message(key1.pub, block->hash (), block->signature);}));
    ASSERT_TRUE (std::any_of (blocks.begin (), blocks.end (), [key2] (std::unique_ptr <paper::send_block> const & block) {return !paper::validate_message(key2.pub, block->hash (), block->signature);}));*/
}

TEST (wallet, partial_spend)
{
    bool init1;
    paper::wallet wallet (init1, boost::filesystem::unique_path ());
    ASSERT_FALSE (init1);
    wallet.insert (paper::test_genesis_key.prv);
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init2;
    paper::ledger ledger (init2, init, store);
    ASSERT_FALSE (init2);
    paper::genesis genesis;
    genesis.initialize (store);
    paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (paper::test_genesis_key.pub, frontier1));
    paper::keypair key2;
    std::vector <std::unique_ptr <paper::send_block>> blocks;
    ASSERT_FALSE (wallet.generate_send (ledger, key2.pub, 500, blocks));
    ASSERT_EQ (1, blocks.size ());
    ASSERT_EQ (frontier1.hash, blocks [0]->hashables.previous);
    ASSERT_EQ (std::numeric_limits <paper::uint128_t>::max () - 500, blocks [0]->hashables.balance.number ());
    ASSERT_FALSE (paper::validate_message (paper::test_genesis_key.pub, blocks [0]->hash (), blocks [0]->signature));
    ASSERT_EQ (key2.pub, blocks [0]->hashables.destination);
}

TEST (wallet, spend_no_previous)
{
    bool init1;
    paper::wallet wallet (init1, boost::filesystem::unique_path ());
    ASSERT_FALSE (init1);
    for (auto i (0); i < 50; ++i)
    {
        paper::keypair key;
        wallet.insert (key.prv);
    }
    wallet.insert (paper::test_genesis_key.prv);
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init2;
    paper::ledger ledger (init2, init, store);
    ASSERT_FALSE (init2);
    paper::genesis genesis;
    genesis.initialize (store);
    paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (paper::test_genesis_key.pub, frontier1));
    for (auto i (0); i < 50; ++i)
    {
        paper::keypair key;
        wallet.insert (key.prv);
    }
    paper::keypair key2;
    std::vector <std::unique_ptr <paper::send_block>> blocks;
    ASSERT_FALSE (wallet.generate_send (ledger, key2.pub, 500, blocks));
    ASSERT_EQ (1, blocks.size ());
    ASSERT_EQ (frontier1.hash, blocks [0]->hashables.previous);
    ASSERT_EQ (std::numeric_limits <paper::uint128_t>::max () - 500, blocks [0]->hashables.balance.number ());
    ASSERT_FALSE (paper::validate_message (paper::test_genesis_key.pub, blocks [0]->hash (), blocks [0]->signature));
    ASSERT_EQ (key2.pub, blocks [0]->hashables.destination);
}

TEST (wallet, find_none)
{
    bool init1;
    paper::wallet wallet (init1, boost::filesystem::unique_path ());
    ASSERT_FALSE (init1);
    paper::uint256_union account;
    ASSERT_EQ (wallet.end (), wallet.find (account));
}

TEST (wallet, find_existing)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    paper::keypair key1;
    wallet.insert (key1.prv);
    auto existing (wallet.find (key1.pub));
    ASSERT_NE (wallet.end (), existing);
    ++existing;
    ASSERT_EQ (wallet.end (), existing);
}

TEST (wallet, rekey)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_EQ (wallet.password.value (), wallet.derive_key (""));
    ASSERT_FALSE (init);
    paper::keypair key1;
    wallet.insert (key1.prv);
    paper::uint256_union prv1;
    wallet.fetch (key1.pub, prv1);
    ASSERT_EQ (key1.prv, prv1);
    ASSERT_FALSE (wallet.rekey ("1"));
    ASSERT_EQ (wallet.derive_key ("1"), wallet.password.value ());
    paper::uint256_union prv2;
    wallet.fetch (key1.pub, prv2);
    ASSERT_EQ (key1.prv, prv2);
    *wallet.password.values [0] = 2;
    ASSERT_TRUE (wallet.rekey ("2"));
}

TEST (base58, encode_zero)
{
    paper::uint256_union number0 (0);
    std::string str0;
    number0.encode_base58check (str0);
    ASSERT_EQ (50, str0.size ());
    paper::uint256_union number1;
    ASSERT_FALSE (number1.decode_base58check (str0));
    ASSERT_EQ (number0, number1);
}

TEST (base58, encode_all)
{
    paper::uint256_union number0;
    number0.decode_hex ("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    std::string str0;
    number0.encode_base58check (str0);
    ASSERT_EQ (50, str0.size ());
    paper::uint256_union number1;
    ASSERT_FALSE (number1.decode_base58check (str0));
    ASSERT_EQ (number0, number1);
}

TEST (base58, encode_fail)
{
    paper::uint256_union number0 (0);
    std::string str0;
    number0.encode_base58check (str0);
    str0 [16] ^= 1;
    paper::uint256_union number1;
    ASSERT_TRUE (number1.decode_base58check (str0));
}

TEST (wallet, hash_password)
{
    bool init;
    paper::wallet wallet (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
    auto hash1 (wallet.derive_key (""));
    auto hash2 (wallet.derive_key (""));
    ASSERT_EQ (hash1, hash2);
    auto hash3 (wallet.derive_key ("a"));
    ASSERT_NE (hash1, hash3);
}

TEST (fan, reconstitute)
{
    paper::uint256_union value0;
    paper::fan fan (value0, 1024);
    for (auto & i: fan.values)
    {
        ASSERT_NE (value0, *i);
    }
    auto value1 (fan.value ());
    ASSERT_EQ (value0, value1);
}

TEST (fan, change)
{
    paper::uint256_union value0;
    paper::uint256_union value1;
    ASSERT_NE (value0, value1);
    paper::fan fan (value0, 1024);
    ASSERT_EQ (value0, fan.value ());
    fan.value_set (value1);
    ASSERT_EQ (value1, fan.value ());
}

TEST (wallet, bad_path)
{
    bool init;
    paper::wallet store (init, boost::filesystem::path {});
    ASSERT_TRUE (init);
}

TEST (wallet, correct)
{
    bool init (true);
    paper::wallet store (init, boost::filesystem::unique_path ());
    ASSERT_FALSE (init);
}

TEST (wallet, already_open)
{
    auto path (boost::filesystem::unique_path ());
    boost::filesystem::create_directories (path);
    std::ofstream file;
    file.open ((path / "wallet.ldb").string ().c_str ());
    ASSERT_TRUE (file.is_open ());
    bool init;
    paper::wallet store (init, path);
    ASSERT_TRUE (init);
}

TEST (wallet, repoen_default_password)
{
    auto path (boost::filesystem::unique_path ());
    {
        bool init;
        paper::wallet wallet (init, path);
        ASSERT_FALSE (init);
        ASSERT_TRUE (wallet.valid_password ());
    }
    {
        bool init;
        paper::wallet wallet (init, path);
        ASSERT_FALSE (init);
        ASSERT_TRUE (wallet.valid_password ());
    }
}