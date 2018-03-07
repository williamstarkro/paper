#include <gtest/gtest.h>
#include <paper/core/core.hpp>
#include <fstream>

TEST (ed25519, signing)
{
    paper::uint256_union prv;
    paper::uint256_union pub;
    ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
    paper::uint256_union message;
    paper::uint512_union signature;
    ed25519_sign (message.bytes.data (), sizeof (message.bytes), prv.bytes.data (), pub.bytes.data (), signature.bytes.data ());
    auto valid1 (ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), pub.bytes.data (), signature.bytes.data ()));
    ASSERT_EQ (0, valid1);
    signature.bytes [32] ^= 0x1;
    auto valid2 (ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), pub.bytes.data (), signature.bytes.data ()));
    ASSERT_NE (0, valid2);
}

TEST (transaction_block, empty)
{
    paper::keypair key1;
    paper::send_block block;
    block.hashables.previous.clear ();
    block.hashables.balance = 13;
    paper::uint256_union hash (block.hash ());
    paper::sign_message (key1.prv, key1.pub, hash, block.signature);
    ASSERT_FALSE (paper::validate_message (key1.pub, hash, block.signature));
    block.signature.bytes [32] ^= 0x1;
    ASSERT_TRUE (paper::validate_message (key1.pub, hash, block.signature));
}

TEST (send_block, empty_send_serialize)
{
    paper::send_block block1;
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream1 (bytes);
        block1.serialize (stream1);
    }
    auto data (bytes.data ());
    auto size (bytes.size ());
    ASSERT_NE (nullptr, data);
    ASSERT_NE (0, size);
    paper::bufferstream stream2 (data, size);
    paper::send_block block2;
    block2.deserialize (stream2);
    ASSERT_EQ (block1, block2);
}

TEST (send_block, receive_serialize)
{
    paper::receive_block block1;
    paper::keypair key1;
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream1 (bytes);
        block1.serialize (stream1);
    }
    paper::bufferstream stream2 (bytes.data (), bytes.size ());
    paper::receive_block block2;
    auto error (block2.deserialize (stream2));
    ASSERT_FALSE (error);
    ASSERT_EQ (block1, block2);
}

TEST (uint512_union, parse_zero)
{
    paper::uint512_union input (paper::uint512_t (0));
    std::string text;
    input.encode_hex (text);
    paper::uint512_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint512_union, parse_zero_short)
{
    std::string text ("0");
    paper::uint512_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint512_union, parse_one)
{
    paper::uint512_union input (paper::uint512_t (1));
    std::string text;
    input.encode_hex (text);
    paper::uint512_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_EQ (1, output.number ());
}

TEST (uint512_union, parse_error_symbol)
{
    paper::uint512_union input (paper::uint512_t (1000));
    std::string text;
    input.encode_hex (text);
    text [5] = '!';
    paper::uint512_union output;
    auto error (output.decode_hex (text));
    ASSERT_TRUE (error);
}

TEST (uint512_union, max)
{
    paper::uint512_union input (std::numeric_limits <paper::uint512_t>::max ());
    std::string text;
    input.encode_hex (text);
    paper::uint512_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_EQ (paper::uint512_t ("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), output.number ());
}

TEST (uint512_union, parse_error_overflow)
{
    paper::uint512_union input (std::numeric_limits <paper::uint512_t>::max ());
    std::string text;
    input.encode_hex (text);
    text.push_back (0);
    paper::uint512_union output;
    auto error (output.decode_hex (text));
    ASSERT_TRUE (error);
}

TEST (send_block, deserialize)
{
    paper::send_block block1;
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream1 (bytes);
        paper::serialize_block (stream1, block1);
    }
    paper::bufferstream stream2 (bytes.data (), bytes.size ());
    auto block2 (paper::deserialize_block (stream2));
    ASSERT_NE (nullptr, block2);
    ASSERT_EQ (block1, *block2);
}

TEST (receive_block, deserialize)
{
    paper::receive_block block1;
    block1.hashables.previous = 2;
    block1.hashables.source = 4;
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream1 (bytes);
        paper::serialize_block (stream1, block1);
    }
    paper::bufferstream stream2 (bytes.data (), bytes.size ());
    auto block2 (paper::deserialize_block (stream2));
    ASSERT_NE (nullptr, block2);
    ASSERT_EQ (block1, *block2);
}

TEST (send_block, copy)
{
    paper::send_block block1;
    paper::send_block block2 (block1);
    ASSERT_EQ (block1, block2);
}

TEST (block_store, empty_blocks)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    auto begin (store.blocks_begin ());
    auto end (store.blocks_end ());
    ASSERT_EQ (end, begin);
}

TEST (block_store, empty_accounts)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    auto begin (store.latest_begin ());
    auto end (store.latest_end ());
    ASSERT_EQ (end, begin);
}

TEST (block_store, one_block)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::send_block block1;
    store.block_put (block1.hash (), block1);
    auto begin (store.blocks_begin ());
    auto end (store.blocks_end ());
    ASSERT_NE (end, begin);
    auto hash1 (begin->first);
    ASSERT_EQ (block1.hash (), hash1);
    auto block2 (begin->second->clone ());
    ASSERT_EQ (block1, *block2);
    ++begin;
    ASSERT_EQ (end, begin);
}

TEST (block_store, frontier_retrieval)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());;
    paper::address address1;
    paper::frontier frontier1;
    store.latest_put (address1, frontier1);
    paper::frontier frontier2;
    store.latest_get (address1, frontier2);
    ASSERT_EQ (frontier1, frontier2);
}

TEST (block_store, one_account)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::address address;
    paper::block_hash hash;
    store.latest_put (address, {hash, address, 42, 100});
    auto begin (store.latest_begin ());
    auto end (store.latest_end ());
    ASSERT_NE (end, begin);
    ASSERT_EQ (address, begin->first);
    ASSERT_EQ (hash, begin->second.hash);
    ASSERT_EQ (42, begin->second.balance.number ());
    ASSERT_EQ (100, begin->second.time);
    ++begin;
    ASSERT_EQ (end, begin);
}

TEST (block_store, two_block)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::send_block block1;
    block1.hashables.destination = 1;
    block1.hashables.balance = 2;
    std::vector <paper::block_hash> hashes;
    std::vector <paper::send_block> blocks;
    hashes.push_back (block1.hash ());
    blocks.push_back (block1);
    store.block_put (hashes [0], block1);
    paper::send_block block2;
    block2.hashables.destination = 3;
    block2.hashables.balance = 4;
    hashes.push_back (block2.hash ());
    blocks.push_back (block2);
    store.block_put (hashes [1], block2);
    auto begin (store.blocks_begin ());
    auto end (store.blocks_end ());
    ASSERT_NE (end, begin);
    auto hash1 (begin->first);
    ASSERT_NE (hashes.end (), std::find (hashes.begin (), hashes.end (), hash1));
    auto block3 (begin->second->clone ());
    ASSERT_NE (blocks.end (), std::find (blocks.begin (), blocks.end (), *block3));
    ++begin;
    ASSERT_NE (end, begin);
    auto hash2 (begin->first);
    ASSERT_NE (hashes.end (), std::find (hashes.begin (), hashes.end (), hash2));
    auto block4 (begin->second->clone ());
    ASSERT_NE (blocks.end (), std::find (blocks.begin (), blocks.end (), *block4));
    ++begin;
    ASSERT_EQ (end, begin);
}

TEST (block_store, two_account)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::address address1 (1);
    paper::block_hash hash1 (2);
    paper::address address2 (3);
    paper::block_hash hash2 (4);
    store.latest_put (address1, {hash1, address1, 42, 100});
    store.latest_put (address2, {hash2, address2, 84, 200});
    auto begin (store.latest_begin ());
    auto end (store.latest_end ());
    ASSERT_NE (end, begin);
    ASSERT_EQ (address1, begin->first);
    ASSERT_EQ (hash1, begin->second.hash);
    ASSERT_EQ (42, begin->second.balance.number ());
    ASSERT_EQ (100, begin->second.time);
    ++begin;
    ASSERT_NE (end, begin);
    ASSERT_EQ (address2, begin->first);
    ASSERT_EQ (hash2, begin->second.hash);
    ASSERT_EQ (84, begin->second.balance.number ());
    ASSERT_EQ (200, begin->second.time);
    ++begin;
    ASSERT_EQ (end, begin);
}

TEST (block_store, latest_find)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::address address1 (1);
    paper::block_hash hash1 (2);
    paper::address address2 (3);
    paper::block_hash hash2 (4);
    store.latest_put (address1, {hash1, address1, 100});
    store.latest_put (address2, {hash2, address2, 200});
    auto first (store.latest_begin ());
    auto second (store.latest_begin ());
    ++second;
    auto find1 (store.latest_begin (1));
    ASSERT_EQ (first, find1);
    auto find2 (store.latest_begin (3));
    ASSERT_EQ (second, find2);
    auto find3 (store.latest_begin (2));
    ASSERT_EQ (second, find3);
}

TEST (block_store, bad_path)
{
    leveldb::Status init;
    paper::block_store store (init, boost::filesystem::path {});
    ASSERT_FALSE (init.ok ());
}

TEST (block_store, already_open)
{
    auto path (boost::filesystem::unique_path ());
    boost::filesystem::create_directories (path);
    std::ofstream file;
    file.open ((path / "addresses.ldb").string ().c_str ());
    ASSERT_TRUE (file.is_open ());
    leveldb::Status init;
    paper::block_store store (init, path);
    ASSERT_FALSE (init.ok ());
}

TEST (gap_cache, add_new)
{
    paper::gap_cache cache;
    paper::send_block block1;
    cache.add (paper::send_block (block1), block1.previous ());
    ASSERT_NE (cache.blocks.end (), cache.blocks.find (block1.previous ()));
}

TEST (gap_cache, add_existing)
{
    paper::gap_cache cache;
    paper::send_block block1;
    auto previous (block1.previous ());
    cache.add (block1, previous);
    auto existing1 (cache.blocks.find (previous));
    ASSERT_NE (cache.blocks.end (), existing1);
    auto arrival (existing1->arrival);
    while (arrival == std::chrono::system_clock::now ());
    cache.add (block1, previous);
    ASSERT_EQ (1, cache.blocks.size ());
    auto existing2 (cache.blocks.find (previous));
    ASSERT_NE (cache.blocks.end (), existing2);
    ASSERT_GT (existing2->arrival, arrival);
}

TEST (gap_cache, comparison)
{
    paper::gap_cache cache;
    paper::send_block block1;
    block1.hashables.previous.clear ();
    auto previous1 (block1.previous ());
    cache.add (paper::send_block (block1), previous1);
    auto existing1 (cache.blocks.find (previous1));
    ASSERT_NE (cache.blocks.end (), existing1);
    auto arrival (existing1->arrival);
    while (std::chrono::system_clock::now () == arrival);
    paper::send_block block3;
    block3.hashables.previous = 42;
    auto previous2 (block3.previous ());
    cache.add (paper::send_block (block3), previous2);
    ASSERT_EQ (2, cache.blocks.size ());
    auto existing2 (cache.blocks.find (previous2));
    ASSERT_NE (cache.blocks.end (), existing2);
    ASSERT_GT (existing2->arrival, arrival);
    ASSERT_EQ (arrival, cache.blocks.get <1> ().begin ()->arrival);
}

TEST (gap_cache, limit)
{
    paper::gap_cache cache;
    for (auto i (0); i < cache.max * 2; ++i)
    {
        paper::send_block block1;
        block1.hashables.previous = i;
        auto previous (block1.previous ());
        cache.add (paper::send_block (block1), previous);
    }
    ASSERT_EQ (cache.max, cache.blocks.size ());
}

TEST (frontier_req, serialization)
{
    paper::frontier_req request1;
    request1.start = 1;
    request1.age = 2;
    request1.count = 3;
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream (bytes);
        request1.serialize (stream);
    }
    paper::bufferstream buffer (bytes.data (), bytes.size ());
    paper::frontier_req request2;
    ASSERT_FALSE (request2.deserialize (buffer));
    ASSERT_EQ (request1, request2);
}

TEST (work, one)
{
    paper::work work;
    paper::uint256_union seed;
    ed25519_randombytes_unsafe (seed.bytes.data (), sizeof (seed));
    paper::uint256_union nonce;
    ed25519_randombytes_unsafe (nonce.bytes.data (), sizeof (nonce));
    auto value1 (work.generate (seed, nonce));
	auto value2 (work.generate (seed, nonce));
	ASSERT_EQ (value1, value2);
}

TEST (work, create)
{
    paper::uint256_union source (1);
    paper::work work;
    auto begin1 (std::chrono::high_resolution_clock::now ());
    auto value (work.create (source));
    auto end1 (std::chrono::high_resolution_clock::now ());
    EXPECT_FALSE (work.validate (source, value));
    auto end2 (std::chrono::high_resolution_clock::now ());
    std::cerr << boost::str (boost::format ("Generation time: %1%us validation time: %2%us\n") % std::chrono::duration_cast <std::chrono::microseconds> (end1 - begin1).count () % std::chrono::duration_cast <std::chrono::microseconds> (end2 - end1).count ());

}