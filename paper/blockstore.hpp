#pragma once

#include <paper/common.hpp>

namespace paper
{
/**
 * The value produced when iterating with \ref store_iterator
 */
class store_entry
{
public:
	store_entry ();
	void clear ();
	store_entry * operator-> ();
	paper::mdb_val first;
	paper::mdb_val second;
};

/**
 * Iterates the key/value pairs of a transaction
 */
class store_iterator
{
public:
	store_iterator (MDB_txn *, MDB_dbi);
	store_iterator (std::nullptr_t);
	store_iterator (MDB_txn *, MDB_dbi, MDB_val const &);
	store_iterator (paper::store_iterator &&);
	store_iterator (paper::store_iterator const &) = delete;
	~store_iterator ();
	paper::store_iterator & operator++ ();
	void next_dup ();
	paper::store_iterator & operator= (paper::store_iterator &&);
	paper::store_iterator & operator= (paper::store_iterator const &) = delete;
	paper::store_entry & operator-> ();
	bool operator== (paper::store_iterator const &) const;
	bool operator!= (paper::store_iterator const &) const;
	MDB_cursor * cursor;
	paper::store_entry current;
};

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
	block_store (bool &, boost::filesystem::path const &, int lmdb_max_dbs = 128);

	MDB_dbi block_database (paper::block_type);
	void block_put_raw (MDB_txn *, MDB_dbi, paper::block_hash const &, MDB_val);
	void block_put (MDB_txn *, paper::block_hash const &, paper::block const &, paper::block_hash const & = paper::block_hash (0));
	MDB_val block_get_raw (MDB_txn *, paper::block_hash const &, paper::block_type &);
	paper::block_hash block_successor (MDB_txn *, paper::block_hash const &);
	void block_successor_clear (MDB_txn *, paper::block_hash const &);
	std::unique_ptr<paper::block> block_get (MDB_txn *, paper::block_hash const &);
	std::unique_ptr<paper::block> block_random (MDB_txn *);
	std::unique_ptr<paper::block> block_random (MDB_txn *, MDB_dbi);
	void block_del (MDB_txn *, paper::block_hash const &);
	bool block_exists (MDB_txn *, paper::block_hash const &);
	paper::block_counts block_count (MDB_txn *);

	void frontier_put (MDB_txn *, paper::block_hash const &, paper::account const &);
	paper::account frontier_get (MDB_txn *, paper::block_hash const &);
	void frontier_del (MDB_txn *, paper::block_hash const &);
	size_t frontier_count (MDB_txn *);

	void account_put (MDB_txn *, paper::account const &, paper::account_info const &);
	bool account_get (MDB_txn *, paper::account const &, paper::account_info &);
	void account_del (MDB_txn *, paper::account const &);
	bool account_exists (MDB_txn *, paper::account const &);
	paper::store_iterator latest_begin (MDB_txn *, paper::account const &);
	paper::store_iterator latest_begin (MDB_txn *);
	paper::store_iterator latest_end ();

	void pending_put (MDB_txn *, paper::pending_key const &, paper::pending_info const &);
	void pending_del (MDB_txn *, paper::pending_key const &);
	bool pending_get (MDB_txn *, paper::pending_key const &, paper::pending_info &);
	bool pending_exists (MDB_txn *, paper::pending_key const &);
	paper::store_iterator pending_begin (MDB_txn *, paper::pending_key const &);
	paper::store_iterator pending_begin (MDB_txn *);
	paper::store_iterator pending_end ();

	void block_info_put (MDB_txn *, paper::block_hash const &, paper::block_info const &);
	void block_info_del (MDB_txn *, paper::block_hash const &);
	bool block_info_get (MDB_txn *, paper::block_hash const &, paper::block_info &);
	bool block_info_exists (MDB_txn *, paper::block_hash const &);
	paper::store_iterator block_info_begin (MDB_txn *, paper::block_hash const &);
	paper::store_iterator block_info_begin (MDB_txn *);
	paper::store_iterator block_info_end ();
	paper::uint128_t block_balance (MDB_txn *, paper::block_hash const &);
	static size_t const block_info_max = 32;

	paper::uint128_t representation_get (MDB_txn *, paper::account const &);
	void representation_put (MDB_txn *, paper::account const &, paper::uint128_t const &);
	void representation_add (MDB_txn *, paper::account const &, paper::uint128_t const &);
	paper::store_iterator representation_begin (MDB_txn *);
	paper::store_iterator representation_end ();

	void unchecked_clear (MDB_txn *);
	void unchecked_put (MDB_txn *, paper::block_hash const &, std::shared_ptr<paper::block> const &);
	std::vector<std::shared_ptr<paper::block>> unchecked_get (MDB_txn *, paper::block_hash const &);
	void unchecked_del (MDB_txn *, paper::block_hash const &, paper::block const &);
	paper::store_iterator unchecked_begin (MDB_txn *);
	paper::store_iterator unchecked_begin (MDB_txn *, paper::block_hash const &);
	paper::store_iterator unchecked_end ();
	size_t unchecked_count (MDB_txn *);
	std::unordered_multimap<paper::block_hash, std::shared_ptr<paper::block>> unchecked_cache;

	void unsynced_put (MDB_txn *, paper::block_hash const &);
	void unsynced_del (MDB_txn *, paper::block_hash const &);
	bool unsynced_exists (MDB_txn *, paper::block_hash const &);
	paper::store_iterator unsynced_begin (MDB_txn *, paper::block_hash const &);
	paper::store_iterator unsynced_begin (MDB_txn *);
	paper::store_iterator unsynced_end ();

	void checksum_put (MDB_txn *, uint64_t, uint8_t, paper::checksum const &);
	bool checksum_get (MDB_txn *, uint64_t, uint8_t, paper::checksum &);
	void checksum_del (MDB_txn *, uint64_t, uint8_t);

	paper::vote_result vote_validate (MDB_txn *, std::shared_ptr<paper::vote>);
	// Return latest vote for an account from store
	std::shared_ptr<paper::vote> vote_get (MDB_txn *, paper::account const &);
	// Populate vote with the next sequence number
	std::shared_ptr<paper::vote> vote_generate (MDB_txn *, paper::account const &, paper::raw_key const &, std::shared_ptr<paper::block>);
	// Return either vote or the stored vote with a higher sequence number
	std::shared_ptr<paper::vote> vote_max (MDB_txn *, std::shared_ptr<paper::vote>);
	// Return latest vote for an account considering the vote cache
	std::shared_ptr<paper::vote> vote_current (MDB_txn *, paper::account const &);
	void flush (MDB_txn *);
	paper::store_iterator vote_begin (MDB_txn *);
	paper::store_iterator vote_end ();
	std::mutex cache_mutex;
	std::unordered_map<paper::account, std::shared_ptr<paper::vote>> vote_cache;

	void version_put (MDB_txn *, int);
	int version_get (MDB_txn *);
	void do_upgrades (MDB_txn *);
	void upgrade_v1_to_v2 (MDB_txn *);
	void upgrade_v2_to_v3 (MDB_txn *);
	void upgrade_v3_to_v4 (MDB_txn *);
	void upgrade_v4_to_v5 (MDB_txn *);
	void upgrade_v5_to_v6 (MDB_txn *);
	void upgrade_v6_to_v7 (MDB_txn *);
	void upgrade_v7_to_v8 (MDB_txn *);
	void upgrade_v8_to_v9 (MDB_txn *);
	void upgrade_v9_to_v10 (MDB_txn *);

	void clear (MDB_dbi);

	paper::mdb_env environment;
	// block_hash -> account                                        // Maps head blocks to owning account
	MDB_dbi frontiers;
	// account -> block_hash, representative, balance, timestamp    // Account to head block, representative, balance, last_change
	MDB_dbi accounts;
	// block_hash -> send_block
	MDB_dbi send_blocks;
	// block_hash -> receive_block
	MDB_dbi receive_blocks;
	// block_hash -> open_block
	MDB_dbi open_blocks;
	// block_hash -> change_block
	MDB_dbi change_blocks;
	// block_hash -> sender, amount, destination                    // Pending blocks to sender account, amount, destination account
	MDB_dbi pending;
	// block_hash -> account, balance                               // Blocks info
	MDB_dbi blocks_info;
	// account -> weight                                            // Representation
	MDB_dbi representation;
	// block_hash -> block                                          // Unchecked bootstrap blocks
	MDB_dbi unchecked;
	// block_hash ->                                                // Blocks that haven't been broadcast
	MDB_dbi unsynced;
	// (uint56_t, uint8_t) -> block_hash                            // Mapping of region to checksum
	MDB_dbi checksum;
	// account -> uint64_t											// Highest vote observed for account
	MDB_dbi vote;
	// uint256_union -> ?											// Meta information about block store
	MDB_dbi meta;
};
}
