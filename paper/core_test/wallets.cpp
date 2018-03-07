#include <gtest/gtest.h>

#include <paper/node/testing.hpp>

TEST (wallets, open_create)
{
	paper::system system (24000, 1);
	bool error (false);
	paper::wallets wallets (error, *system.nodes[0]);
	ASSERT_FALSE (error);
	ASSERT_EQ (1, wallets.items.size ()); // it starts out with a default wallet
	paper::uint256_union id;
	ASSERT_EQ (nullptr, wallets.open (id));
	auto wallet (wallets.create (id));
	ASSERT_NE (nullptr, wallet);
	ASSERT_EQ (wallet, wallets.open (id));
}

TEST (wallets, open_existing)
{
	paper::system system (24000, 1);
	paper::uint256_union id;
	{
		bool error (false);
		paper::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
		auto wallet (wallets.create (id));
		ASSERT_NE (nullptr, wallet);
		ASSERT_EQ (wallet, wallets.open (id));
		auto iterations (0);
		paper::raw_key password;
		password.data.clear ();
		while (password.data == 0)
		{
			system.poll ();
			++iterations;
			ASSERT_LT (iterations, 200);
			wallet->store.password.value (password);
		}
	}
	{
		bool error (false);
		paper::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (2, wallets.items.size ());
		ASSERT_NE (nullptr, wallets.open (id));
	}
}

TEST (wallets, remove)
{
	paper::system system (24000, 1);
	paper::uint256_union one (1);
	{
		bool error (false);
		paper::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
		auto wallet (wallets.create (one));
		ASSERT_NE (nullptr, wallet);
		ASSERT_EQ (2, wallets.items.size ());
		wallets.destroy (one);
		ASSERT_EQ (1, wallets.items.size ());
	}
	{
		bool error (false);
		paper::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
	}
}

TEST (wallets, wallet_create_max)
{
	paper::system system (24000, 1);
	bool error (false);
	paper::wallets wallets (error, *system.nodes[0]);
	const int nonWalletDbs = 17;
	for (int i = 0; i < system.nodes[0]->config.lmdb_max_dbs - nonWalletDbs; i++)
	{
		paper::keypair key;
		auto wallet = wallets.create (key.pub);
		auto existing = wallets.items.find (key.pub);
		ASSERT_TRUE (existing != wallets.items.end ());
		paper::raw_key seed;
		seed.data = 0;
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		existing->second->store.seed_set (transaction, seed);
	}
	paper::keypair key;
	wallets.create (key.pub);
	auto existing = wallets.items.find (key.pub);
	ASSERT_TRUE (existing == wallets.items.end ());
}
