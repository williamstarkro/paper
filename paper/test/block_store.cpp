#include <gtest/gtest.h>
#include <paper/core/core.hpp>

TEST (block_store, construction)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    auto now (db.now ());
    ASSERT_GT (now, 1408074640);
}

TEST (block_store, add_item)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::send_block block;
    paper::uint256_union hash1 (block.hash ());
    auto latest1 (db.block_get (hash1));
    ASSERT_EQ (nullptr, latest1);
    ASSERT_FALSE (db.block_exists (hash1));
    db.block_put (hash1, block);
    auto latest2 (db.block_get (hash1));
    ASSERT_NE (nullptr, latest2);
    ASSERT_EQ (block, *latest2);
    ASSERT_TRUE (db.block_exists (hash1));
	db.block_del (hash1);
	auto latest3 (db.block_get (hash1));
	ASSERT_EQ (nullptr, latest3);
}

TEST (block_store, add_nonempty_block)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::keypair key1;
    paper::send_block block;
    paper::uint256_union hash1 (block.hash ());
    paper::sign_message (key1.prv, key1.pub, hash1, block.signature);
    auto latest1 (db.block_get (hash1));
    ASSERT_EQ (nullptr, latest1);
    db.block_put (hash1, block);
    auto latest2 (db.block_get (hash1));
    ASSERT_NE (nullptr, latest2);
    ASSERT_EQ (block, *latest2);
}

TEST (block_store, add_two_items)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::keypair key1;
    paper::send_block block;
    block.hashables.balance = 1;
    paper::uint256_union hash1 (block.hash ());
    paper::sign_message (key1.prv, key1.pub, hash1, block.signature);
    auto latest1 (db.block_get (hash1));
    ASSERT_EQ (nullptr, latest1);
    paper::send_block block2;
    block2.hashables.balance = 3;
    paper::uint256_union hash2 (block2.hash ());
    paper::sign_message (key1.prv, key1.pub, hash2, block2.signature);
    auto latest2 (db.block_get (hash2));
    ASSERT_EQ (nullptr, latest2);
    db.block_put (hash1, block);
    db.block_put (hash2, block2);
    auto latest3 (db.block_get (hash1));
    ASSERT_NE (nullptr, latest3);
    ASSERT_EQ (block, *latest3);
    auto latest4 (db.block_get (hash2));
    ASSERT_NE (nullptr, latest4);
    ASSERT_EQ (block2, *latest4);
    ASSERT_FALSE (*latest3 == *latest4);
}

TEST (block_store, add_receive)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::keypair key1;
    paper::keypair key2;
    paper::receive_block block;
    paper::block_hash hash1 (block.hash ());
    auto latest1 (db.block_get (hash1));
    ASSERT_EQ (nullptr, latest1);
    db.block_put (hash1, block);
    auto latest2 (db.block_get (hash1));
    ASSERT_NE (nullptr, latest2);
    ASSERT_EQ (block, *latest2);
}

TEST (block_store, add_pending)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::keypair key1;
    paper::block_hash hash1;
    paper::address sender1;
    paper::amount amount1;
    paper::address destination1;
    auto pending1 (db.pending_get (hash1, sender1, amount1, destination1));
    ASSERT_TRUE (pending1);
    db.pending_put (hash1, sender1, amount1, destination1);
    paper::address sender2;
    paper::amount amount2;
    paper::address destination2;
    auto pending2 (db.pending_get (hash1, sender2, amount2, destination2));
    ASSERT_EQ (sender1, sender2);
    ASSERT_EQ (amount1, amount2);
    ASSERT_EQ (destination1, destination2);
    ASSERT_FALSE (pending2);
    db.pending_del (hash1);
    auto pending3 (db.pending_get (hash1, sender2, amount2, destination2));
    ASSERT_TRUE (pending3);
}

TEST (block_store, add_genesis)
{
    leveldb::Status init;
    paper::block_store db (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::genesis genesis;
    genesis.initialize (db);
    paper::frontier frontier;
    ASSERT_FALSE (db.latest_get (paper::genesis_address, frontier));
    auto block1 (db.block_get (frontier.hash));
    ASSERT_NE (nullptr, block1);
    auto receive1 (dynamic_cast <paper::open_block *> (block1.get ()));
    ASSERT_NE (nullptr, receive1);
    ASSERT_LE (frontier.time, db.now ());
}

TEST (representation, changes)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::keypair key1;
    ASSERT_EQ (0, store.representation_get (key1.pub));
    store.representation_put (key1.pub, 1);
    ASSERT_EQ (1, store.representation_get (key1.pub));
    store.representation_put (key1.pub, 2);
    ASSERT_EQ (2, store.representation_get (key1.pub));
}

TEST (fork, adding_checking)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::keypair key1;
    paper::change_block block1 (key1.pub, paper::uint256_union (0), paper::uint256_union (0), paper::uint256_union (0));
    ASSERT_EQ (nullptr, store.fork_get (block1.hash ()));
    paper::keypair key2;
    paper::change_block block2 (paper::uint256_union (0), paper::uint256_union (0), paper::uint256_union (0), paper::uint256_union (0));
    store.fork_put (block1.hash (), block2);
    auto block3 (store.fork_get (block1.hash ()));
    ASSERT_EQ (block2, *block3);
}

TEST (bootstrap, simple)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::send_block block1;
    auto block2 (store.bootstrap_get (block1.previous ()));
    ASSERT_EQ (nullptr, block2);
    store.bootstrap_put (block1.previous (), block1);
    auto block3 (store.bootstrap_get (block1.previous ()));
    ASSERT_NE (nullptr, block3);
    ASSERT_EQ (block1, *block3);
    store.bootstrap_del (block1.previous ());
    auto block4 (store.bootstrap_get (block1.previous ()));
    ASSERT_EQ (nullptr, block4);
}

TEST (checksum, simple)
{
    leveldb::Status init;
    paper::block_store store (init, paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    paper::block_hash hash0;
    ASSERT_TRUE (store.checksum_get (0x100, 0x10, hash0));
    paper::block_hash hash1;
    store.checksum_put (0x100, 0x10, hash1);
    paper::block_hash hash2;
    ASSERT_FALSE (store.checksum_get (0x100, 0x10, hash2));
    ASSERT_EQ (hash1, hash2);
    store.checksum_del (0x100, 0x10);
    paper::block_hash hash3;
    ASSERT_TRUE (store.checksum_get (0x100, 0x10, hash3));
}