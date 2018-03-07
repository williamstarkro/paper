#pragma once

#include <paper/blockstore.hpp>
#include <paper/common.hpp>
#include <paper/node/common.hpp>
#include <paper/node/openclwork.hpp>

#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace paper
{
// The fan spreads a key out over the heap to decrease the likelihood of it being recovered by memory inspection
class fan
{
public:
	fan (paper::uint256_union const &, size_t);
	void value (paper::raw_key &);
	void value_set (paper::raw_key const &);
	std::vector<std::unique_ptr<paper::uint256_union>> values;

private:
	std::mutex mutex;
	void value_get (paper::raw_key &);
};
class wallet_value
{
public:
	wallet_value () = default;
	wallet_value (paper::mdb_val const &);
	wallet_value (paper::uint256_union const &, uint64_t);
	paper::mdb_val val () const;
	paper::private_key key;
	uint64_t work;
};
class node_config;
class kdf
{
public:
	void phs (paper::raw_key &, std::string const &, paper::uint256_union const &);
	std::mutex mutex;
};
enum class key_type
{
	not_a_type,
	unknown,
	adhoc,
	deterministic
};
class wallet_store
{
public:
	wallet_store (bool &, paper::kdf &, paper::transaction &, paper::account, unsigned, std::string const &);
	wallet_store (bool &, paper::kdf &, paper::transaction &, paper::account, unsigned, std::string const &, std::string const &);
	std::vector<paper::account> accounts (MDB_txn *);
	void initialize (MDB_txn *, bool &, std::string const &);
	paper::uint256_union check (MDB_txn *);
	bool rekey (MDB_txn *, std::string const &);
	bool valid_password (MDB_txn *);
	bool attempt_password (MDB_txn *, std::string const &);
	void wallet_key (paper::raw_key &, MDB_txn *);
	void seed (paper::raw_key &, MDB_txn *);
	void seed_set (MDB_txn *, paper::raw_key const &);
	paper::key_type key_type (paper::wallet_value const &);
	paper::public_key deterministic_insert (MDB_txn *);
	void deterministic_key (paper::raw_key &, MDB_txn *, uint32_t);
	uint32_t deterministic_index_get (MDB_txn *);
	void deterministic_index_set (MDB_txn *, uint32_t);
	void deterministic_clear (MDB_txn *);
	paper::uint256_union salt (MDB_txn *);
	bool is_representative (MDB_txn *);
	paper::account representative (MDB_txn *);
	void representative_set (MDB_txn *, paper::account const &);
	paper::public_key insert_adhoc (MDB_txn *, paper::raw_key const &);
	void erase (MDB_txn *, paper::public_key const &);
	paper::wallet_value entry_get_raw (MDB_txn *, paper::public_key const &);
	void entry_put_raw (MDB_txn *, paper::public_key const &, paper::wallet_value const &);
	bool fetch (MDB_txn *, paper::public_key const &, paper::raw_key &);
	bool exists (MDB_txn *, paper::public_key const &);
	void destroy (MDB_txn *);
	paper::store_iterator find (MDB_txn *, paper::uint256_union const &);
	paper::store_iterator begin (MDB_txn *, paper::uint256_union const &);
	paper::store_iterator begin (MDB_txn *);
	paper::store_iterator end ();
	void derive_key (paper::raw_key &, MDB_txn *, std::string const &);
	void serialize_json (MDB_txn *, std::string &);
	void write_backup (MDB_txn *, boost::filesystem::path const &);
	bool move (MDB_txn *, paper::wallet_store &, std::vector<paper::public_key> const &);
	bool import (MDB_txn *, paper::wallet_store &);
	bool work_get (MDB_txn *, paper::public_key const &, uint64_t &);
	void work_put (MDB_txn *, paper::public_key const &, uint64_t);
	unsigned version (MDB_txn *);
	void version_put (MDB_txn *, unsigned);
	void upgrade_v1_v2 ();
	void upgrade_v2_v3 ();
	paper::fan password;
	paper::fan wallet_key_mem;
	static unsigned const version_1;
	static unsigned const version_2;
	static unsigned const version_3;
	static unsigned const version_current;
	static paper::uint256_union const version_special;
	static paper::uint256_union const wallet_key_special;
	static paper::uint256_union const salt_special;
	static paper::uint256_union const check_special;
	static paper::uint256_union const representative_special;
	static paper::uint256_union const seed_special;
	static paper::uint256_union const deterministic_index_special;
	static int const special_count;
	static unsigned const kdf_full_work = 64 * 1024;
	static unsigned const kdf_test_work = 8;
	static unsigned const kdf_work = paper::paper_network == paper::paper_networks::paper_test_network ? kdf_test_work : kdf_full_work;
	paper::kdf & kdf;
	paper::mdb_env & environment;
	MDB_dbi handle;
	std::recursive_mutex mutex;
};
class node;
// A wallet is a set of account keys encrypted by a common encryption key
class wallet : public std::enable_shared_from_this<paper::wallet>
{
public:
	std::shared_ptr<paper::block> change_action (paper::account const &, paper::account const &, bool = true);
	std::shared_ptr<paper::block> receive_action (paper::send_block const &, paper::account const &, paper::uint128_union const &, bool = true);
	std::shared_ptr<paper::block> send_action (paper::account const &, paper::account const &, paper::uint128_t const &, bool = true, boost::optional<std::string> = {});
	wallet (bool &, paper::transaction &, paper::node &, std::string const &);
	wallet (bool &, paper::transaction &, paper::node &, std::string const &, std::string const &);
	void enter_initial_password ();
	bool valid_password ();
	bool enter_password (std::string const &);
	paper::public_key insert_adhoc (paper::raw_key const &, bool = true);
	paper::public_key insert_adhoc (MDB_txn *, paper::raw_key const &, bool = true);
	paper::public_key deterministic_insert (MDB_txn *, bool = true);
	paper::public_key deterministic_insert (bool = true);
	bool exists (paper::public_key const &);
	bool import (std::string const &, std::string const &);
	void serialize (std::string &);
	bool change_sync (paper::account const &, paper::account const &);
	void change_async (paper::account const &, paper::account const &, std::function<void(std::shared_ptr<paper::block>)> const &, bool = true);
	bool receive_sync (std::shared_ptr<paper::block>, paper::account const &, paper::uint128_t const &);
	void receive_async (std::shared_ptr<paper::block>, paper::account const &, paper::uint128_t const &, std::function<void(std::shared_ptr<paper::block>)> const &, bool = true);
	paper::block_hash send_sync (paper::account const &, paper::account const &, paper::uint128_t const &);
	void send_async (paper::account const &, paper::account const &, paper::uint128_t const &, std::function<void(std::shared_ptr<paper::block>)> const &, bool = true, boost::optional<std::string> = {});
	void work_generate (paper::account const &, paper::block_hash const &);
	void work_update (MDB_txn *, paper::account const &, paper::block_hash const &, uint64_t);
	uint64_t work_fetch (MDB_txn *, paper::account const &, paper::block_hash const &);
	void work_ensure (MDB_txn *, paper::account const &);
	bool search_pending ();
	void init_free_accounts (MDB_txn *);
	/** Changes the wallet seed and returns the first account */
	paper::public_key change_seed (MDB_txn * transaction_a, paper::raw_key const & prv_a);
	std::unordered_set<paper::account> free_accounts;
	std::function<void(bool, bool)> lock_observer;
	paper::wallet_store store;
	paper::node & node;
};
// The wallets set is all the wallets a node controls.  A node may contain multiple wallets independently encrypted and operated.
class wallets
{
public:
	wallets (bool &, paper::node &);
	~wallets ();
	std::shared_ptr<paper::wallet> open (paper::uint256_union const &);
	std::shared_ptr<paper::wallet> create (paper::uint256_union const &);
	bool search_pending (paper::uint256_union const &);
	void search_pending_all ();
	void destroy (paper::uint256_union const &);
	void do_wallet_actions ();
	void queue_wallet_action (paper::uint128_t const &, std::function<void()> const &);
	void foreach_representative (MDB_txn *, std::function<void(paper::public_key const &, paper::raw_key const &)> const &);
	bool exists (MDB_txn *, paper::public_key const &);
	void stop ();
	std::function<void(bool)> observer;
	std::unordered_map<paper::uint256_union, std::shared_ptr<paper::wallet>> items;
	std::multimap<paper::uint128_t, std::function<void()>, std::greater<paper::uint128_t>> actions;
	std::mutex mutex;
	std::condition_variable condition;
	paper::kdf kdf;
	MDB_dbi handle;
	MDB_dbi send_action_ids;
	paper::node & node;
	bool stopped;
	std::thread thread;
	static paper::uint128_t const generate_priority;
	static paper::uint128_t const high_priority;
};
}
