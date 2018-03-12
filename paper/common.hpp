#pragma once

#include <paper/lib/blocks.hpp>
#include <paper/node/utility.hpp>

#include <boost/property_tree/ptree.hpp>

#include <unordered_map>

#include <blake2/blake2.h>

namespace boost
{
template <>
struct hash<paper::uint256_union>
{
	size_t operator() (paper::uint256_union const & value_a) const
	{
		std::hash<paper::uint256_union> hash;
		return hash (value_a);
	}
};
}
namespace paper
{
class block_store;
/**
 * Determine the Asset Key as of this block
 */
class assetKey_visitor : public paper::block_visitor
{
public:
	assetKey_visitor (MDB_txn *, paper::block_store &);
	virtual ~assetKey_visitor () = default;
	void compute (paper::block_hash const &);
	void send_block (paper::send_block const &) override;
	void receive_block (paper::receive_block const &) override;
	void open_block (paper::open_block const &) override;
	void change_block (paper::change_block const &) override;
	MDB_txn * transaction;
	paper::block_store & store;
	paper::block_hash current;
	paper::uint256_t result;
};

/**
 * Determine the amount delta resultant from this block
 */
class amount_visitor : public paper::block_visitor
{
public:
	amount_visitor (MDB_txn *, paper::block_store &);
	virtual ~amount_visitor () = default;
	void compute (paper::block_hash const &);
	void send_block (paper::send_block const &) override;
	void receive_block (paper::receive_block const &) override;
	void open_block (paper::open_block const &) override;
	void change_block (paper::change_block const &) override;
	void from_send (paper::block_hash const &);
	MDB_txn * transaction;
	paper::block_store & store;
	paper::uint256_t result;
};

/**
 * Determine the representative for this block
 */
class representative_visitor : public paper::block_visitor
{
public:
	representative_visitor (MDB_txn * transaction_a, paper::block_store & store_a);
	virtual ~representative_visitor () = default;
	void compute (paper::block_hash const & hash_a);
	void send_block (paper::send_block const & block_a) override;
	void receive_block (paper::receive_block const & block_a) override;
	void open_block (paper::open_block const & block_a) override;
	void change_block (paper::change_block const & block_a) override;
	MDB_txn * transaction;
	paper::block_store & store;
	paper::block_hash current;
	paper::block_hash result;
};

/**
 * A key pair. The private key is generated from the random pool, or passed in
 * as a hex string. The public key is derived using ed25519.
 */
class keypair
{
public:
	keypair ();
	keypair (std::string const &);
	paper::public_key pub;
	paper::raw_key prv;
};

std::unique_ptr<paper::block> deserialize_block (MDB_val const &);

/**
 * Latest information about an account
 */
class account_info
{
public:
	account_info ();
	account_info (MDB_val const &);
	account_info (paper::account_info const &) = default;
	account_info (paper::block_hash const &, paper::block_hash const &, paper::block_hash const &, paper::assetKey const &, uint64_t, uint64_t);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	bool operator== (paper::account_info const &) const;
	bool operator!= (paper::account_info const &) const;
	paper::mdb_val val () const;
	paper::block_hash head;
	paper::block_hash rep_block;
	paper::block_hash open_block;
	paper::assetKey assetKey;
	/** Seconds since posix epoch */
	uint64_t modified;
	uint64_t block_count;
};

/**
 * Information on an uncollected send, source account, amount, target account.
 */
class pending_info
{
public:
	pending_info ();
	pending_info (MDB_val const &);
	pending_info (paper::account const &, paper::assetKey const &);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	bool operator== (paper::pending_info const &) const;
	paper::mdb_val val () const;
	paper::account source;
	paper::assetKey amount;
};
class pending_key
{
public:
	pending_key (paper::account const &, paper::block_hash const &);
	pending_key (MDB_val const &);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	bool operator== (paper::pending_key const &) const;
	paper::mdb_val val () const;
	paper::account account;
	paper::block_hash hash;
};
class block_info
{
public:
	block_info ();
	block_info (MDB_val const &);
	block_info (paper::account const &, paper::assetKey const &);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	bool operator== (paper::block_info const &) const;
	paper::mdb_val val () const;
	paper::account account;
	paper::assetKey assetKey;
};
class block_counts
{
public:
	block_counts ();
	size_t sum ();
	size_t send;
	size_t receive;
	size_t open;
	size_t change;
};
class vote
{
public:
	vote () = default;
	vote (paper::vote const &);
	vote (bool &, paper::stream &);
	vote (bool &, paper::stream &, paper::block_type);
	vote (paper::account const &, paper::raw_key const &, uint64_t, std::shared_ptr<paper::block>);
	vote (MDB_val const &);
	paper::uint256_union hash () const;
	bool operator== (paper::vote const &) const;
	bool operator!= (paper::vote const &) const;
	void serialize (paper::stream &, paper::block_type);
	void serialize (paper::stream &);
	std::string to_json () const;
	// Vote round sequence number
	uint64_t sequence;
	std::shared_ptr<paper::block> block;
	// Account that's voting
	paper::account account;
	// Signature of sequence + block hash
	paper::signature signature;
};
enum class vote_code
{
	invalid, // Vote is not signed correctly
	replay, // Vote does not have the highest sequence number, it's a replay
	vote // Vote has the highest sequence number
};
class vote_result
{
public:
	paper::vote_code code;
	std::shared_ptr<paper::vote> vote;
};

enum class process_result
{
	progress, // Hasn't been seen before, signed correctly
	bad_signature, // Signature was bad, forged or transmission error
	old, // Already seen and was valid
	negative_spend, // Malicious attempt to spend a negative amount
	fork, // Malicious fork based on previous
	unreceivable, // Source block doesn't exist or has already been received
	gap_previous, // Block marked as previous is unknown
	gap_source, // Block marked as source is unknown
	not_receive_from_send, // Receive does not have a send source
	account_mismatch, // Account number in open block doesn't match send destination
	opened_burn_account // The impossible happened, someone found the private key associated with the public key '0'.
};
class process_return
{
public:
	paper::process_result code;
	paper::account account;
	paper::assetKey amount;
	paper::account pending_account;
};
enum class tally_result
{
	vote,
	changed,
	confirm
};
class votes
{
public:
	votes (std::shared_ptr<paper::block>);
	paper::tally_result vote (std::shared_ptr<paper::vote>);
	// Root block of fork
	paper::block_hash id;
	// All votes received by account
	std::unordered_map<paper::account, std::shared_ptr<paper::block>> rep_votes;
};
extern paper::keypair const & zero_key;
extern paper::keypair const & test_genesis_key;
extern paper::account const & paper_test_account;
extern paper::account const & paper_beta_account;
extern paper::account const & paper_live_account;
extern std::string const & paper_test_genesis;
extern std::string const & paper_beta_genesis;
extern std::string const & paper_live_genesis;
extern std::string const & genesis_block;
extern paper::account const & genesis_account;
extern paper::account const & burn_account;
extern paper::uint128_t const & genesis_amount;
// A block hash that compares inequal to any real block hash
extern paper::block_hash const & not_a_block;
// An account number that compares inequal to any real account number
extern paper::block_hash const & not_an_account;
class genesis
{
public:
	explicit genesis ();
	void initialize (MDB_txn *, paper::block_store &) const;
	paper::block_hash hash () const;
	std::unique_ptr<paper::open_block> open;
};
}
