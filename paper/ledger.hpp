#pragma once

#include <paper/common.hpp>

namespace paper
{
class block_store;

class shared_ptr_block_hash
{
public:
	size_t operator() (std::shared_ptr<paper::block> const &) const;
	bool operator() (std::shared_ptr<paper::block> const &, std::shared_ptr<paper::block> const &) const;
};

class ledger
{
public:
	ledger (paper::block_store &, paper::uint128_t const & = 0);
	std::pair<paper::uint128_t, std::shared_ptr<paper::block>> winner (MDB_txn *, paper::votes const & votes_a);
	// Map of weight -> associated block, ordered greatest to least
	std::map<paper::uint128_t, std::shared_ptr<paper::block>, std::greater<paper::uint128_t>> tally (MDB_txn *, paper::votes const &);
	paper::account account (MDB_txn *, paper::block_hash const &);
	paper::uint128_t amount (MDB_txn *, paper::block_hash const &);
	paper::uint128_t balance (MDB_txn *, paper::block_hash const &);
	paper::uint128_t account_balance (MDB_txn *, paper::account const &);
	paper::uint128_t account_pending (MDB_txn *, paper::account const &);
	paper::uint128_t weight (MDB_txn *, paper::account const &);
	std::unique_ptr<paper::block> successor (MDB_txn *, paper::block_hash const &);
	std::unique_ptr<paper::block> forked_block (MDB_txn *, paper::block const &);
	paper::block_hash latest (MDB_txn *, paper::account const &);
	paper::block_hash latest_root (MDB_txn *, paper::account const &);
	paper::block_hash representative (MDB_txn *, paper::block_hash const &);
	paper::block_hash representative_calculated (MDB_txn *, paper::block_hash const &);
	bool block_exists (paper::block_hash const &);
	std::string block_text (char const *);
	std::string block_text (paper::block_hash const &);
	paper::uint128_t supply (MDB_txn *);
	paper::process_return process (MDB_txn *, paper::block const &);
	void rollback (MDB_txn *, paper::block_hash const &);
	void change_latest (MDB_txn *, paper::account const &, paper::block_hash const &, paper::account const &, paper::uint128_union const &, uint64_t);
	void checksum_update (MDB_txn *, paper::block_hash const &);
	paper::checksum checksum (MDB_txn *, paper::account const &, paper::account const &);
	void dump_account_chain (paper::account const &);
	static paper::uint128_t const unit;
	paper::block_store & store;
	paper::uint128_t inactive_supply;
	std::unordered_map<paper::account, paper::uint128_t> bootstrap_weights;
	uint64_t bootstrap_weight_max_blocks;
	std::atomic<bool> check_bootstrap_weights;
};
};
